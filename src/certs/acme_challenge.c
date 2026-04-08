#ifdef PHOSPHOR_HAS_LIBCURL

#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/signal.h"
#include "phosphor/json.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * find the HTTP-01 challenge in an authorization response.
 * returns the challenge object's token and URL.
 */
static ph_result_t find_http01_challenge(const char *auth_body,
                                           char **out_token,
                                           char **out_challenge_url) {
    /* look for "type":"http-01" in the challenges array */
    const char *http01 = strstr(auth_body, "\"http-01\"");
    if (!http01) return PH_ERR;

    /* find the enclosing challenge object -- scan backwards for { */
    const char *obj_start = http01;
    int depth = 0;
    while (obj_start > auth_body) {
        obj_start--;
        if (*obj_start == '{') {
            if (depth == 0) break;
            depth--;
        } else if (*obj_start == '}') {
            depth++;
        }
    }

    /* find end of this challenge object */
    const char *obj_end = http01;
    depth = 0;
    bool found_start = false;
    while (*obj_end) {
        if (*obj_end == '{') { depth++; found_start = true; }
        else if (*obj_end == '}') {
            depth--;
            if (found_start && depth == 0) { obj_end++; break; }
        }
        obj_end++;
    }

    /* extract token and url from this challenge sub-object */
    size_t obj_len = (size_t)(obj_end - obj_start);
    char *obj = ph_alloc(obj_len + 1);
    if (!obj) return PH_ERR;
    memcpy(obj, obj_start, obj_len);
    obj[obj_len] = '\0';

    ph_json_t *obj_json = ph_json_parse(obj);
    ph_free(obj);
    *out_token = ph_json_get_string(obj_json, "token");
    *out_challenge_url = ph_json_get_string(obj_json, "url");
    ph_json_destroy(obj_json);

    if (!*out_token || !*out_challenge_url) {
        ph_free(*out_token);
        ph_free(*out_challenge_url);
        *out_token = NULL;
        *out_challenge_url = NULL;
        return PH_ERR;
    }

    return PH_OK;
}

/* ---- challenge response ---- */

ph_result_t ph_acme_challenge_respond(const char *key_path,
                                        const char *account_url,
                                        const char *auth_url,
                                        const char *webroot,
                                        const char *directory_url,
                                        ph_error_t **err) {
    if (!key_path || !account_url || !auth_url || !webroot || !directory_url) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_challenge_respond: NULL argument");
        return PH_ERR;
    }

    /* step 1: GET authorization */
    char *auth_body = NULL;
    if (ph_acme_http_get(auth_url, &auth_body, NULL, err) != PH_OK) {
        return PH_ERR;
    }

    /* step 2: find HTTP-01 challenge */
    char *token = NULL;
    char *challenge_url = NULL;
    if (find_http01_challenge(auth_body, &token, &challenge_url) != PH_OK) {
        ph_free(auth_body);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "no HTTP-01 challenge found in authorization");
        return PH_ERR;
    }
    ph_free(auth_body);

    /* step 3: compute key authorization: {token}.{thumbprint} */
    char *thumbprint = NULL;
    if (ph_acme_jwk_thumbprint(key_path, &thumbprint, err) != PH_OK) {
        ph_free(token);
        ph_free(challenge_url);
        return PH_ERR;
    }

    size_t ka_len = strlen(token) + 1 + strlen(thumbprint);
    char *key_auth = ph_alloc(ka_len + 1);
    if (!key_auth) {
        ph_free(token);
        ph_free(challenge_url);
        ph_free(thumbprint);
        return PH_ERR;
    }
    snprintf(key_auth, ka_len + 1, "%s.%s", token, thumbprint);
    ph_free(thumbprint);

    /* step 4: write challenge file to webroot */
    char *challenge_dir = ph_path_join(webroot, ".well-known/acme-challenge");
    if (!challenge_dir) {
        ph_free(token);
        ph_free(challenge_url);
        ph_free(key_auth);
        return PH_ERR;
    }

    ph_fs_mkdir_p(challenge_dir, 0755);

    char *token_path = ph_path_join(challenge_dir, token);
    ph_free(challenge_dir);
    if (!token_path) {
        ph_free(token);
        ph_free(challenge_url);
        ph_free(key_auth);
        return PH_ERR;
    }

    if (ph_io_write_file(token_path, (const uint8_t *)key_auth,
                           strlen(key_auth), err) != PH_OK) {
        ph_free(token);
        ph_free(challenge_url);
        ph_free(key_auth);
        ph_free(token_path);
        return PH_ERR;
    }
    ph_free(key_auth);

    ph_log_info("certs: ACME challenge token written to %s", token_path);

    /* step 5: respond to challenge: POST {challenge_url} with {} */
    /* get fresh nonce */
    char *nonce = NULL;
    {
        char *dir_body = NULL;
        if (ph_acme_http_get(directory_url,
                &dir_body, NULL, NULL) == PH_OK) {
            ph_json_t *dir_json = ph_json_parse(dir_body);
            ph_free(dir_body);
            char *nonce_url_key = ph_json_get_string(dir_json, "newNonce");
            ph_json_destroy(dir_json);
            if (nonce_url_key) {
                ph_acme_http_head(nonce_url_key, "Replay-Nonce", &nonce, NULL);
                ph_free(nonce_url_key);
            }
        }
    }

    if (!nonce) {
        ph_free(token);
        ph_free(challenge_url);
        ph_free(token_path);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "cannot obtain ACME nonce for challenge response");
        return PH_ERR;
    }

    /* build protected header */
    size_t prot_cap = 256 + strlen(nonce) + strlen(challenge_url) +
                      strlen(account_url);
    char *protected_hdr = ph_alloc(prot_cap);
    if (!protected_hdr) {
        ph_free(nonce);
        ph_free(token);
        ph_free(challenge_url);
        ph_free(token_path);
        return PH_ERR;
    }
    snprintf(protected_hdr, prot_cap,
        "{\"alg\":\"RS256\",\"nonce\":\"%s\",\"url\":\"%s\","
        "\"kid\":\"%s\"}",
        nonce, challenge_url, account_url);
    ph_free(nonce);

    /* sign JWS with empty payload (= {}) */
    char *jws = NULL;
    if (ph_acme_jws_sign(key_path, protected_hdr, "{}", &jws,
                           err) != PH_OK) {
        ph_free(protected_hdr);
        ph_free(token);
        ph_free(challenge_url);
        ph_free(token_path);
        return PH_ERR;
    }
    ph_free(protected_hdr);

    /* POST challenge response */
    char *resp_body = NULL;
    int http_status = 0;
    if (ph_acme_http_post(challenge_url, jws, &resp_body, NULL,
                            NULL, NULL, &http_status, err) != PH_OK) {
        ph_free(jws);
        ph_free(token);
        ph_free(challenge_url);
        ph_free(token_path);
        return PH_ERR;
    }
    ph_free(jws);
    ph_free(resp_body);

    /* step 6: poll authorization until status = "valid" */
    int max_polls = 30;
    bool validated = false;
    char *last_status = NULL;
    for (int attempt = 0; attempt < max_polls; attempt++) {
        if (ph_signal_interrupted()) {
            ph_free(last_status);
            ph_free(token);
            ph_free(challenge_url);
            ph_free(token_path);
            if (err)
                *err = ph_error_createf(PH_ERR_SIGNAL, 0, "interrupted");
            return PH_ERR;
        }

        /* brief pause between polls: 2 seconds via nanosleep (audit fix:
         * replaces CPU-spinning busy-wait that pegged a core) */
        if (attempt > 0) {
            struct timespec ts = {2, 0};
            nanosleep(&ts, NULL);
        }

        char *poll_body = NULL;
        if (ph_acme_http_get(auth_url, &poll_body, NULL, NULL) != PH_OK) {
            continue;
        }

        ph_json_t *poll_json = ph_json_parse(poll_body);
        ph_free(poll_body);
        char *status = ph_json_get_string(poll_json, "status");
        ph_json_destroy(poll_json);

        if (status && strcmp(status, "valid") == 0) {
            ph_free(last_status);
            last_status = status;
            validated = true;
            ph_log_info("certs: ACME authorization validated");
            break;
        }
        if (status && strcmp(status, "invalid") == 0) {
            ph_free(last_status);
            ph_free(status);
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "ACME authorization failed (invalid)");
            ph_free(token);
            ph_free(challenge_url);
            ph_free(token_path);
            return PH_ERR;
        }
        ph_free(last_status);
        last_status = status;

        ph_log_debug("certs: polling authorization (attempt %d/%d)...",
                     attempt + 1, max_polls);
    }

    /* step 7: cleanup challenge token */
    unlink(token_path);

    /* audit fix: the old code unconditionally returned PH_OK after the poll
     * loop exited, so a timeout without ever seeing status="valid" was
     * reported as success and the caller would proceed to finalize against
     * an un-validated authorization. require validated=true. */
    if (!validated) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME authorization polling timed out after %d attempts "
                "(last status: %s)",
                max_polls, last_status ? last_status : "none");
        ph_free(last_status);
        ph_free(token);
        ph_free(challenge_url);
        ph_free(token_path);
        return PH_ERR;
    }

    ph_free(last_status);
    ph_free(token);
    ph_free(challenge_url);
    ph_free(token_path);
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBCURL */

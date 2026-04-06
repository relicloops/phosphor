#ifdef PHOSPHOR_HAS_LIBCURL

#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/proc.h"

#include "phosphor/json.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- account key management ---- */

ph_result_t ph_acme_account_ensure(const char *key_path,
                                     ph_error_t **err) {
    if (!key_path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_account_ensure: NULL key_path");
        return PH_ERR;
    }

    /* check if key already exists */
    ph_fs_stat_t st;
    if (ph_fs_stat(key_path, &st) == PH_OK && st.is_file) {
        ph_log_debug("certs: ACME account key exists: %s", key_path);
        return PH_OK;
    }

    /* create parent directory */
    char *dir = ph_path_dirname(key_path);
    if (dir) {
        ph_fs_mkdir_p(dir, 0700);
        ph_free(dir);
    }

    /* generate RSA 4096 key */
    ph_argv_builder_t b;
    if (ph_argv_init(&b, 8) != PH_OK) return PH_ERR;
    ph_argv_push(&b, "openssl");
    ph_argv_push(&b, "genrsa");
    ph_argv_push(&b, "-out");
    ph_argv_push(&b, key_path);
    ph_argv_push(&b, "4096");
    char **argv = ph_argv_finalize(&b);
    if (!argv) return PH_ERR;

    ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
    int exit_code = 0;
    ph_result_t rc = ph_proc_exec(&opts, &exit_code);
    ph_argv_free(argv);

    if (rc != PH_OK || exit_code != 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "openssl genrsa (account key) failed (exit %d)", exit_code);
        return PH_ERR;
    }

    ph_fs_chmod(key_path, 0600);
    ph_log_info("certs: ACME account key created: %s", key_path);
    return PH_OK;
}

/* ---- account registration ---- */

ph_result_t ph_acme_account_register(const char *key_path,
                                       const char *email,
                                       const char *directory_url,
                                       char **out_account_url,
                                       ph_error_t **err) {
    if (!key_path || !email || !directory_url || !out_account_url) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_account_register: NULL argument");
        return PH_ERR;
    }

    /* step 1: fetch directory */
    char *dir_body = NULL;
    if (ph_acme_http_get(directory_url, &dir_body, NULL, err) != PH_OK) {
        return PH_ERR;
    }

    ph_json_t *dir_json = ph_json_parse(dir_body);
    ph_free(dir_body);
    char *new_nonce_url = ph_json_get_string(dir_json, "newNonce");
    char *new_account_url = ph_json_get_string(dir_json, "newAccount");
    ph_json_destroy(dir_json);

    if (!new_nonce_url || !new_account_url) {
        ph_free(new_nonce_url);
        ph_free(new_account_url);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME directory missing newNonce or newAccount URL");
        return PH_ERR;
    }

    /* step 2: get nonce */
    char *nonce = NULL;
    if (ph_acme_http_head(new_nonce_url, "Replay-Nonce", &nonce,
                            err) != PH_OK) {
        ph_free(new_nonce_url);
        ph_free(new_account_url);
        return PH_ERR;
    }
    ph_free(new_nonce_url);

    /* step 3: get full JWK JSON for the protected header */
    char *jwk_json = NULL;
    if (ph_acme_jwk_json(key_path, &jwk_json, err) != PH_OK) {
        ph_free(nonce);
        ph_free(new_account_url);
        return PH_ERR;
    }

    /* step 4: build registration payload */
    size_t payload_cap = 256 + strlen(email);
    char *payload = ph_alloc(payload_cap);
    if (!payload) {
        ph_free(jwk_json);
        ph_free(nonce);
        ph_free(new_account_url);
        return PH_ERR;
    }
    snprintf(payload, payload_cap,
        "{\"termsOfServiceAgreed\":true,"
        "\"contact\":[\"mailto:%s\"]}",
        email);

    /* build protected header with full JWK (not kid, for new account) */
    size_t prot_cap = 128 + strlen(nonce) + strlen(new_account_url) +
                      strlen(jwk_json);
    char *protected_hdr = ph_alloc(prot_cap);
    if (!protected_hdr) {
        ph_free(payload);
        ph_free(jwk_json);
        ph_free(nonce);
        ph_free(new_account_url);
        return PH_ERR;
    }
    snprintf(protected_hdr, prot_cap,
        "{\"alg\":\"RS256\",\"nonce\":\"%s\",\"url\":\"%s\","
        "\"jwk\":%s}",
        nonce, new_account_url, jwk_json);
    ph_free(nonce);
    ph_free(jwk_json);

    /* sign the JWS */
    char *jws = NULL;
    if (ph_acme_jws_sign(key_path, protected_hdr, payload, &jws,
                           err) != PH_OK) {
        ph_free(protected_hdr);
        ph_free(payload);
        ph_free(new_account_url);
        return PH_ERR;
    }
    ph_free(protected_hdr);
    ph_free(payload);

    /* step 5: POST newAccount */
    char *resp_body = NULL;
    char *resp_nonce = NULL;
    char *location = NULL;
    int http_status = 0;
    if (ph_acme_http_post(new_account_url, jws, &resp_body, NULL,
                            &resp_nonce, &location,
                            &http_status, err) != PH_OK) {
        ph_free(jws);
        ph_free(new_account_url);
        return PH_ERR;
    }
    ph_free(jws);
    ph_free(new_account_url);
    ph_free(resp_nonce);

    /* 200 = existing account, 201 = new account */
    if (http_status != 200 && http_status != 201) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME newAccount returned HTTP %d: %s",
                http_status, resp_body ? resp_body : "");
        ph_free(resp_body);
        ph_free(location);
        return PH_ERR;
    }
    ph_free(resp_body);

    /* account URL comes from the Location response header */
    if (location && *location) {
        *out_account_url = location;
    } else {
        ph_free(location);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME newAccount response missing Location header");
        return PH_ERR;
    }

    ph_log_info("certs: ACME account registered (HTTP %d)", http_status);
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBCURL */

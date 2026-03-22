#ifdef PHOSPHOR_HAS_LIBCURL

#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "acme_json.h"

#include <stdio.h>
#include <string.h>

/* ---- order creation ---- */

ph_result_t ph_acme_order_create(const char *key_path,
                                   const char *account_url,
                                   const char *new_order_url,
                                   const char *directory_url,
                                   const char *const *domains,
                                   size_t domain_count,
                                   char **out_order_url,
                                   char **out_finalize_url,
                                   char ***out_auth_urls,
                                   size_t *out_auth_count,
                                   ph_error_t **err) {
    if (!key_path || !account_url || !new_order_url || !directory_url ||
        !domains || domain_count == 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_order_create: invalid arguments");
        return PH_ERR;
    }

    /* build identifiers JSON array */
    size_t ids_cap = 64;
    for (size_t i = 0; i < domain_count; i++)
        ids_cap += strlen(domains[i]) + 48;

    char *identifiers = ph_alloc(ids_cap);
    if (!identifiers) return PH_ERR;

    size_t pos = 0;
    pos += (size_t)snprintf(identifiers + pos, ids_cap - pos,
        "{\"identifiers\":[");

    for (size_t i = 0; i < domain_count; i++) {
        if (i > 0)
            pos += (size_t)snprintf(identifiers + pos, ids_cap - pos, ",");
        pos += (size_t)snprintf(identifiers + pos, ids_cap - pos,
            "{\"type\":\"dns\",\"value\":\"%s\"}", domains[i]);
    }
    pos += (size_t)snprintf(identifiers + pos, ids_cap - pos, "]}");

    /* get nonce (we need to fetch it fresh for each request) */
    char *nonce = NULL;
    char *dir_body = NULL;
    if (ph_acme_http_get(directory_url, &dir_body, NULL, err) != PH_OK) {
        ph_free(identifiers);
        return PH_ERR;
    }
    char *nonce_url = json_extract_string(dir_body, "newNonce");
    ph_free(dir_body);

    if (nonce_url) {
        ph_acme_http_head(nonce_url, "Replay-Nonce", &nonce, NULL);
        ph_free(nonce_url);
    }

    if (!nonce) {
        ph_free(identifiers);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "cannot obtain ACME nonce");
        return PH_ERR;
    }

    /* build protected header with kid */
    size_t prot_cap = 256 + strlen(nonce) + strlen(new_order_url) +
                      strlen(account_url);
    char *protected_hdr = ph_alloc(prot_cap);
    if (!protected_hdr) {
        ph_free(nonce);
        ph_free(identifiers);
        return PH_ERR;
    }
    snprintf(protected_hdr, prot_cap,
        "{\"alg\":\"RS256\",\"nonce\":\"%s\",\"url\":\"%s\","
        "\"kid\":\"%s\"}",
        nonce, new_order_url, account_url);
    ph_free(nonce);

    /* sign JWS */
    char *jws = NULL;
    if (ph_acme_jws_sign(key_path, protected_hdr, identifiers, &jws,
                           err) != PH_OK) {
        ph_free(protected_hdr);
        ph_free(identifiers);
        return PH_ERR;
    }
    ph_free(protected_hdr);
    ph_free(identifiers);

    /* POST newOrder */
    char *resp_body = NULL;
    char *location = NULL;
    int http_status = 0;
    if (ph_acme_http_post(new_order_url, jws, &resp_body, NULL,
                            NULL, &location, &http_status, err) != PH_OK) {
        ph_free(jws);
        return PH_ERR;
    }
    ph_free(jws);

    if (http_status != 201) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME newOrder returned HTTP %d: %s",
                http_status, resp_body ? resp_body : "");
        ph_free(resp_body);
        ph_free(location);
        return PH_ERR;
    }

    /* extract finalize URL and authorization URLs */
    *out_finalize_url = json_extract_string(resp_body, "finalize");
    *out_auth_urls = json_extract_string_array(resp_body, "authorizations",
                                                 out_auth_count);
    /* order URL comes from Location header */
    *out_order_url = location;

    ph_free(resp_body);

    if (!*out_finalize_url) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME order response missing finalize URL");
        return PH_ERR;
    }

    ph_log_info("certs: ACME order created (%zu authorization(s))",
                *out_auth_count);
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBCURL */

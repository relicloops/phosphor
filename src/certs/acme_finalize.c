#ifdef PHOSPHOR_HAS_LIBCURL

#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/proc.h"
#include "phosphor/signal.h"

#include "phosphor/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---- finalize ---- */

ph_result_t ph_acme_finalize(const char *key_path,
                               const char *account_url,
                               const char *finalize_url,
                               const char *order_url,
                               const char *directory_url,
                               const char *const *domains,
                               size_t domain_count,
                               const char *privkey_path,
                               const char *cert_path,
                               ph_error_t **err) {
    if (!key_path || !account_url || !finalize_url || !order_url ||
        !directory_url || !domains || domain_count == 0 ||
        !privkey_path || !cert_path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_finalize: invalid arguments");
        return PH_ERR;
    }

    /* step 1: generate private key for the certificate */
    {
        ph_argv_builder_t b;
        if (ph_argv_init(&b, 8) != PH_OK) return PH_ERR;
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "genrsa");
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, privkey_path);
        ph_argv_push(&b, "2048");
        char **argv = ph_argv_finalize(&b);
        if (!argv) return PH_ERR;

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl genrsa (cert key) failed (exit %d)", exit_code);
            return PH_ERR;
        }
    }
    ph_fs_chmod(privkey_path, 0600);

    if (ph_signal_interrupted()) return PH_ERR;

    /* step 2: generate CSR */
    char tmp_csr[] = "/tmp/ph_acme_csr_XXXXXX";
    int csr_fd = mkstemp(tmp_csr);
    if (csr_fd < 0) return PH_ERR;
    close(csr_fd);

    /* write SAN config for CSR */
    char tmp_cnf[] = "/tmp/ph_acme_cnf_XXXXXX";
    int cnf_fd = mkstemp(tmp_cnf);
    if (cnf_fd < 0) { unlink(tmp_csr); return PH_ERR; }
    close(cnf_fd);

    if (ph_cert_san_write_cnf(tmp_cnf, domains, domain_count,
                                err) != PH_OK) {
        unlink(tmp_csr);
        unlink(tmp_cnf);
        return PH_ERR;
    }

    {
        char subj[256];
        snprintf(subj, sizeof(subj), "/CN=%s", domains[0]);

        ph_argv_builder_t b;
        if (ph_argv_init(&b, 14) != PH_OK) {
            unlink(tmp_csr);
            unlink(tmp_cnf);
            return PH_ERR;
        }
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "req");
        ph_argv_push(&b, "-new");
        ph_argv_push(&b, "-key");
        ph_argv_push(&b, privkey_path);
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, tmp_csr);
        ph_argv_push(&b, "-subj");
        ph_argv_push(&b, subj);
        ph_argv_push(&b, "-config");
        ph_argv_push(&b, tmp_cnf);
        ph_argv_push(&b, "-outform");
        ph_argv_push(&b, "DER");
        char **argv = ph_argv_finalize(&b);
        if (!argv) {
            unlink(tmp_csr);
            unlink(tmp_cnf);
            return PH_ERR;
        }

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl req (CSR) failed (exit %d)", exit_code);
            unlink(tmp_csr);
            unlink(tmp_cnf);
            return PH_ERR;
        }
    }
    unlink(tmp_cnf);

    /* step 3: read CSR DER and base64url-encode */
    uint8_t *csr_data = NULL;
    size_t csr_len = 0;
    if (ph_fs_read_file(tmp_csr, &csr_data, &csr_len) != PH_OK) {
        unlink(tmp_csr);
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot read CSR from %s", tmp_csr);
        return PH_ERR;
    }
    unlink(tmp_csr);

    char *b64_csr = ph_acme_base64url_encode(csr_data, csr_len);
    ph_free(csr_data);
    if (!b64_csr) return PH_ERR;

    /* step 4: build finalize payload */
    size_t payload_cap = 32 + strlen(b64_csr);
    char *payload = ph_alloc(payload_cap);
    if (!payload) { ph_free(b64_csr); return PH_ERR; }
    snprintf(payload, payload_cap, "{\"csr\":\"%s\"}", b64_csr);
    ph_free(b64_csr);

    /* get fresh nonce */
    char *nonce = NULL;
    {
        char *dir_body = NULL;
        if (ph_acme_http_get(directory_url,
                &dir_body, NULL, NULL) == PH_OK) {
            ph_json_t *dir_json = ph_json_parse(dir_body);
            ph_free(dir_body);
            char *nonce_url = ph_json_get_string(dir_json, "newNonce");
            ph_json_destroy(dir_json);
            if (nonce_url) {
                ph_acme_http_head(nonce_url, "Replay-Nonce", &nonce, NULL);
                ph_free(nonce_url);
            }
        }
    }

    if (!nonce) {
        ph_free(payload);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "cannot obtain ACME nonce for finalize");
        return PH_ERR;
    }

    /* build protected header */
    size_t prot_cap = 256 + strlen(nonce) + strlen(finalize_url) +
                      strlen(account_url);
    char *protected_hdr = ph_alloc(prot_cap);
    if (!protected_hdr) {
        ph_free(nonce);
        ph_free(payload);
        return PH_ERR;
    }
    snprintf(protected_hdr, prot_cap,
        "{\"alg\":\"RS256\",\"nonce\":\"%s\",\"url\":\"%s\","
        "\"kid\":\"%s\"}",
        nonce, finalize_url, account_url);
    ph_free(nonce);

    /* sign JWS */
    char *jws = NULL;
    if (ph_acme_jws_sign(key_path, protected_hdr, payload, &jws,
                           err) != PH_OK) {
        ph_free(protected_hdr);
        ph_free(payload);
        return PH_ERR;
    }
    ph_free(protected_hdr);
    ph_free(payload);

    /* POST finalize */
    char *resp_body = NULL;
    int http_status = 0;
    if (ph_acme_http_post(finalize_url, jws, &resp_body, NULL,
                            NULL, NULL, &http_status, err) != PH_OK) {
        ph_free(jws);
        return PH_ERR;
    }
    ph_free(jws);

    if (http_status != 200) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME finalize returned HTTP %d: %s",
                http_status, resp_body ? resp_body : "");
        ph_free(resp_body);
        return PH_ERR;
    }

    /* step 5: poll order until status = "valid", then get certificate URL */
    char *cert_url = NULL;
    int max_polls = 30;
    for (int attempt = 0; attempt < max_polls; attempt++) {
        if (ph_signal_interrupted()) {
            ph_free(resp_body);
            return PH_ERR;
        }

        if (attempt > 0) {
            /* audit fix: nanosleep instead of CPU-spinning busy-wait */
            struct timespec ts = {2, 0};
            nanosleep(&ts, NULL);
        }

        char *order_body = NULL;
        if (attempt == 0) {
            /* first check: use the finalize response */
            order_body = resp_body;
            resp_body = NULL;
        } else {
            if (ph_acme_http_get(order_url, &order_body, NULL, NULL) != PH_OK)
                continue;
        }

        ph_json_t *order_json = ph_json_parse(order_body);
        ph_free(order_body);
        char *status = ph_json_get_string(order_json, "status");
        if (status && strcmp(status, "valid") == 0) {
            cert_url = ph_json_get_string(order_json, "certificate");
            ph_free(status);
            ph_json_destroy(order_json);
            break;
        }
        ph_free(status);
        ph_json_destroy(order_json);
    }
    ph_free(resp_body);

    if (!cert_url) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "ACME order did not become valid or missing certificate URL");
        return PH_ERR;
    }

    /* step 6: download certificate */
    char *cert_body = NULL;
    size_t cert_body_len = 0;
    if (ph_acme_http_get(cert_url, &cert_body, &cert_body_len,
                           err) != PH_OK) {
        ph_free(cert_url);
        return PH_ERR;
    }
    ph_free(cert_url);

    /* write certificate */
    if (ph_io_write_file(cert_path, (const uint8_t *)cert_body,
                           cert_body_len, err) != PH_OK) {
        ph_free(cert_body);
        return PH_ERR;
    }
    ph_free(cert_body);

    /* set permissions: cert 644, key already 600 */
    ph_fs_chmod(cert_path, 0644);

    ph_log_info("certs: certificate saved to %s", cert_path);
    ph_log_info("certs: private key saved to %s", privkey_path);
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBCURL */

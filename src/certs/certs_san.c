#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ---- SAN helpers ---- */

bool ph_cert_san_is_ip(const char *san) {
    if (!san || !*san) return false;

    /* simple heuristic: all digits and dots = IPv4 */
    bool all_digits_dots = true;
    bool has_dot = false;
    for (const char *p = san; *p; p++) {
        if (*p == '.') {
            has_dot = true;
        } else if (!isdigit((unsigned char)*p)) {
            all_digits_dots = false;
            break;
        }
    }
    if (all_digits_dots && has_dot) return true;

    /* IPv6: contains colons */
    if (strchr(san, ':')) return true;

    return false;
}

ph_result_t ph_cert_san_write_cnf(const char *cnf_path,
                                    const char *const *san,
                                    size_t san_count,
                                    ph_error_t **err) {
    if (!cnf_path || !san || san_count == 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_cert_san_write_cnf: invalid arguments");
        return PH_ERR;
    }

    /*
     * build openssl config:
     *
     * [req]
     * distinguished_name = req_distinguished_name
     * req_extensions = v3_req
     * prompt = no
     *
     * [req_distinguished_name]
     * CN = <first SAN>
     *
     * [v3_req]
     * basicConstraints = CA:FALSE
     * keyUsage = digitalSignature, keyEncipherment
     * subjectAltName = @alt_names
     *
     * [alt_names]
     * DNS.1 = example.com
     * IP.1 = 10.0.0.1
     */

    /* estimate buffer size: 512 base + 128 per SAN */
    size_t buf_cap = 512 + san_count * 128;
    char *buf = ph_alloc(buf_cap);
    if (!buf) return PH_ERR;

    int written = snprintf(buf, buf_cap,
        "[req]\n"
        "distinguished_name = req_distinguished_name\n"
        "req_extensions = v3_req\n"
        "prompt = no\n"
        "\n"
        "[req_distinguished_name]\n"
        "CN = %s\n"
        "\n"
        "[v3_req]\n"
        "basicConstraints = CA:FALSE\n"
        "keyUsage = digitalSignature, keyEncipherment\n"
        "subjectAltName = @alt_names\n"
        "\n"
        "[alt_names]\n",
        san[0]);

    if (written < 0 || (size_t)written >= buf_cap) {
        ph_free(buf);
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "SAN config buffer overflow");
        return PH_ERR;
    }

    size_t pos = (size_t)written;
    int dns_idx = 1;
    int ip_idx = 1;

    for (size_t i = 0; i < san_count; i++) {
        int n;
        if (ph_cert_san_is_ip(san[i])) {
            n = snprintf(buf + pos, buf_cap - pos,
                         "IP.%d = %s\n", ip_idx++, san[i]);
        } else {
            n = snprintf(buf + pos, buf_cap - pos,
                         "DNS.%d = %s\n", dns_idx++, san[i]);
        }
        if (n < 0 || (size_t)n >= buf_cap - pos) {
            ph_free(buf);
            if (err)
                *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                    "SAN config buffer overflow at entry %zu", i);
            return PH_ERR;
        }
        pos += (size_t)n;
    }

    /* write to file */
    ph_result_t rc = ph_io_write_file(cnf_path, (const uint8_t *)buf, pos, err);
    ph_free(buf);
    return rc;
}

/* ---- effective SAN list ---- */

/*
 * audit fix (2026-04-08T11-07-17Z): previously the cert pipeline
 * passed only domain->san to ph_acme_order_create / ph_acme_finalize
 * / ph_cert_san_write_cnf, which meant manifests that set only
 * [[certs.domains]] name = "example.com" tripped the count==0 reject
 * in every downstream helper. This helper centralizes the
 * "include the primary name at index 0" rule so every caller gets
 * a list that starts with domain->name and cannot be empty.
 */
ph_result_t ph_cert_domain_effective_sans(const ph_cert_domain_t *domain,
                                            char ***out_list,
                                            size_t *out_count,
                                            ph_error_t **err) {
    if (!domain || !domain->name || !out_list || !out_count) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_cert_domain_effective_sans: invalid arguments");
        return PH_ERR;
    }

    *out_list = NULL;
    *out_count = 0;

    size_t cap = 1 + domain->san_count;
    char **list = ph_alloc(cap * sizeof(*list));
    if (!list) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_cert_domain_effective_sans: allocation failed");
        return PH_ERR;
    }
    for (size_t i = 0; i < cap; i++) list[i] = NULL;

    /* duplicate a C string via the ph_alloc + memcpy idiom (no
     * ph_strdup exists in the tree). */
    size_t name_len = strlen(domain->name);
    list[0] = ph_alloc(name_len + 1);
    if (!list[0]) goto fail_alloc;
    memcpy(list[0], domain->name, name_len + 1);
    size_t count = 1;

    for (size_t i = 0; i < domain->san_count; i++) {
        const char *s = domain->san[i];
        if (!s) continue;
        if (strcmp(s, domain->name) == 0) continue;

        size_t slen = strlen(s);
        list[count] = ph_alloc(slen + 1);
        if (!list[count]) goto fail_alloc;
        memcpy(list[count], s, slen + 1);
        count++;
    }

    *out_list = list;
    *out_count = count;
    return PH_OK;

fail_alloc:
    ph_cert_domain_sans_free(list, cap);
    if (err)
        *err = ph_error_createf(PH_ERR_INTERNAL, 0,
            "ph_cert_domain_effective_sans: allocation failed");
    return PH_ERR;
}

void ph_cert_domain_sans_free(char **list, size_t count) {
    if (!list) return;
    for (size_t i = 0; i < count; i++)
        ph_free(list[i]);
    ph_free(list);
}

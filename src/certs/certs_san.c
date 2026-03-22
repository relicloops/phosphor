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

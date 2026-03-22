#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/platform.h"

#include "toml.h"

#include <string.h>
#include <stdlib.h>

/* ---- helpers ---- */

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = ph_alloc(len + 1);
    if (out) memcpy(out, s, len + 1);
    return out;
}

static char *toml_get_string(toml_table_t *tbl, const char *key) {
    toml_value_t v = toml_table_string(tbl, key);
    if (!v.ok) return NULL;
    char *out = dup_str(v.u.s);
    free(v.u.s);
    return out;
}

static char **toml_get_string_array(toml_array_t *arr, size_t *out_count) {
    if (!arr) { *out_count = 0; return NULL; }

    int len = toml_array_len(arr);
    if (len <= 0) { *out_count = 0; return NULL; }

    char **out = ph_calloc((size_t)len, sizeof(char *));
    if (!out) { *out_count = 0; return NULL; }

    for (int i = 0; i < len; i++) {
        toml_value_t v = toml_array_string(arr, i);
        if (v.ok) {
            out[i] = dup_str(v.u.s);
            free(v.u.s);
        }
    }
    *out_count = (size_t)len;
    return out;
}

static int toml_get_int_default(toml_table_t *tbl, const char *key, int def) {
    toml_value_t v = toml_table_int(tbl, key);
    return v.ok ? (int)v.u.i : def;
}

/* ---- parse domains ---- */

static ph_result_t parse_domains(toml_table_t *certs_tbl,
                                  ph_cert_domain_t **out_domains,
                                  size_t *out_count,
                                  ph_error_t **err) {
    toml_array_t *arr = toml_table_array(certs_tbl, "domains");
    if (!arr) {
        *out_domains = NULL;
        *out_count = 0;
        return PH_OK;
    }

    int len = toml_array_len(arr);
    if (len <= 0) {
        *out_domains = NULL;
        *out_count = 0;
        return PH_OK;
    }
    if ((size_t)len > PH_MAX_CERT_DOMAINS) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "too many cert domains (%d > %d)", len, PH_MAX_CERT_DOMAINS);
        return PH_ERR;
    }

    ph_cert_domain_t *domains = ph_calloc((size_t)len, sizeof(ph_cert_domain_t));
    if (!domains) return PH_ERR;

    for (int i = 0; i < len; i++) {
        toml_table_t *d = toml_array_table(arr, i);
        if (!d) continue;

        domains[i].name = toml_get_string(d, "name");
        if (!domains[i].name) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                    "certs.domains[%d].name is required", i);
            *out_domains = domains;
            *out_count = (size_t)len;
            return PH_ERR;
        }

        /* mode: "local" (default) or "letsencrypt" */
        char *mode_str = toml_get_string(d, "mode");
        if (mode_str && strcmp(mode_str, "letsencrypt") == 0) {
            domains[i].mode = PH_CERT_LETSENCRYPT;
        } else {
            domains[i].mode = PH_CERT_LOCAL;
        }
        ph_free(mode_str);

        /* SAN array */
        domains[i].san = toml_get_string_array(
            toml_table_array(d, "san"), &domains[i].san_count);

        /* optional overrides */
        domains[i].dir_name = toml_get_string(d, "dir_name");
        domains[i].email    = toml_get_string(d, "email");
        domains[i].webroot  = toml_get_string(d, "webroot");

        /* validation: letsencrypt requires email and webroot */
        if (domains[i].mode == PH_CERT_LETSENCRYPT) {
            if (!domains[i].email) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "certs.domains[%d] (%s): email is required "
                        "for letsencrypt mode", i, domains[i].name);
                *out_domains = domains;
                *out_count = (size_t)len;
                return PH_ERR;
            }
            if (!domains[i].webroot) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "certs.domains[%d] (%s): webroot is required "
                        "for letsencrypt mode", i, domains[i].name);
                *out_domains = domains;
                *out_count = (size_t)len;
                return PH_ERR;
            }
        }
    }

    *out_domains = domains;
    *out_count = (size_t)len;
    return PH_OK;
}

/* ---- public API ---- */

ph_result_t ph_certs_config_parse(const char *toml_path,
                                   ph_certs_config_t *config,
                                   ph_error_t **err) {
    if (!toml_path || !config) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_certs_config_parse: NULL argument");
        return PH_ERR;
    }

    memset(config, 0, sizeof(*config));

    /* read file */
    uint8_t *data = NULL;
    size_t data_len = 0;
    if (ph_fs_read_file(toml_path, &data, &data_len) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot read manifest: %s", toml_path);
        return PH_ERR;
    }

    /* parse TOML */
    char errbuf[256] = {0};
    toml_table_t *root = toml_parse((char *)data, errbuf, sizeof(errbuf));
    ph_free(data);

    if (!root) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "TOML parse error in %s: %s", toml_path, errbuf);
        return PH_ERR;
    }

    /* look for [certs] section */
    toml_table_t *certs_tbl = toml_table_table(root, "certs");
    if (!certs_tbl) {
        config->present = false;
        toml_free(root);
        return PH_OK;
    }

    config->present = true;

    /* scalar fields with defaults */
    config->output_dir = toml_get_string(certs_tbl, "output_dir");
    if (!config->output_dir)
        config->output_dir = dup_str(PH_CERTS_DEFAULT_DIR);

    config->ca_cn = toml_get_string(certs_tbl, "ca_cn");
    if (!config->ca_cn)
        config->ca_cn = dup_str(PH_CERTS_DEFAULT_CA_CN);

    config->ca_bits  = toml_get_int_default(certs_tbl, "ca_bits",
                                             PH_CERTS_DEFAULT_CA_BITS);
    config->ca_days  = toml_get_int_default(certs_tbl, "ca_days",
                                             PH_CERTS_DEFAULT_CA_DAYS);
    config->leaf_bits = toml_get_int_default(certs_tbl, "leaf_bits",
                                              PH_CERTS_DEFAULT_LEAF_BITS);
    config->leaf_days = toml_get_int_default(certs_tbl, "leaf_days",
                                              PH_CERTS_DEFAULT_LEAF_DAYS);

    config->account_key = toml_get_string(certs_tbl, "account_key");

    /* domains */
    ph_result_t rc = parse_domains(certs_tbl, &config->domains,
                                    &config->domain_count, err);
    toml_free(root);

    if (rc != PH_OK) {
        ph_certs_config_destroy(config);
        return PH_ERR;
    }

    return PH_OK;
}

static void free_string_array(char **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) ph_free(arr[i]);
    ph_free(arr);
}

void ph_certs_config_destroy(ph_certs_config_t *config) {
    if (!config) return;

    ph_free(config->output_dir);
    ph_free(config->ca_cn);
    ph_free(config->account_key);

    for (size_t i = 0; i < config->domain_count; i++) {
        ph_cert_domain_t *d = &config->domains[i];
        ph_free(d->name);
        free_string_array(d->san, d->san_count);
        ph_free(d->dir_name);
        ph_free(d->email);
        ph_free(d->webroot);
    }
    ph_free(config->domains);

    memset(config, 0, sizeof(*config));
}

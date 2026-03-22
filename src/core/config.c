#include "phosphor/config.h"
#include "phosphor/platform.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include "toml.h"

#include <string.h>
#include <stdlib.h>

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = ph_alloc(len + 1);
    if (out) memcpy(out, s, len + 1);
    return out;
}

static ph_result_t parse_config_toml(const char *path, ph_config_t *cfg,
                                      ph_error_t **err) {
    uint8_t *data = NULL;
    size_t data_len = 0;
    if (ph_fs_read_file(path, &data, &data_len) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "cannot read config: %s", path);
        return PH_ERR;
    }

    char errbuf[256] = {0};
    toml_table_t *root = toml_parse((char *)data, errbuf, sizeof(errbuf));
    ph_free(data);

    if (!root) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "TOML parse error in %s: %s",
                                     path, errbuf);
        return PH_ERR;
    }

    int len = toml_table_len(root);
    if (len <= 0) {
        toml_free(root);
        return PH_OK;
    }

    cfg->keys   = ph_calloc((size_t)len, sizeof(char *));
    cfg->values = ph_calloc((size_t)len, sizeof(char *));
    cfg->cap = (size_t)len;
    if (!cfg->keys || !cfg->values) {
        toml_free(root);
        return PH_ERR;
    }

    for (int i = 0; i < len; i++) {
        int keylen;
        const char *key = toml_table_key(root, i, &keylen);
        if (!key) continue;

        toml_value_t v = toml_table_string(root, key);
        if (v.ok) {
            cfg->keys[cfg->count]   = dup_str(key);
            cfg->values[cfg->count] = dup_str(v.u.s);
            free(v.u.s);
            cfg->count++;
        } else {
            /* try bool and int as string representations */
            toml_value_t vb = toml_table_bool(root, key);
            if (vb.ok) {
                cfg->keys[cfg->count] = dup_str(key);
                cfg->values[cfg->count] = dup_str(vb.u.b ? "true" : "false");
                cfg->count++;
                continue;
            }
            toml_value_t vi = toml_table_int(root, key);
            if (vi.ok) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)vi.u.i);
                cfg->keys[cfg->count] = dup_str(key);
                cfg->values[cfg->count] = dup_str(buf);
                cfg->count++;
            }
        }
    }

    toml_free(root);
    return PH_OK;
}

ph_result_t ph_config_discover(const char *start_dir, ph_config_t *out,
                                ph_error_t **err) {
    if (!out) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_config_discover: NULL argument");
        return PH_ERR;
    }

    memset(out, 0, sizeof(*out));

    if (!start_dir) return PH_OK;

    /* verify directory exists */
    ph_fs_stat_t dir_st;
    if (ph_fs_stat(start_dir, &dir_st) != PH_OK || !dir_st.is_dir) {
        /* directory doesn't exist -- not an error */
        return PH_OK;
    }

    char *normalized = ph_path_normalize(start_dir);
    if (!normalized) return PH_OK;

    static const char *config_names[] = {
        ".phosphor.toml",
        "phosphor.toml",
    };

    char *dir = normalized;
    if (!dir) return PH_ERR;

    while (dir) {
        for (size_t i = 0; i < sizeof(config_names) / sizeof(config_names[0]); i++) {
            char *path = ph_path_join(dir, config_names[i]);
            if (!path) continue;

            ph_fs_stat_t st;
            if (ph_fs_stat(path, &st) == PH_OK && st.exists && st.is_file) {
                ph_log_debug("config found: %s", path);
                out->file_path = path;
                ph_free(dir);

                return parse_config_toml(out->file_path, out, err);
            }
            ph_free(path);
        }

        /* walk up */
        if (strcmp(dir, "/") == 0) break;

        char *parent = ph_path_dirname(dir);
        ph_free(dir);
        dir = parent;

        if (!dir || strcmp(dir, ".") == 0) {
            ph_free(dir);
            break;
        }
    }

    return PH_OK;
}

const char *ph_config_get(const ph_config_t *cfg, const char *key) {
    if (!cfg || !key) return NULL;
    for (size_t i = 0; i < cfg->count; i++) {
        if (cfg->keys[i] && strcmp(cfg->keys[i], key) == 0)
            return cfg->values[i];
    }
    return NULL;
}

ph_result_t ph_config_set(ph_config_t *cfg, const char *key,
                           const char *value) {
    if (!cfg || !key) return PH_ERR;

    /* overwrite existing key */
    for (size_t i = 0; i < cfg->count; i++) {
        if (cfg->keys[i] && strcmp(cfg->keys[i], key) == 0) {
            ph_free(cfg->values[i]);
            cfg->values[i] = dup_str(value);
            return PH_OK;
        }
    }

    /* grow if needed */
    if (cfg->count >= cfg->cap) {
        size_t new_cap = cfg->cap ? cfg->cap * 2 : 8;
        char **nk = ph_calloc(new_cap, sizeof(char *));
        char **nv = ph_calloc(new_cap, sizeof(char *));
        if (!nk || !nv) {
            ph_free(nk);
            ph_free(nv);
            return PH_ERR;
        }
        if (cfg->count > 0) {
            memcpy(nk, cfg->keys,   cfg->count * sizeof(char *));
            memcpy(nv, cfg->values, cfg->count * sizeof(char *));
        }
        ph_free(cfg->keys);
        ph_free(cfg->values);
        cfg->keys   = nk;
        cfg->values = nv;
        cfg->cap    = new_cap;
    }

    cfg->keys[cfg->count]   = dup_str(key);
    cfg->values[cfg->count] = dup_str(value);
    cfg->count++;
    return PH_OK;
}

void ph_config_destroy(ph_config_t *cfg) {
    if (!cfg) return;
    ph_free(cfg->file_path);
    for (size_t i = 0; i < cfg->count; i++) {
        ph_free(cfg->keys[i]);
        ph_free(cfg->values[i]);
    }
    ph_free(cfg->keys);
    ph_free(cfg->values);
    memset(cfg, 0, sizeof(*cfg));
}

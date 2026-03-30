#include "phosphor/manifest.h"
#include "phosphor/platform.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include "toml.h"

#include <string.h>
#include <stdlib.h>

/* ---- helpers ---- */

const char *ph_var_type_name(ph_var_type_t type) {
    switch (type) {
    case PH_VAR_STRING: return "string";
    case PH_VAR_BOOL:   return "bool";
    case PH_VAR_INT:    return "int";
    case PH_VAR_ENUM:   return "enum";
    case PH_VAR_PATH:   return "path";
    case PH_VAR_URL:    return "url";
    }
    return "unknown";
}

ph_var_type_t ph_var_type_from_str(const char *s) {
    if (!s) return PH_VAR_STRING;
    if (strcmp(s, "string") == 0) return PH_VAR_STRING;
    if (strcmp(s, "bool")   == 0) return PH_VAR_BOOL;
    if (strcmp(s, "int")    == 0) return PH_VAR_INT;
    if (strcmp(s, "enum")   == 0) return PH_VAR_ENUM;
    if (strcmp(s, "path")   == 0) return PH_VAR_PATH;
    if (strcmp(s, "url")    == 0) return PH_VAR_URL;
    return PH_VAR_STRING;
}

const char *ph_op_kind_name(ph_op_kind_t kind) {
    switch (kind) {
    case PH_OP_MKDIR:  return "mkdir";
    case PH_OP_COPY:   return "copy";
    case PH_OP_RENDER: return "render";
    case PH_OP_CHMOD:  return "chmod";
    case PH_OP_REMOVE: return "remove";
    }
    return "unknown";
}

ph_result_t ph_op_kind_from_str(const char *s, ph_op_kind_t *out) {
    if (!s || !out) return PH_ERR;
    if (strcmp(s, "mkdir")  == 0) { *out = PH_OP_MKDIR;  return PH_OK; }
    if (strcmp(s, "copy")   == 0) { *out = PH_OP_COPY;   return PH_OK; }
    if (strcmp(s, "render") == 0) { *out = PH_OP_RENDER; return PH_OK; }
    if (strcmp(s, "chmod")  == 0) { *out = PH_OP_CHMOD;  return PH_OK; }
    if (strcmp(s, "remove") == 0) { *out = PH_OP_REMOVE; return PH_OK; }
    return PH_ERR;
}

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

/* ---- parse sections ---- */

static ph_result_t parse_manifest_meta(toml_table_t *root,
                                        ph_manifest_meta_t *out,
                                        ph_error_t **err) {
    toml_table_t *m = toml_table_table(root, "manifest");
    if (!m) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "missing required [manifest] section");
        return PH_ERR;
    }

    toml_value_t schema = toml_table_int(m, "schema");
    if (!schema.ok || schema.u.i <= 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "manifest.schema must be a positive integer");
        return PH_ERR;
    }
    out->schema = (int)schema.u.i;

    out->id = toml_get_string(m, "id");
    if (!out->id) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "manifest.id is required");
        return PH_ERR;
    }

    out->version = toml_get_string(m, "version");
    if (!out->version) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "manifest.version is required");
        return PH_ERR;
    }

    return PH_OK;
}

static ph_result_t parse_template_meta(toml_table_t *root,
                                        ph_template_meta_t *out,
                                        ph_error_t **err) {
    toml_table_t *t = toml_table_table(root, "template");
    if (!t) {
        /* template section is optional but source_root is required if present */
        return PH_OK;
    }

    out->name        = toml_get_string(t, "name");
    out->source_root = toml_get_string(t, "source_root");
    out->description = toml_get_string(t, "description");
    out->min_phosphor = toml_get_string(t, "min_phosphor");
    out->license     = toml_get_string(t, "license");

    if (!out->source_root) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "template.source_root is required");
        return PH_ERR;
    }

    return PH_OK;
}

static ph_result_t parse_defaults(toml_table_t *root,
                                   ph_defaults_t *out) {
    toml_table_t *d = toml_table_table(root, "defaults");
    if (!d) return PH_OK;

    int len = toml_table_len(d);
    if (len <= 0) return PH_OK;

    out->keys   = ph_calloc((size_t)len, sizeof(char *));
    out->values = ph_calloc((size_t)len, sizeof(char *));
    if (!out->keys || !out->values) return PH_ERR;

    for (int i = 0; i < len; i++) {
        int keylen;
        const char *key = toml_table_key(d, i, &keylen);
        if (!key) continue;

        out->keys[out->count] = dup_str(key);

        toml_value_t v = toml_table_string(d, key);
        if (v.ok) {
            out->values[out->count] = dup_str(v.u.s);
            free(v.u.s);
        }
        out->count++;
    }
    return PH_OK;
}

static ph_result_t parse_variables(toml_table_t *root,
                                    ph_var_def_t **out_vars,
                                    size_t *out_count,
                                    ph_error_t **err) {
    toml_array_t *arr = toml_table_array(root, "variables");
    if (!arr) { *out_count = 0; return PH_OK; }

    int len = toml_array_len(arr);
    if (len <= 0) { *out_count = 0; return PH_OK; }
    if ((size_t)len > PH_MAX_VARIABLES) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "too many variables (%d > %d)",
                                     len, PH_MAX_VARIABLES);
        return PH_ERR;
    }

    ph_var_def_t *vars = ph_calloc((size_t)len, sizeof(ph_var_def_t));
    if (!vars) return PH_ERR;

    for (int i = 0; i < len; i++) {
        toml_table_t *v = toml_array_table(arr, i);
        if (!v) continue;

        vars[i].name = toml_get_string(v, "name");
        if (!vars[i].name) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                         "variables[%d].name is required", i);
            *out_vars = vars;
            *out_count = (size_t)len;
            return PH_ERR;
        }

        char *type_str = toml_get_string(v, "type");
        vars[i].type = ph_var_type_from_str(type_str);
        ph_free(type_str);

        toml_value_t req = toml_table_bool(v, "required");
        vars[i].required = req.ok && req.u.b;

        vars[i].default_val = toml_get_string(v, "default");
        vars[i].env         = toml_get_string(v, "env");
        vars[i].pattern     = toml_get_string(v, "pattern");

        toml_value_t mn = toml_table_int(v, "min");
        if (mn.ok) { vars[i].min = mn.u.i; vars[i].has_min = true; }

        toml_value_t mx = toml_table_int(v, "max");
        if (mx.ok) { vars[i].max = mx.u.i; vars[i].has_max = true; }

        toml_array_t *choices_arr = toml_table_array(v, "choices");
        vars[i].choices = toml_get_string_array(choices_arr,
                                                 &vars[i].choice_count);

        toml_value_t sec = toml_table_bool(v, "secret");
        vars[i].secret = sec.ok && sec.u.b;
    }

    *out_vars = vars;
    *out_count = (size_t)len;
    return PH_OK;
}

static ph_result_t parse_filters(toml_table_t *root,
                                  ph_filters_t *out) {
    toml_table_t *f = toml_table_table(root, "filters");
    if (!f) return PH_OK;

    out->exclude    = toml_get_string_array(toml_table_array(f, "exclude"),
                                             &out->exclude_count);
    out->deny       = toml_get_string_array(toml_table_array(f, "deny"),
                                             &out->deny_count);
    out->exclude_regex = toml_get_string_array(
                            toml_table_array(f, "exclude_regex"),
                            &out->exclude_regex_count);
    out->deny_regex    = toml_get_string_array(
                            toml_table_array(f, "deny_regex"),
                            &out->deny_regex_count);
    out->binary_ext = toml_get_string_array(toml_table_array(f, "binary_ext"),
                                             &out->binary_ext_count);
    out->text_ext   = toml_get_string_array(toml_table_array(f, "text_ext"),
                                             &out->text_ext_count);
    return PH_OK;
}

static ph_result_t parse_build_config(toml_table_t *root,
                                       ph_build_config_t *out,
                                       ph_error_t **err) {
    toml_table_t *b = toml_table_table(root, "build");
    if (!b) {
        out->present = false;
        return PH_OK;
    }

    out->present = true;
    out->entry = toml_get_string(b, "entry");

    toml_array_t *arr = toml_table_array(b, "defines");
    if (!arr) { out->define_count = 0; return PH_OK; }

    int len = toml_array_len(arr);
    if (len <= 0) { out->define_count = 0; return PH_OK; }
    if ((size_t)len > PH_MAX_BUILD_DEFINES) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "too many build.defines (%d > %d)",
                                     len, PH_MAX_BUILD_DEFINES);
        return PH_ERR;
    }

    ph_build_define_t *defs = ph_calloc((size_t)len, sizeof(ph_build_define_t));
    if (!defs) return PH_ERR;

    for (int i = 0; i < len; i++) {
        toml_table_t *d = toml_array_table(arr, i);
        if (!d) continue;

        defs[i].name = toml_get_string(d, "name");
        if (!defs[i].name) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                         "build.defines[%d].name is required",
                                         i);
            out->defines = defs;
            out->define_count = (size_t)len;
            return PH_ERR;
        }

        defs[i].env = toml_get_string(d, "env");
        defs[i].default_val = toml_get_string(d, "default");
    }

    out->defines = defs;
    out->define_count = (size_t)len;
    return PH_OK;
}

static ph_result_t parse_deploy_config(toml_table_t *root,
                                        ph_deploy_config_t *out) {
    toml_table_t *d = toml_table_table(root, "deploy");
    if (!d) {
        out->present = false;
        return PH_OK;
    }

    out->present = true;
    out->public_dir = toml_get_string(d, "public_dir");
    return PH_OK;
}

static ph_result_t parse_serve_config(toml_table_t *root,
                                       ph_serve_manifest_config_t *out) {
    toml_table_t *s = toml_table_table(root, "serve");
    if (!s) {
        out->present = false;
        return PH_OK;
    }

    out->present = true;

    toml_value_t nr = toml_table_bool(s, "no_redirect");
    out->no_redirect = nr.ok && nr.u.b;

    /* [serve.neonsignal] */
    toml_table_t *ns = toml_table_table(s, "neonsignal");
    if (ns) {
        out->ns_bin       = toml_get_string(ns, "bin");
        out->ns_host      = toml_get_string(ns, "host");
        out->ns_www_root  = toml_get_string(ns, "www_root");
        out->ns_certs_root = toml_get_string(ns, "certs_root");
        out->ns_working_dir = toml_get_string(ns, "working_dir");
        out->ns_upload_dir  = toml_get_string(ns, "upload_dir");
        out->ns_augments_dir = toml_get_string(ns, "augments_dir");
        out->ns_grafts_dir  = toml_get_string(ns, "grafts_dir");

        toml_value_t th = toml_table_int(ns, "threads");
        if (th.ok) out->ns_threads = (int)th.u.i;

        toml_value_t pt = toml_table_int(ns, "port");
        if (pt.ok) out->ns_port = (int)pt.u.i;

        toml_value_t wt = toml_table_bool(ns, "watch");
        out->ns_watch = wt.ok && wt.u.b;
        out->ns_watch_cmd = toml_get_string(ns, "watch_cmd");
    }

    /* [serve.redirect] */
    toml_table_t *rd = toml_table_table(s, "redirect");
    if (rd) {
        out->redir_bin       = toml_get_string(rd, "bin");
        out->redir_host      = toml_get_string(rd, "host");
        out->redir_acme_webroot = toml_get_string(rd, "acme_webroot");
        out->redir_working_dir  = toml_get_string(rd, "working_dir");

        toml_value_t ri = toml_table_int(rd, "instances");
        if (ri.ok) out->redir_instances = (int)ri.u.i;

        toml_value_t rp = toml_table_int(rd, "port");
        if (rp.ok) out->redir_port = (int)rp.u.i;

        toml_value_t tp = toml_table_int(rd, "target_port");
        if (tp.ok) out->redir_target_port = (int)tp.u.i;
    }

    return PH_OK;
}

static ph_result_t parse_ops(toml_table_t *root,
                              ph_op_def_t **out_ops,
                              size_t *out_count,
                              ph_error_t **err) {
    toml_array_t *arr = toml_table_array(root, "ops");
    if (!arr) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "at least one [[ops]] entry is required");
        return PH_ERR;
    }

    int len = toml_array_len(arr);
    if (len <= 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "at least one [[ops]] entry is required");
        return PH_ERR;
    }
    if ((size_t)len > PH_MAX_OPS) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                     "too many ops (%d > %d)",
                                     len, PH_MAX_OPS);
        return PH_ERR;
    }

    ph_op_def_t *ops = ph_calloc((size_t)len, sizeof(ph_op_def_t));
    if (!ops) return PH_ERR;

    for (int i = 0; i < len; i++) {
        toml_table_t *o = toml_array_table(arr, i);
        if (!o) continue;

        ops[i].id = toml_get_string(o, "id");

        char *kind_str = toml_get_string(o, "kind");
        if (!kind_str) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                         "ops[%d].kind is required", i);
            ph_free(kind_str);
            *out_ops = ops;
            *out_count = (size_t)len;
            return PH_ERR;
        }
        if (ph_op_kind_from_str(kind_str, &ops[i].kind) != PH_OK) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                                         "ops[%d].kind '%s' is invalid", i,
                                         kind_str);
            ph_free(kind_str);
            *out_ops = ops;
            *out_count = (size_t)len;
            return PH_ERR;
        }
        ph_free(kind_str);

        ops[i].from = toml_get_string(o, "from");
        ops[i].to   = toml_get_string(o, "to");
        ops[i].mode = toml_get_string(o, "mode");

        toml_value_t ow = toml_table_bool(o, "overwrite");
        ops[i].overwrite = ow.ok && ow.u.b;

        ops[i].condition = toml_get_string(o, "condition");

        toml_value_t at = toml_table_bool(o, "atomic");
        ops[i].atomic = at.ok && at.u.b;

        ops[i].newline = toml_get_string(o, "newline");
    }

    *out_ops = ops;
    *out_count = (size_t)len;
    return PH_OK;
}

static ph_result_t parse_hooks(toml_table_t *root,
                                ph_hook_def_t **out_hooks,
                                size_t *out_count) {
    toml_array_t *arr = toml_table_array(root, "hooks");
    if (!arr) { *out_count = 0; return PH_OK; }

    int len = toml_array_len(arr);
    if (len <= 0) { *out_count = 0; return PH_OK; }
    if ((size_t)len > PH_MAX_HOOKS) {
        *out_count = 0;
        return PH_ERR;
    }

    ph_hook_def_t *hooks = ph_calloc((size_t)len, sizeof(ph_hook_def_t));
    if (!hooks) return PH_ERR;

    for (int i = 0; i < len; i++) {
        toml_table_t *h = toml_array_table(arr, i);
        if (!h) continue;

        hooks[i].when = toml_get_string(h, "when");

        toml_array_t *run_arr = toml_table_array(h, "run");
        hooks[i].run = toml_get_string_array(run_arr, &hooks[i].run_count);

        hooks[i].cwd       = toml_get_string(h, "cwd");
        hooks[i].condition  = toml_get_string(h, "condition");

        toml_value_t af = toml_table_bool(h, "allow_failure");
        hooks[i].allow_failure = af.ok && af.u.b;
    }

    *out_hooks = hooks;
    *out_count = (size_t)len;
    return PH_OK;
}

/* ---- public API ---- */

ph_result_t ph_manifest_load(const char *path, ph_manifest_t *out,
                              ph_error_t **err) {
    if (!path || !out) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_manifest_load: NULL argument");
        return PH_ERR;
    }

    memset(out, 0, sizeof(*out));

    /* read file */
    uint8_t *data = NULL;
    size_t data_len = 0;
    if (ph_fs_read_file(path, &data, &data_len) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot read manifest: %s", path);
        return PH_ERR;
    }

    /* parse TOML */
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

    /* parse sections */
    ph_result_t rc;

    rc = parse_manifest_meta(root, &out->manifest, err);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_template_meta(root, &out->tmpl, err);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_defaults(root, &out->defaults);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_variables(root, &out->variables, &out->variable_count, err);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_filters(root, &out->filters);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    /* regex filters require pcre2 at compile time */
    if (out->filters.exclude_regex_count > 0 ||
        out->filters.deny_regex_count > 0) {
#ifndef PHOSPHOR_HAS_PCRE2
        if (err)
            *err = ph_error_createf(PH_ERR_USAGE, 0,
                "manifest uses regex filters (exclude_regex/deny_regex) "
                "but phosphor was compiled without PCRE2 support; "
                "recompile with: meson setup build -Dpcre2=true");
        toml_free(root);
        return PH_ERR;
#endif
    }

    rc = parse_build_config(root, &out->build, err);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_deploy_config(root, &out->deploy);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_serve_config(root, &out->serve);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_ops(root, &out->ops, &out->op_count, err);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    rc = parse_hooks(root, &out->hooks, &out->hook_count);
    if (rc != PH_OK) { toml_free(root); return PH_ERR; }

    toml_free(root);

    /* semantic: check min_phosphor version */
    if (out->tmpl.min_phosphor) {
        /* simple major.minor.patch comparison */
        int req_major = 0, req_minor = 0, req_patch = 0;
        sscanf(out->tmpl.min_phosphor, "%d.%d.%d",
               &req_major, &req_minor, &req_patch);

        int cur_major = 0, cur_minor = 0, cur_patch = 0;
        sscanf(PHOSPHOR_VERSION, "%d.%d.%d",
               &cur_major, &cur_minor, &cur_patch);

        bool too_old = false;
        if (cur_major < req_major) too_old = true;
        else if (cur_major == req_major && cur_minor < req_minor) too_old = true;
        else if (cur_major == req_major && cur_minor == req_minor &&
                 cur_patch < req_patch) too_old = true;

        if (too_old) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "template requires phosphor >= %s, but current is %s",
                    out->tmpl.min_phosphor, PHOSPHOR_VERSION);
            ph_manifest_destroy(out);
            return PH_ERR;
        }
    }

    return PH_OK;
}

static void free_string_array(char **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) ph_free(arr[i]);
    ph_free(arr);
}

void ph_manifest_destroy(ph_manifest_t *m) {
    if (!m) return;

    /* manifest meta */
    ph_free(m->manifest.id);
    ph_free(m->manifest.version);

    /* template meta */
    ph_free(m->tmpl.name);
    ph_free(m->tmpl.source_root);
    ph_free(m->tmpl.description);
    ph_free(m->tmpl.min_phosphor);
    ph_free(m->tmpl.license);

    /* defaults */
    free_string_array(m->defaults.keys, m->defaults.count);
    free_string_array(m->defaults.values, m->defaults.count);

    /* variables */
    for (size_t i = 0; i < m->variable_count; i++) {
        ph_var_def_t *v = &m->variables[i];
        ph_free(v->name);
        ph_free(v->default_val);
        ph_free(v->env);
        ph_free(v->pattern);
        free_string_array(v->choices, v->choice_count);
    }
    ph_free(m->variables);

    /* filters */
    free_string_array(m->filters.exclude, m->filters.exclude_count);
    free_string_array(m->filters.deny, m->filters.deny_count);
    free_string_array(m->filters.exclude_regex, m->filters.exclude_regex_count);
    free_string_array(m->filters.deny_regex, m->filters.deny_regex_count);
    free_string_array(m->filters.binary_ext, m->filters.binary_ext_count);
    free_string_array(m->filters.text_ext, m->filters.text_ext_count);

    /* build config */
    ph_free(m->build.entry);
    for (size_t i = 0; i < m->build.define_count; i++) {
        ph_free(m->build.defines[i].name);
        ph_free(m->build.defines[i].env);
        ph_free(m->build.defines[i].default_val);
    }
    ph_free(m->build.defines);

    /* deploy config */
    ph_free(m->deploy.public_dir);

    /* serve config */
    ph_free(m->serve.ns_bin);
    ph_free(m->serve.ns_host);
    ph_free(m->serve.ns_www_root);
    ph_free(m->serve.ns_certs_root);
    ph_free(m->serve.ns_working_dir);
    ph_free(m->serve.ns_upload_dir);
    ph_free(m->serve.ns_augments_dir);
    ph_free(m->serve.ns_grafts_dir);
    ph_free(m->serve.ns_watch_cmd);
    ph_free(m->serve.redir_bin);
    ph_free(m->serve.redir_host);
    ph_free(m->serve.redir_acme_webroot);
    ph_free(m->serve.redir_working_dir);

    /* ops */
    for (size_t i = 0; i < m->op_count; i++) {
        ph_op_def_t *o = &m->ops[i];
        ph_free(o->id);
        ph_free(o->from);
        ph_free(o->to);
        ph_free(o->mode);
        ph_free(o->condition);
        ph_free(o->newline);
    }
    ph_free(m->ops);

    /* hooks */
    for (size_t i = 0; i < m->hook_count; i++) {
        ph_hook_def_t *h = &m->hooks[i];
        ph_free(h->when);
        free_string_array(h->run, h->run_count);
        ph_free(h->cwd);
        ph_free(h->condition);
    }
    ph_free(m->hooks);

    memset(m, 0, sizeof(*m));
}

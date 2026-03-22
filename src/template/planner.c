#include "phosphor/template.h"
#include "phosphor/render.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <string.h>

/* simple condition evaluator for string equality checks */
static bool eval_condition(const char *cond,
                            const ph_resolved_var_t *vars, size_t var_count) {
    if (!cond || cond[0] == '\0') return true;

    /* support: var.<name> == "value" and var.<name> != "value" */
    const char *p = cond;
    while (*p == ' ') p++;

    /* check for ! (negation prefix) */
    bool negate = false;
    if (*p == '!') {
        negate = true;
        p++;
        while (*p == ' ') p++;
    }

    /* check for var. prefix */
    if (strncmp(p, "var.", 4) == 0) {
        p += 4;
        const char *name_start = p;
        while (*p && *p != ' ' && *p != '=' && *p != '!') p++;
        size_t name_len = (size_t)(p - name_start);

        char name[256];
        if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
        memcpy(name, name_start, name_len);
        name[name_len] = '\0';

        const char *val = ph_resolved_var_get(vars, var_count, name);

        while (*p == ' ') p++;

        /* standalone var.<name> -- true if non-NULL and non-empty */
        if (*p == '\0') {
            bool result = (val != NULL && val[0] != '\0');
            return negate ? !result : result;
        }

        /* == or != */
        bool eq_op = true;
        if (p[0] == '=' && p[1] == '=') {
            eq_op = true;
            p += 2;
        } else if (p[0] == '!' && p[1] == '=') {
            eq_op = false;
            p += 2;
        } else {
            ph_log_warn("condition: unsupported operator in '%s'", cond);
            return true;
        }

        while (*p == ' ') p++;

        /* extract quoted value */
        if (*p == '"') {
            p++;
            const char *vs = p;
            while (*p && *p != '"') p++;
            size_t vlen = (size_t)(p - vs);
            char cmp_val[256];
            if (vlen >= sizeof(cmp_val)) vlen = sizeof(cmp_val) - 1;
            memcpy(cmp_val, vs, vlen);
            cmp_val[vlen] = '\0';

            bool match = val && strcmp(val, cmp_val) == 0;
            bool result = eq_op ? match : !match;
            return negate ? !result : result;
        }
    }

    /* fallback: unrecognized condition, treat as true */
    ph_log_warn("condition: cannot evaluate '%s', assuming true", cond);
    return true;
}

/* render <<var>> placeholders in a short string (paths, etc.) */
static char *render_str(const char *s,
                         const ph_resolved_var_t *vars, size_t var_count) {
    if (!s) return NULL;

    size_t len = strlen(s);
    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    if (ph_render_template((const uint8_t *)s, len,
                            vars, var_count,
                            &out, &out_len, &err) != PH_OK) {
        ph_error_destroy(err);
        return NULL;
    }

    /* null-terminate */
    uint8_t *result = ph_realloc(out, out_len + 1);
    if (!result) { ph_free(out); return NULL; }
    result[out_len] = '\0';
    return (char *)result;
}

ph_result_t ph_plan_build(const ph_manifest_t *manifest,
                           const ph_resolved_var_t *vars, size_t var_count,
                           const char *template_root,
                           const char *dest_dir,
                           ph_plan_t *out,
                           ph_error_t **err) {
    if (!manifest || !template_root || !dest_dir || !out) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_plan_build: NULL argument");
        return PH_ERR;
    }

    memset(out, 0, sizeof(*out));

    size_t count = manifest->op_count;
    if (count == 0) return PH_OK;

    out->ops = ph_calloc(count, sizeof(ph_planned_op_t));
    if (!out->ops) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0, "allocation failed");
        return PH_ERR;
    }
    out->cap = count;

    for (size_t i = 0; i < count; i++) {
        const ph_op_def_t *def = &manifest->ops[i];
        ph_planned_op_t *pop = &out->ops[i];

        pop->kind      = def->kind;
        pop->overwrite = def->overwrite;
        pop->atomic    = def->atomic;
        pop->newline   = def->newline ? render_str(def->newline, vars, var_count)
                                      : NULL;

        /* evaluate condition */
        if (def->condition) {
            pop->skip = !eval_condition(def->condition, vars, var_count);
            if (pop->skip) {
                ph_log_debug("op %zu: skipped (condition false)", i);
                out->count++;
                continue;
            }
        }

        /* resolve paths with <<var>> substitution */
        if (def->from) {
            char *rendered = render_str(def->from, vars, var_count);
            if (rendered) {
                pop->from_abs = ph_path_join(template_root, rendered);
                ph_free(rendered);
            }
        }

        if (def->to) {
            char *rendered = render_str(def->to, vars, var_count);
            if (rendered) {
                pop->to_abs = ph_path_join(dest_dir, rendered);
                ph_free(rendered);
            }
        }

        if (def->mode) {
            pop->mode = render_str(def->mode, vars, var_count);
        }

        /* detect binary for copy/render */
        if (def->kind == PH_OP_COPY || def->kind == PH_OP_RENDER) {
            if (pop->from_abs) {
                const char *ext = ph_path_extension(pop->from_abs);
                pop->is_binary = false;
                /* preliminary: will be checked at execution time too */
                if (ext) {
                    /* check manifest filter extensions */
                    for (size_t j = 0; j < manifest->filters.binary_ext_count; j++) {
                        if (manifest->filters.binary_ext[j] &&
                            strcmp(ext, manifest->filters.binary_ext[j]) == 0) {
                            pop->is_binary = true;
                            break;
                        }
                    }
                }
            }
        }

        out->count++;
    }

    return PH_OK;
}

void ph_plan_destroy(ph_plan_t *plan) {
    if (!plan) return;
    for (size_t i = 0; i < plan->count; i++) {
        ph_free(plan->ops[i].from_abs);
        ph_free(plan->ops[i].to_abs);
        ph_free(plan->ops[i].mode);
        ph_free(plan->ops[i].newline);
    }
    ph_free(plan->ops);
    memset(plan, 0, sizeof(*plan));
}

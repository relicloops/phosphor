#include "phosphor/template.h"
#include "phosphor/render.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <string.h>

/* condition evaluator for string equality checks.
 *
 * audit fix: the previous implementation was fail-open -- any unrecognized
 * operator or malformed expression returned true, which silently executed
 * guarded ops on a manifest typo. This version is fail-closed: it returns
 * PH_ERR with a descriptive error on anything it cannot parse, so the
 * plan-build step aborts instead of falling through.
 *
 * supported grammar:
 *   empty string         -> true
 *   var.<name>           -> non-NULL and non-empty
 *   ! var.<name>         -> negation
 *   var.<name> == "val"  -> equality
 *   var.<name> != "val"  -> inequality
 *   ! var.<name> == "val" -> negated equality (and so on)
 */
static ph_result_t eval_condition(const char *cond,
                                   const ph_resolved_var_t *vars,
                                   size_t var_count,
                                   bool *out_value,
                                   ph_error_t **err) {
    if (!out_value) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "eval_condition: NULL out_value");
        return PH_ERR;
    }

    if (!cond || cond[0] == '\0') {
        *out_value = true;
        return PH_OK;
    }

    const char *p = cond;
    while (*p == ' ') p++;

    /* check for ! (negation prefix) */
    bool negate = false;
    if (*p == '!' && p[1] != '=') {
        negate = true;
        p++;
        while (*p == ' ') p++;
    }

    /* only var.<name> atoms are supported */
    if (strncmp(p, "var.", 4) != 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "condition: expected 'var.<name>', got: '%s'", cond);
        return PH_ERR;
    }

    p += 4;
    const char *name_start = p;
    while (*p && *p != ' ' && *p != '=' && *p != '!') p++;
    size_t name_len = (size_t)(p - name_start);

    if (name_len == 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "condition: empty variable name in: '%s'", cond);
        return PH_ERR;
    }

    char name[256];
    if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
    memcpy(name, name_start, name_len);
    name[name_len] = '\0';

    const char *val = ph_resolved_var_get(vars, var_count, name);

    while (*p == ' ') p++;

    /* standalone var.<name> -- true if non-NULL and non-empty */
    if (*p == '\0') {
        bool result = (val != NULL && val[0] != '\0');
        *out_value = negate ? !result : result;
        return PH_OK;
    }

    /* == or != */
    bool eq_op;
    if (p[0] == '=' && p[1] == '=') {
        eq_op = true;
        p += 2;
    } else if (p[0] == '!' && p[1] == '=') {
        eq_op = false;
        p += 2;
    } else {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "condition: unsupported operator in: '%s' "
                "(expected '==' or '!=')", cond);
        return PH_ERR;
    }

    while (*p == ' ') p++;

    /* extract quoted value */
    if (*p != '"') {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "condition: expected quoted value in: '%s'", cond);
        return PH_ERR;
    }

    p++;
    const char *vs = p;
    while (*p && *p != '"') p++;
    if (*p != '"') {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "condition: unterminated quoted value in: '%s'", cond);
        return PH_ERR;
    }
    size_t vlen = (size_t)(p - vs);
    char cmp_val[256];
    if (vlen >= sizeof(cmp_val)) vlen = sizeof(cmp_val) - 1;
    memcpy(cmp_val, vs, vlen);
    cmp_val[vlen] = '\0';

    bool match = val && strcmp(val, cmp_val) == 0;
    bool result = eq_op ? match : !match;
    *out_value = negate ? !result : result;
    return PH_OK;
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

    /* store canonical containment roots for belt-and-suspenders
     * re-verification in ph_plan_execute. */
    size_t tr_len = strlen(template_root);
    size_t dd_len = strlen(dest_dir);
    out->template_root = ph_alloc(tr_len + 1);
    out->dest_dir      = ph_alloc(dd_len + 1);
    if (!out->template_root || !out->dest_dir) {
        ph_free(out->template_root);
        ph_free(out->dest_dir);
        out->template_root = NULL;
        out->dest_dir = NULL;
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0, "allocation failed");
        return PH_ERR;
    }
    memcpy(out->template_root, template_root, tr_len + 1);
    memcpy(out->dest_dir, dest_dir, dd_len + 1);

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

        /* evaluate condition -- audit fix: fail-closed.
         * a malformed or unsupported condition is now a hard error rather
         * than implicitly evaluating to true. the error already carries
         * the offending condition string. */
        if (def->condition) {
            bool cond_value = false;
            if (eval_condition(def->condition, vars, var_count,
                                &cond_value, err) != PH_OK) {
                ph_log_error("plan: op %zu has invalid condition", i);
                return PH_ERR;
            }
            pop->skip = !cond_value;
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
                /* audit fix: reject traversal injected via variable substitution */
                if (ph_path_is_absolute(rendered) ||
                    ph_path_has_traversal(rendered)) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "op %zu: rendered 'from' is unsafe: %s", i,
                            rendered);
                    ph_free(rendered);
                    return PH_ERR;
                }
                pop->from_abs = ph_path_join(template_root, rendered);
                /* audit fix (2026-04-07T20-37-55Z): retain manifest-relative
                 * `from` so ph_plan_execute can apply the full filter rules
                 * (metadata, exclude, deny) consistently for both single-file
                 * and directory ops. ownership transfers to pop. */
                pop->from_rel = rendered;
            }
        }

        if (def->to) {
            char *rendered = render_str(def->to, vars, var_count);
            if (rendered) {
                /* audit fix: reject traversal injected via variable substitution */
                if (ph_path_is_absolute(rendered) ||
                    ph_path_has_traversal(rendered)) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "op %zu: rendered 'to' is unsafe: %s", i,
                            rendered);
                    ph_free(rendered);
                    return PH_ERR;
                }
                pop->to_abs = ph_path_join(dest_dir, rendered);
                ph_free(rendered);
            }
        }

        /* audit fix: post-join canonical containment check.
         * from_abs must stay under template_root, to_abs under dest_dir.
         * ph_path_is_under fully canonicalizes both paths to catch symlink
         * escapes and any traversal that survived parse+render gates. */
        if (pop->from_abs && !ph_path_is_under(pop->from_abs, template_root)) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "op %zu: resolved 'from' escapes template root: %s",
                    i, pop->from_abs);
            return PH_ERR;
        }
        if (pop->to_abs && !ph_path_is_under(pop->to_abs, dest_dir)) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "op %zu: resolved 'to' escapes destination: %s",
                    i, pop->to_abs);
            return PH_ERR;
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
        ph_free(plan->ops[i].from_rel);
        ph_free(plan->ops[i].to_abs);
        ph_free(plan->ops[i].mode);
        ph_free(plan->ops[i].newline);
    }
    ph_free(plan->ops);
    ph_free(plan->template_root);
    ph_free(plan->dest_dir);
    memset(plan, 0, sizeof(*plan));
}

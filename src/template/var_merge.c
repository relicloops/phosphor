#include "phosphor/template.h"
#include "phosphor/args.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <string.h>
#include <stdlib.h>
#include <regex.h>

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = ph_alloc(len + 1);
    if (out) memcpy(out, s, len + 1);
    return out;
}

static ph_result_t validate_var(const ph_var_def_t *def,
                                 const char *value,
                                 ph_error_t **err) {
    if (!value) return PH_OK;

    /* enum validation */
    if (def->type == PH_VAR_ENUM && def->choices && def->choice_count > 0) {
        bool found = false;
        for (size_t i = 0; i < def->choice_count; i++) {
            if (def->choices[i] && strcmp(value, def->choices[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': value '%s' is not a valid choice",
                    def->name, value);
            return PH_ERR;
        }
    }

    /* int bounds */
    if (def->type == PH_VAR_INT) {
        char *end;
        long long val = strtoll(value, &end, 10);
        if (*end != '\0') {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': '%s' is not a valid integer",
                    def->name, value);
            return PH_ERR;
        }
        if (def->has_min && val < def->min) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': value %lld < min %lld",
                    def->name, val, (long long)def->min);
            return PH_ERR;
        }
        if (def->has_max && val > def->max) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': value %lld > max %lld",
                    def->name, val, (long long)def->max);
            return PH_ERR;
        }
    }

    /* URL scheme check */
    if (def->type == PH_VAR_URL) {
        if (strncmp(value, "http://", 7) != 0 &&
            strncmp(value, "https://", 8) != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': URL must start with http:// or https://",
                    def->name);
            return PH_ERR;
        }
    }

    /* path traversal check */
    if (def->type == PH_VAR_PATH) {
        if (ph_path_has_traversal(value)) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': path contains '..' traversal",
                    def->name);
            return PH_ERR;
        }
    }

    /* pattern (POSIX ERE) */
    if (def->pattern) {
        regex_t re;
        int rc = regcomp(&re, def->pattern, REG_EXTENDED | REG_NOSUB);
        if (rc != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': invalid regex pattern '%s'",
                    def->name, def->pattern);
            return PH_ERR;
        }
        int match = regexec(&re, value, 0, NULL, 0);
        regfree(&re);
        if (match != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "variable '%s': value '%s' does not match pattern '%s'",
                    def->name, value, def->pattern);
            return PH_ERR;
        }
    }

    return PH_OK;
}

ph_result_t ph_var_merge(const ph_var_merge_ctx_t *ctx,
                          ph_resolved_var_t **out_vars,
                          size_t *out_count,
                          ph_error_t **err) {
    if (!ctx || !ctx->manifest || !out_vars || !out_count) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_var_merge: NULL argument");
        return PH_ERR;
    }

    size_t count = ctx->manifest->variable_count;
    if (count == 0) {
        *out_vars = NULL;
        *out_count = 0;
        return PH_OK;
    }

    ph_resolved_var_t *vars = ph_calloc(count, sizeof(ph_resolved_var_t));
    if (!vars) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0, "allocation failed");
        return PH_ERR;
    }

    for (size_t i = 0; i < count; i++) {
        const ph_var_def_t *def = &ctx->manifest->variables[i];
        vars[i].name = dup_str(def->name);
        vars[i].type = def->type;

        const char *value = NULL;

        /* precedence 1: CLI flags (with argspec var_name mapping) */
        if (ctx->args) {
            const char *lookup_name = def->name;
            if (ctx->cli_config) {
                size_t sc = 0;
                const ph_argspec_t *specs =
                    ph_cmd_def_specs(ctx->cli_config, ctx->args->command_id, &sc);
                for (size_t s = 0; specs && s < sc; s++) {
                    if (specs[s].var_name &&
                        strcmp(specs[s].var_name, def->name) == 0) {
                        lookup_name = specs[s].name;
                        break;
                    }
                }
            }
            value = ph_args_get_flag(ctx->args, lookup_name);
        }

        /* precedence 2: env vars */
        if (!value && def->env) {
            value = getenv(def->env);
        }

        /* precedence 3: project config */
        if (!value && ctx->config) {
            value = ph_config_get(ctx->config, def->name);
        }

        /* precedence 4: manifest defaults */
        if (!value && def->default_val) {
            value = def->default_val;
        }

        /* required check */
        if (!value && def->required) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "required variable '%s' has no value", def->name);
            ph_resolved_vars_destroy(vars, count);
            return PH_ERR;
        }

        if (value) {
            vars[i].value = dup_str(value);

            /* validate constraints */
            ph_result_t rc = validate_var(def, vars[i].value, err);
            if (rc != PH_OK) {
                ph_resolved_vars_destroy(vars, count);
                return PH_ERR;
            }
        }

        if (def->secret && value) {
            ph_log_debug("var '%s' = [secret]", def->name);
        } else if (value) {
            ph_log_debug("var '%s' = '%s'", def->name, value);
        }
    }

    *out_vars = vars;
    *out_count = count;
    return PH_OK;
}

void ph_resolved_vars_destroy(ph_resolved_var_t *vars, size_t count) {
    if (!vars) return;
    for (size_t i = 0; i < count; i++) {
        ph_free(vars[i].name);
        ph_free(vars[i].value);
    }
    ph_free(vars);
}

const char *ph_resolved_var_get(const ph_resolved_var_t *vars, size_t count,
                                 const char *name) {
    if (!vars || !name) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (vars[i].name && strcmp(vars[i].name, name) == 0)
            return vars[i].value;
    }
    return NULL;
}

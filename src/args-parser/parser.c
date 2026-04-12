#include "phosphor/args.h"
#include "phosphor/alloc.h"

#include <string.h>

/* ---- internal helpers ---- */

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = ph_alloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

static int match_command(const ph_cli_config_t *config, const char *name) {
    for (size_t i = 0; i < config->command_count; i++) {
        if (strcmp(config->commands[i].name, name) == 0) {
            return config->commands[i].id;
        }
    }
    return 0;
}

static const ph_cmd_def_t *find_cmd_def(const ph_cli_config_t *config,
                                         int command_id) {
    for (size_t i = 0; i < config->command_count; i++) {
        if (config->commands[i].id == command_id) {
            return &config->commands[i];
        }
    }
    return NULL;
}

static ph_result_t flags_push(ph_parsed_args_t *a,
                               const ph_parsed_flag_t *f) {
    if (a->flag_count >= a->flag_cap) {
        size_t new_cap = a->flag_cap ? a->flag_cap * 2 : 8;
        ph_parsed_flag_t *buf = ph_realloc(a->flags,
                                            new_cap * sizeof(ph_parsed_flag_t));
        if (!buf) return PH_ERR;
        a->flags = buf;
        a->flag_cap = new_cap;
    }
    a->flags[a->flag_count++] = *f;
    return PH_OK;
}

/*
 * Check whether a valued or bool flag with the given name exists.
 */
static bool has_flag(const ph_parsed_args_t *a, const char *name) {
    for (size_t i = 0; i < a->flag_count; i++) {
        if ((a->flags[i].kind == PH_FLAG_VALUED ||
             a->flags[i].kind == PH_FLAG_BOOL) &&
            strcmp(a->flags[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_enable(const ph_parsed_args_t *a, const char *name) {
    for (size_t i = 0; i < a->flag_count; i++) {
        if (a->flags[i].kind == PH_FLAG_ENABLE &&
            strcmp(a->flags[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_disable(const ph_parsed_args_t *a, const char *name) {
    for (size_t i = 0; i < a->flag_count; i++) {
        if (a->flags[i].kind == PH_FLAG_DISABLE &&
            strcmp(a->flags[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

/* ---- public API ---- */

ph_result_t ph_parser_parse(const ph_cli_config_t *config,
                             const ph_token_stream_t *tokens,
                             ph_parsed_args_t *out,
                             ph_error_t **err) {
    if (!config || !tokens || !out || !err) return PH_ERR;

    *out = (ph_parsed_args_t){0};
    *err = NULL;

    if (!tokens->tokens || tokens->count == 0) {
        *err = ph_error_create(PH_ERR_USAGE, 0, "no command specified");
        return PH_ERR;
    }

    size_t pos = 0;
    const ph_arg_token_t *tok = &tokens->tokens[pos];

    /* expect first token to be a positional (command name) */
    if (tok->kind == PH_TOK_END) {
        *err = ph_error_createf(PH_ERR_USAGE, 0,
            "no command specified (try: %s help)", config->tool_name);
        return PH_ERR;
    }

    if (tok->kind != PH_TOK_POSITIONAL) {
        *err = ph_error_createf(PH_ERR_USAGE, 0,
            "expected command name, got --%s (try: %s help)",
            tok->name ? tok->name : "?", config->tool_name);
        return PH_ERR;
    }

    out->command_id = match_command(config, tok->name);
    if (out->command_id == 0) {
        *err = ph_error_createf(PH_ERR_USAGE, 0,
            "unknown command: %s (try: %s help)",
            tok->name, config->tool_name);
        return PH_ERR;
    }

    const ph_cmd_def_t *cmd_def = find_cmd_def(config, out->command_id);
    pos++;

    /* parse remaining tokens as flags */
    while (pos < tokens->count) {
        tok = &tokens->tokens[pos];
        if (tok->kind == PH_TOK_END) break;

        ph_parsed_flag_t flag = { .argv_index = tok->argv_index };

        switch (tok->kind) {

        case PH_TOK_VALUED_FLAG: {
            if (has_flag(out, tok->name)) {
                *err = ph_error_createf(PH_ERR_USAGE,
                    PH_UX003_DUPLICATE_FLAG,
                    "duplicate flag: --%s at argv[%d]",
                    tok->name, tok->argv_index);
                ph_parsed_args_destroy(out);
                return PH_ERR;
            }
            flag.kind  = PH_FLAG_VALUED;
            flag.name  = dup_str(tok->name);
            flag.value = dup_str(tok->value);
            if (!flag.name || (!flag.value && tok->value)) {
                ph_free(flag.name);
                ph_free(flag.value);
                goto alloc_fail;
            }
            break;
        }

        case PH_TOK_BOOL_FLAG: {
            if (has_flag(out, tok->name)) {
                *err = ph_error_createf(PH_ERR_USAGE,
                    PH_UX003_DUPLICATE_FLAG,
                    "duplicate flag: --%s at argv[%d]",
                    tok->name, tok->argv_index);
                ph_parsed_args_destroy(out);
                return PH_ERR;
            }
            flag.kind  = PH_FLAG_BOOL;
            flag.name  = dup_str(tok->name);
            flag.value = NULL;
            if (!flag.name) goto alloc_fail;
            break;
        }

        case PH_TOK_ENABLE_FLAG: {
            if (has_disable(out, tok->name)) {
                *err = ph_error_createf(PH_ERR_USAGE,
                    PH_UX004_POLARITY_CONFLICT,
                    "conflicting flags: --enable-%s and "
                    "--disable-%s at argv[%d]",
                    tok->name, tok->name, tok->argv_index);
                ph_parsed_args_destroy(out);
                return PH_ERR;
            }
            if (has_enable(out, tok->name)) {
                *err = ph_error_createf(PH_ERR_USAGE,
                    PH_UX003_DUPLICATE_FLAG,
                    "duplicate flag: --enable-%s at argv[%d]",
                    tok->name, tok->argv_index);
                ph_parsed_args_destroy(out);
                return PH_ERR;
            }
            flag.kind  = PH_FLAG_ENABLE;
            flag.name  = dup_str(tok->name);
            flag.value = NULL;
            if (!flag.name) goto alloc_fail;
            break;
        }

        case PH_TOK_DISABLE_FLAG: {
            if (has_enable(out, tok->name)) {
                *err = ph_error_createf(PH_ERR_USAGE,
                    PH_UX004_POLARITY_CONFLICT,
                    "conflicting flags: --enable-%s and "
                    "--disable-%s at argv[%d]",
                    tok->name, tok->name, tok->argv_index);
                ph_parsed_args_destroy(out);
                return PH_ERR;
            }
            if (has_disable(out, tok->name)) {
                *err = ph_error_createf(PH_ERR_USAGE,
                    PH_UX003_DUPLICATE_FLAG,
                    "duplicate flag: --disable-%s at argv[%d]",
                    tok->name, tok->argv_index);
                ph_parsed_args_destroy(out);
                return PH_ERR;
            }
            flag.kind  = PH_FLAG_DISABLE;
            flag.name  = dup_str(tok->name);
            flag.value = NULL;
            if (!flag.name) goto alloc_fail;
            break;
        }

        case PH_TOK_POSITIONAL: {
            if (cmd_def && cmd_def->accepts_positional && !out->positional) {
                out->positional = dup_str(tok->name);
                if (!out->positional) goto alloc_fail;
                pos++;
                continue;
            }
            *err = ph_error_createf(PH_ERR_USAGE, 0,
                "unexpected argument: %s at argv[%d]",
                tok->name, tok->argv_index);
            ph_parsed_args_destroy(out);
            return PH_ERR;
        }

        default:
            break;
        }

        if (flags_push(out, &flag) != PH_OK) {
            ph_free(flag.name);
            ph_free(flag.value);
            goto alloc_fail;
        }

        pos++;
    }

    return PH_OK;

alloc_fail:
    *err = ph_error_create(PH_ERR_INTERNAL, 0, "allocation failed in parser");
    ph_parsed_args_destroy(out);
    return PH_ERR;
}

ph_result_t ph_args_apply_defaults(const ph_cli_config_t *config,
                                   ph_parsed_args_t *args,
                                   ph_error_t **err) {
    if (!config || !args || !err) return PH_ERR;
    *err = NULL;

    size_t spec_count = 0;
    const ph_argspec_t *specs =
        ph_cmd_def_specs(config, args->command_id, &spec_count);
    if (!specs) return PH_OK;

    for (size_t i = 0; i < spec_count; i++) {
        if (!specs[i].default_value)          continue;
        if (specs[i].form != PH_FORM_VALUED)  continue;
        if (ph_args_has_flag(args, specs[i].name)) continue;

        ph_parsed_flag_t f = {
            .kind       = PH_FLAG_VALUED,
            .name       = dup_str(specs[i].name),
            .value      = dup_str(specs[i].default_value),
            .argv_index = -1,
        };
        if (!f.name || !f.value) {
            ph_free(f.name);
            ph_free(f.value);
            *err = ph_error_create(PH_ERR_INTERNAL, 0,
                                   "allocation failed applying defaults");
            return PH_ERR;
        }
        if (flags_push(args, &f) != PH_OK) {
            ph_free(f.name);
            ph_free(f.value);
            *err = ph_error_create(PH_ERR_INTERNAL, 0,
                                   "allocation failed applying defaults");
            return PH_ERR;
        }
    }
    return PH_OK;
}

void ph_parsed_args_destroy(ph_parsed_args_t *args) {
    if (!args) return;
    for (size_t i = 0; i < args->flag_count; i++) {
        ph_free(args->flags[i].name);
        ph_free(args->flags[i].value);
    }
    ph_free(args->flags);
    ph_free(args->positional);
    *args = (ph_parsed_args_t){0};
}

#include "phosphor/args.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

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

static char *dup_nstr(const char *s, size_t n) {
    char *copy = ph_alloc(n + 1);
    if (!copy) return NULL;
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

static ph_result_t stream_push(ph_token_stream_t *s,
                                const ph_arg_token_t *tok) {
    if (s->count >= s->cap) {
        size_t new_cap = s->cap ? s->cap * 2 : 8;
        ph_arg_token_t *buf = ph_realloc(s->tokens,
                                          new_cap * sizeof(ph_arg_token_t));
        if (!buf) return PH_ERR;
        s->tokens = buf;
        s->cap = new_cap;
    }
    s->tokens[s->count++] = *tok;
    return PH_OK;
}

/*
 * Validate identifier per EBNF: letter { letter | digit | "-" | "_" }
 */
static bool is_valid_ident(const char *s) {
    if (!s || !*s) return false;
    char c = *s;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    for (const char *p = s + 1; *p; p++) {
        c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

/* ---- public API ---- */

ph_result_t ph_lexer_tokenize(int argc, const char *const *argv,
                               ph_token_stream_t *out,
                               ph_error_t **err) {
    if (!argv || !out || !err) return PH_ERR;

    *out = (ph_token_stream_t){0};
    *err = NULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!arg) continue;

        ph_arg_token_t tok = { .argv_index = i };

        if (arg[0] == '-' && arg[1] == '-') {
            /* long flag family (starts with "--") */
            const char *rest = arg + 2;

            if (*rest == '\0') {
                *err = ph_error_createf(PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
                    "bare -- is not a valid argument at argv[%d]", i);
                ph_token_stream_destroy(out);
                return PH_ERR;
            }

            const char *eq = strchr(rest, '=');

            if (eq) {
                /* --IDENT=VALUE  (valued flag) */
                if (eq == rest) {
                    *err = ph_error_createf(PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
                        "empty flag name at argv[%d]: %s", i, arg);
                    ph_token_stream_destroy(out);
                    return PH_ERR;
                }
                size_t name_len = (size_t)(eq - rest);
                char *name = dup_nstr(rest, name_len);
                if (!name) goto alloc_fail;

                if (!is_valid_ident(name)) {
                    *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                        "invalid flag name at argv[%d]: --%s", i, name);
                    ph_free(name);
                    ph_token_stream_destroy(out);
                    return PH_ERR;
                }

                tok.kind  = PH_TOK_VALUED_FLAG;
                tok.name  = name;
                tok.value = dup_str(eq + 1);
                if (!tok.value) {
                    ph_free(tok.name);
                    goto alloc_fail;
                }
            }
            else if (strncmp(rest, "enable-", 7) == 0) {
                /* --enable-IDENT  (polarity toggle on) */
                const char *ident = rest + 7;
                if (*ident == '\0') {
                    *err = ph_error_createf(PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
                        "empty flag name after --enable- at argv[%d]", i);
                    ph_token_stream_destroy(out);
                    return PH_ERR;
                }
                if (!is_valid_ident(ident)) {
                    *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                        "invalid flag name at argv[%d]: --enable-%s", i, ident);
                    ph_token_stream_destroy(out);
                    return PH_ERR;
                }
                tok.kind  = PH_TOK_ENABLE_FLAG;
                tok.name  = dup_str(ident);
                tok.value = NULL;
                if (!tok.name) goto alloc_fail;
            }
            else if (strncmp(rest, "disable-", 8) == 0) {
                /* --disable-IDENT  (polarity toggle off) */
                const char *ident = rest + 8;
                if (*ident == '\0') {
                    *err = ph_error_createf(PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
                        "empty flag name after --disable- at argv[%d]", i);
                    ph_token_stream_destroy(out);
                    return PH_ERR;
                }
                if (!is_valid_ident(ident)) {
                    *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                        "invalid flag name at argv[%d]: --disable-%s", i, ident);
                    ph_token_stream_destroy(out);
                    return PH_ERR;
                }
                tok.kind  = PH_TOK_DISABLE_FLAG;
                tok.name  = dup_str(ident);
                tok.value = NULL;
                if (!tok.name) goto alloc_fail;
            }
            else {
                /* --IDENT  (bare boolean flag) */
                if (!is_valid_ident(rest)) {
                    *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                        "invalid flag name at argv[%d]: --%s", i, rest);
                    ph_token_stream_destroy(out);
                    return PH_ERR;
                }
                tok.kind  = PH_TOK_BOOL_FLAG;
                tok.name  = dup_str(rest);
                tok.value = NULL;
                if (!tok.name) goto alloc_fail;
            }
        }
        else if (arg[0] == '-' && arg[1] != '\0') {
            /* single-dash flag -- not supported in v1 (long flags only) */
            *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                "short flags are not supported at argv[%d]: %s "
                "(use long form: --flag)", i, arg);
            ph_token_stream_destroy(out);
            return PH_ERR;
        }
        else {
            /* bare word (command name or positional) */
            tok.kind  = PH_TOK_POSITIONAL;
            tok.name  = dup_str(arg);
            tok.value = NULL;
            if (!tok.name) goto alloc_fail;
        }

        if (stream_push(out, &tok) != PH_OK) {
            ph_free(tok.name);
            ph_free(tok.value);
            goto alloc_fail;
        }
    }

    /* append END sentinel */
    ph_arg_token_t end = { .kind = PH_TOK_END, .name = NULL,
                           .value = NULL,       .argv_index = -1 };
    if (stream_push(out, &end) != PH_OK) goto alloc_fail;

    return PH_OK;

alloc_fail:
    *err = ph_error_create(PH_ERR_INTERNAL, 0, "allocation failed in lexer");
    ph_token_stream_destroy(out);
    return PH_ERR;
}

void ph_token_stream_destroy(ph_token_stream_t *stream) {
    if (!stream) return;
    for (size_t i = 0; i < stream->count; i++) {
        ph_free(stream->tokens[i].name);
        ph_free(stream->tokens[i].value);
    }
    ph_free(stream->tokens);
    *stream = (ph_token_stream_t){0};
}

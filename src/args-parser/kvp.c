#include "phosphor/args.h"
#include "phosphor/alloc.h"

#include <string.h>

/* ---- parser state ---- */

typedef struct {
    const char *start;      /* original input (for position calc) */
    const char *cur;        /* current cursor position */
    int         depth;      /* current nesting depth */
} kvp_ctx_t;

/* ---- internal helpers ---- */

static int pos_of(const kvp_ctx_t *ctx) {
    return (int)(ctx->cur - ctx->start);
}

static char *dup_nstr(const char *s, size_t n) {
    char *copy = ph_alloc(n + 1);
    if (!copy) return NULL;
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_ident_start(char c) {
    return is_alpha(c);
}

static bool is_ident_cont(char c) {
    return is_alpha(c) || is_digit(c) || c == '-' || c == '_';
}

static bool is_delimiter(char c) {
    return c == '|' || c == '}' || c == '\0';
}

/* ---- free a single stack-allocated node's heap fields ---- */

static void kvp_node_free_fields(ph_kvp_node_t *node) {
    ph_free(node->key);
    if (node->is_object) {
        ph_kvp_destroy(node->children, node->child_count);
    } else {
        ph_free(node->value);
    }
}

/* ---- grow-on-demand node array ---- */

typedef struct {
    ph_kvp_node_t *items;
    size_t          count;
    size_t          cap;
} node_vec_t;

static ph_result_t node_vec_push(node_vec_t *v, const ph_kvp_node_t *node) {
    if (v->count >= v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 4;
        ph_kvp_node_t *buf = ph_realloc(v->items,
                                         new_cap * sizeof(ph_kvp_node_t));
        if (!buf) return PH_ERR;
        v->items = buf;
        v->cap = new_cap;
    }
    v->items[v->count++] = *node;
    return PH_OK;
}

/* ---- forward declarations ---- */

static ph_result_t parse_pairs(kvp_ctx_t *ctx, ph_kvp_node_t **out,
                                size_t *count, ph_error_t **err);
static ph_result_t parse_pair(kvp_ctx_t *ctx, ph_kvp_node_t *out,
                               ph_error_t **err);
static ph_result_t parse_value(kvp_ctx_t *ctx, ph_kvp_node_t *node,
                                ph_error_t **err);
static ph_result_t parse_scalar(kvp_ctx_t *ctx, ph_kvp_node_t *node,
                                 ph_error_t **err);

/* ---- parse_scalar ---- */

static ph_result_t parse_scalar(kvp_ctx_t *ctx, ph_kvp_node_t *node,
                                 ph_error_t **err) {
    const char *c = ctx->cur;

    /* DR-02 precedence: quoted > bool > number > bare */

    /* quoted string */
    if (*c == '"') {
        c++;  /* skip opening quote */
        const char *start = c;
        while (*c != '\0' && *c != '"') {
            if (*c == '\\' && *(c + 1) == '"') {
                c += 2;
            } else {
                c++;
            }
        }
        if (*c != '"') {
            *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
                "unterminated quoted string at position %d", pos_of(ctx));
            return PH_ERR;
        }
        size_t raw_len = (size_t)(c - start);
        /* unescape \" in the result */
        char *buf = ph_alloc(raw_len + 1);
        if (!buf) goto alloc_fail;
        size_t out_len = 0;
        for (size_t i = 0; i < raw_len; i++) {
            if (start[i] == '\\' && i + 1 < raw_len && start[i + 1] == '"') {
                buf[out_len++] = '"';
                i++;
            } else {
                buf[out_len++] = start[i];
            }
        }
        buf[out_len] = '\0';
        node->is_object    = false;
        node->scalar_kind  = PH_KVP_STRING;
        node->value        = buf;
        ctx->cur = c + 1;  /* skip closing quote */
        return PH_OK;
    }

    /* bool literal: true/false followed by delimiter */
    if (strncmp(c, "true", 4) == 0 && is_delimiter(c[4])) {
        node->is_object    = false;
        node->scalar_kind  = PH_KVP_BOOL;
        node->value        = dup_nstr(c, 4);
        if (!node->value) goto alloc_fail;
        ctx->cur = c + 4;
        return PH_OK;
    }
    if (strncmp(c, "false", 5) == 0 && is_delimiter(c[5])) {
        node->is_object    = false;
        node->scalar_kind  = PH_KVP_BOOL;
        node->value        = dup_nstr(c, 5);
        if (!node->value) goto alloc_fail;
        ctx->cur = c + 5;
        return PH_OK;
    }

    /* number literal: optional '-', digits, followed by delimiter */
    {
        const char *n = c;
        if (*n == '-') n++;
        if (is_digit(*n)) {
            const char *num_start = c;
            n++;
            while (is_digit(*n)) n++;
            if (is_delimiter(*n)) {
                size_t len = (size_t)(n - num_start);
                node->is_object    = false;
                node->scalar_kind  = PH_KVP_INT;
                node->value        = dup_nstr(num_start, len);
                if (!node->value) goto alloc_fail;
                ctx->cur = n;
                return PH_OK;
            }
            /* not followed by delimiter: fall through to bare token */
        }
    }

    /* bare token: consume until : | { } \0 */
    {
        const char *start = c;
        while (*c != ':' && *c != '|' && *c != '{' && *c != '}' && *c != '\0') {
            c++;
        }
        if (c == start) {
            *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
                "empty value at position %d", pos_of(ctx));
            return PH_ERR;
        }
        node->is_object    = false;
        node->scalar_kind  = PH_KVP_STRING;
        node->value        = dup_nstr(start, (size_t)(c - start));
        if (!node->value) goto alloc_fail;
        ctx->cur = c;
        return PH_OK;
    }

alloc_fail:
    *err = ph_error_create(PH_ERR_INTERNAL, 0, "allocation failed in kvp parser");
    return PH_ERR;
}

/* ---- parse_value ---- */

static ph_result_t parse_value(kvp_ctx_t *ctx, ph_kvp_node_t *node,
                                ph_error_t **err) {
    if (*ctx->cur == '{') {
        ctx->depth++;
        if (ctx->depth > PH_KVP_MAX_DEPTH) {
            *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
                "nesting depth exceeds %d at position %d",
                PH_KVP_MAX_DEPTH, pos_of(ctx));
            return PH_ERR;
        }
        ctx->cur++;  /* skip '{' */

        ph_kvp_node_t *children = NULL;
        size_t child_count = 0;

        if (parse_pairs(ctx, &children, &child_count, err) != PH_OK) {
            return PH_ERR;
        }

        if (*ctx->cur != '}') {
            ph_kvp_destroy(children, child_count);
            *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
                "expected '}' at position %d", pos_of(ctx));
            return PH_ERR;
        }
        ctx->cur++;  /* skip '}' */
        ctx->depth--;

        node->is_object   = true;
        node->children    = children;
        node->child_count = child_count;
        return PH_OK;
    }

    return parse_scalar(ctx, node, err);
}

/* ---- parse_pair ---- */

static ph_result_t parse_pair(kvp_ctx_t *ctx, ph_kvp_node_t *out,
                               ph_error_t **err) {
    *out = (ph_kvp_node_t){0};

    /* parse key (ident) */
    const char *key_start = ctx->cur;
    if (!is_ident_start(*ctx->cur)) {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
            "expected key identifier at position %d", pos_of(ctx));
        return PH_ERR;
    }
    ctx->cur++;
    while (is_ident_cont(*ctx->cur)) {
        ctx->cur++;
    }
    size_t key_len = (size_t)(ctx->cur - key_start);
    out->key = dup_nstr(key_start, key_len);
    if (!out->key) {
        *err = ph_error_create(PH_ERR_INTERNAL, 0,
            "allocation failed in kvp parser");
        return PH_ERR;
    }

    /* expect ':' separator */
    if (*ctx->cur != ':') {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
            "expected ':' after key '%s' at position %d",
            out->key, pos_of(ctx));
        ph_free(out->key);
        *out = (ph_kvp_node_t){0};
        return PH_ERR;
    }
    ctx->cur++;  /* skip ':' */

    /* parse value */
    if (parse_value(ctx, out, err) != PH_OK) {
        ph_free(out->key);
        *out = (ph_kvp_node_t){0};
        return PH_ERR;
    }

    return PH_OK;
}

/* ---- parse_pairs ---- */

static ph_result_t parse_pairs(kvp_ctx_t *ctx, ph_kvp_node_t **out,
                                size_t *count, ph_error_t **err) {
    node_vec_t vec = {0};

    /* parse first pair */
    ph_kvp_node_t node;
    if (parse_pair(ctx, &node, err) != PH_OK) {
        return PH_ERR;
    }
    if (node_vec_push(&vec, &node) != PH_OK) {
        kvp_node_free_fields(&node);
        *err = ph_error_create(PH_ERR_INTERNAL, 0,
            "allocation failed in kvp parser");
        return PH_ERR;
    }

    /* loop on '|' separator */
    while (*ctx->cur == '|') {
        ctx->cur++;  /* skip '|' */

        if (parse_pair(ctx, &node, err) != PH_OK) {
            ph_kvp_destroy(vec.items, vec.count);
            return PH_ERR;
        }

        /* duplicate key check at current depth (linear scan, small N) */
        for (size_t i = 0; i < vec.count; i++) {
            if (strcmp(vec.items[i].key, node.key) == 0) {
                *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
                    "duplicate key '%s' at position %d",
                    node.key, pos_of(ctx));
                kvp_node_free_fields(&node);
                ph_kvp_destroy(vec.items, vec.count);
                return PH_ERR;
            }
        }

        if (node_vec_push(&vec, &node) != PH_OK) {
            kvp_node_free_fields(&node);
            ph_kvp_destroy(vec.items, vec.count);
            *err = ph_error_create(PH_ERR_INTERNAL, 0,
                "allocation failed in kvp parser");
            return PH_ERR;
        }
    }

    *out   = vec.items;
    *count = vec.count;
    return PH_OK;
}

/* ---- public API ---- */

ph_result_t ph_kvp_parse(const char *input, ph_kvp_node_t **out,
                          size_t *count, ph_error_t **err) {
    if (!input || !out || !count || !err) return PH_ERR;

    *out   = NULL;
    *count = 0;
    *err   = NULL;

    /* validate '!' prefix */
    if (*input != '!') {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
            "KVP value must start with '!' at position 0");
        return PH_ERR;
    }

    /* reject empty '!' */
    if (input[1] == '\0') {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
            "empty KVP after '!' at position 1");
        return PH_ERR;
    }

    kvp_ctx_t ctx = {
        .start = input,
        .cur   = input + 1,    /* skip '!' */
        .depth = 0,
    };

    if (parse_pairs(&ctx, out, count, err) != PH_OK) {
        return PH_ERR;
    }

    /* check no trailing characters */
    if (*ctx.cur != '\0') {
        ph_kvp_destroy(*out, *count);
        *out   = NULL;
        *count = 0;
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
            "unexpected character '%c' at position %d",
            *ctx.cur, pos_of(&ctx));
        return PH_ERR;
    }

    return PH_OK;
}

void ph_kvp_destroy(ph_kvp_node_t *nodes, size_t count) {
    if (!nodes) return;
    for (size_t i = 0; i < count; i++) {
        ph_free(nodes[i].key);
        if (nodes[i].is_object) {
            ph_kvp_destroy(nodes[i].children, nodes[i].child_count);
        } else {
            ph_free(nodes[i].value);
        }
    }
    ph_free(nodes);
}

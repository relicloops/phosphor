#include "phosphor/str.h"
#include "phosphor/alloc.h"

#include <string.h>

ph_result_t ph_str_init(ph_str_t *s, size_t cap) {
    if (!s) return PH_ERR;
    if (cap == 0) cap = 1;

    s->data = ph_alloc(cap);
    if (!s->data) { s->len = 0; s->cap = 0; return PH_ERR; }

    s->data[0] = '\0';
    s->len = 0;
    s->cap = cap;
    return PH_OK;
}

ph_result_t ph_str_from_cstr(ph_str_t *s, const char *cstr) {
    if (!s) return PH_ERR;
    if (!cstr) return ph_str_init(s, 1);

    size_t len = strlen(cstr);
    s->data = ph_alloc(len + 1);
    if (!s->data) { s->len = 0; s->cap = 0; return PH_ERR; }

    memcpy(s->data, cstr, len + 1);
    s->len = len;
    s->cap = len + 1;
    return PH_OK;
}

ph_result_t ph_str_dup(ph_str_t *dst, const ph_str_t *src) {
    if (!dst) return PH_ERR;
    if (!src || !src->data) return ph_str_init(dst, 1);

    dst->data = ph_alloc(src->len + 1);
    if (!dst->data) { dst->len = 0; dst->cap = 0; return PH_ERR; }

    memcpy(dst->data, src->data, src->len + 1);
    dst->len = src->len;
    dst->cap = src->len + 1;
    return PH_OK;
}

static ph_result_t str_grow(ph_str_t *s, size_t needed_cap) {
    if (s->cap >= needed_cap) return PH_OK;

    size_t new_cap = s->cap * 2;
    if (new_cap < needed_cap) new_cap = needed_cap;

    char *new_data = ph_realloc(s->data, new_cap);
    if (!new_data) return PH_ERR;

    s->data = new_data;
    s->cap  = new_cap;
    return PH_OK;
}

ph_result_t ph_str_append(ph_str_t *s, const char *data, size_t len) {
    if (!s) return PH_ERR;
    if (len == 0) return PH_OK;

    if (str_grow(s, s->len + len + 1) != PH_OK) return PH_ERR;

    memcpy(s->data + s->len, data, len);
    s->len += len;
    s->data[s->len] = '\0';
    return PH_OK;
}

ph_result_t ph_str_append_cstr(ph_str_t *s, const char *cstr) {
    if (!cstr) return PH_OK;
    return ph_str_append(s, cstr, strlen(cstr));
}

bool ph_str_equal(const ph_str_t *a, const ph_str_t *b) {
    if (!a || !b) return a == b;
    if (a->len != b->len) return false;
    if (a->len == 0) return true;
    return memcmp(a->data, b->data, a->len) == 0;
}

bool ph_str_equal_cstr(const ph_str_t *a, const char *cstr) {
    if (!a) return !cstr;
    if (!cstr) return false;
    size_t clen = strlen(cstr);
    if (a->len != clen) return false;
    return memcmp(a->data, cstr, clen) == 0;
}

const char *ph_str_cstr(const ph_str_t *s) {
    if (!s || !s->data) return "";
    return s->data;
}

void ph_str_clear(ph_str_t *s) {
    if (!s || !s->data) return;
    s->len = 0;
    s->data[0] = '\0';
}

void ph_str_destroy(ph_str_t *s) {
    if (!s) return;
    ph_free(s->data);
    s->data = NULL;
    s->len  = 0;
    s->cap  = 0;
}

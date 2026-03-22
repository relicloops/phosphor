#include "phosphor/vec.h"
#include "phosphor/alloc.h"

#include <string.h>

ph_result_t ph_vec_init(ph_vec_t *v, size_t elem_size, size_t initial_cap) {
    if (!v || elem_size == 0) return PH_ERR;
    if (initial_cap == 0) initial_cap = 8;

    v->data = ph_alloc(elem_size * initial_cap);
    if (!v->data) {
        v->len = 0; v->cap = 0; v->elem_size = 0;
        return PH_ERR;
    }

    v->len       = 0;
    v->cap       = initial_cap;
    v->elem_size = elem_size;
    return PH_OK;
}

static ph_result_t vec_grow(ph_vec_t *v) {
    size_t new_cap = v->cap * 2;
    void *new_data = ph_realloc(v->data, v->elem_size * new_cap);
    if (!new_data) return PH_ERR;

    v->data = new_data;
    v->cap  = new_cap;
    return PH_OK;
}

ph_result_t ph_vec_push(ph_vec_t *v, const void *elem) {
    if (!v || !elem) return PH_ERR;

    if (v->len >= v->cap) {
        if (vec_grow(v) != PH_OK) return PH_ERR;
    }

    uint8_t *dst = (uint8_t *)v->data + v->len * v->elem_size;
    memcpy(dst, elem, v->elem_size);
    v->len++;
    return PH_OK;
}

void *ph_vec_get(const ph_vec_t *v, size_t index) {
    if (!v || index >= v->len) return NULL;
    return (uint8_t *)v->data + index * v->elem_size;
}

ph_result_t ph_vec_set(ph_vec_t *v, size_t index, const void *elem) {
    if (!v || !elem || index >= v->len) return PH_ERR;

    uint8_t *dst = (uint8_t *)v->data + index * v->elem_size;
    memcpy(dst, elem, v->elem_size);
    return PH_OK;
}

void ph_vec_pop(ph_vec_t *v) {
    if (!v || v->len == 0) return;
    v->len--;
}

void ph_vec_clear(ph_vec_t *v) {
    if (!v) return;
    v->len = 0;
}

void ph_vec_destroy(ph_vec_t *v) {
    if (!v) return;
    ph_free(v->data);
    v->data      = NULL;
    v->len       = 0;
    v->cap       = 0;
    v->elem_size = 0;
}

#ifndef PHOSPHOR_VEC_H
#define PHOSPHOR_VEC_H

#include "phosphor/types.h"

/*
 * ph_vec_t -- generic dynamic array.
 *
 * ownership:
 *   data -- owner: self (heap-allocated, freed by ph_vec_destroy).
 *           elements are stored by value (memcpy). if elements contain
 *           pointers, the caller is responsible for freeing them before
 *           destroying the vector.
 */
typedef struct {
    void  *data;
    size_t len;
    size_t cap;
    size_t elem_size;
} ph_vec_t;

ph_result_t ph_vec_init(ph_vec_t *v, size_t elem_size, size_t initial_cap);
ph_result_t ph_vec_push(ph_vec_t *v, const void *elem);
void       *ph_vec_get(const ph_vec_t *v, size_t index);
ph_result_t ph_vec_set(ph_vec_t *v, size_t index, const void *elem);
void        ph_vec_pop(ph_vec_t *v);
void        ph_vec_clear(ph_vec_t *v);
void        ph_vec_destroy(ph_vec_t *v);

/* typed accessor -- returns pointer to element or NULL if out of bounds */
#define PH_VEC_AT(vec, type, index) ((type *)ph_vec_get((vec), (index)))

#endif /* PHOSPHOR_VEC_H */

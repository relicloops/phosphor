#ifndef PHOSPHOR_STR_H
#define PHOSPHOR_STR_H

#include "phosphor/types.h"

/*
 * ph_str_t -- owned dynamic string, always null-terminated.
 *
 * ownership:
 *   data -- owner: self (heap-allocated, freed by ph_str_destroy)
 *
 * invariants:
 *   data[len] == '\0' after every mutation.
 *   cap is total buffer size (including null terminator space).
 */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} ph_str_t;

ph_result_t ph_str_init(ph_str_t *s, size_t cap);
ph_result_t ph_str_from_cstr(ph_str_t *s, const char *cstr);
ph_result_t ph_str_dup(ph_str_t *dst, const ph_str_t *src);

ph_result_t ph_str_append(ph_str_t *s, const char *data, size_t len);
ph_result_t ph_str_append_cstr(ph_str_t *s, const char *cstr);

bool        ph_str_equal(const ph_str_t *a, const ph_str_t *b);
bool        ph_str_equal_cstr(const ph_str_t *a, const char *cstr);
const char *ph_str_cstr(const ph_str_t *s);

void        ph_str_clear(ph_str_t *s);
void        ph_str_destroy(ph_str_t *s);

#endif /* PHOSPHOR_STR_H */

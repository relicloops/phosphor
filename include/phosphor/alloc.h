#ifndef PHOSPHOR_ALLOC_H
#define PHOSPHOR_ALLOC_H

#include "phosphor/types.h"

/* allocator mode */
typedef enum {
    PH_ALLOC_DEFAULT,   /* thin wrappers around libc */
    PH_ALLOC_DEBUG      /* canary/poison for buffer overflow detection */
} ph_alloc_mode_t;

void  ph_alloc_set_mode(ph_alloc_mode_t mode);
void *ph_alloc(size_t size);
void *ph_calloc(size_t count, size_t size);
void *ph_realloc(void *ptr, size_t size);
void  ph_free(void *ptr);

#endif /* PHOSPHOR_ALLOC_H */

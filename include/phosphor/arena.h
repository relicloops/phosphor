#ifndef PHOSPHOR_ARENA_H
#define PHOSPHOR_ARENA_H

#include "phosphor/types.h"

/*
 * ph_arena_t -- bump allocator for short-lived command lifecycle objects.
 *
 * allocations are fast (pointer bump). individual frees are not supported.
 * ph_arena_reset reuses existing blocks without freeing.
 * ph_arena_destroy frees all blocks.
 *
 * ownership:
 *   head    -- owner: self (entire block chain)
 *   current -- NOT owned, points into head chain
 */

typedef struct ph_arena_block ph_arena_block_t;

struct ph_arena_block {
    uint8_t          *data;   /* owner: self */
    size_t            used;
    size_t            cap;
    ph_arena_block_t *next;   /* owner: self */
};

typedef struct {
    ph_arena_block_t *head;
    ph_arena_block_t *current;
    size_t            block_size;
} ph_arena_t;

ph_result_t ph_arena_init(ph_arena_t *a, size_t block_size);
void       *ph_arena_alloc(ph_arena_t *a, size_t size);
void        ph_arena_reset(ph_arena_t *a);
void        ph_arena_destroy(ph_arena_t *a);

#endif /* PHOSPHOR_ARENA_H */

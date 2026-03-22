#include "phosphor/arena.h"
#include "phosphor/alloc.h"

#include <string.h>

#define PH_ARENA_ALIGN _Alignof(max_align_t)

static size_t align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

static ph_arena_block_t *block_create(size_t cap) {
    ph_arena_block_t *blk = ph_alloc(sizeof(ph_arena_block_t));
    if (!blk) return NULL;

    blk->data = ph_alloc(cap);
    if (!blk->data) { ph_free(blk); return NULL; }

    blk->used = 0;
    blk->cap  = cap;
    blk->next = NULL;
    return blk;
}

static void block_destroy(ph_arena_block_t *blk) {
    while (blk) {
        ph_arena_block_t *next = blk->next;
        ph_free(blk->data);
        ph_free(blk);
        blk = next;
    }
}

ph_result_t ph_arena_init(ph_arena_t *a, size_t block_size) {
    if (!a) return PH_ERR;
    if (block_size == 0) block_size = 4096;

    ph_arena_block_t *blk = block_create(block_size);
    if (!blk) {
        a->head = NULL; a->current = NULL; a->block_size = 0;
        return PH_ERR;
    }

    a->head       = blk;
    a->current    = blk;
    a->block_size = block_size;
    return PH_OK;
}

void *ph_arena_alloc(ph_arena_t *a, size_t size) {
    if (!a || !a->current || size == 0) return NULL;

    size_t aligned = align_up(size, PH_ARENA_ALIGN);
    ph_arena_block_t *blk = a->current;

    /* fits in current block */
    if (blk->used + aligned <= blk->cap) {
        void *ptr = blk->data + blk->used;
        blk->used += aligned;
        return ptr;
    }

    /* try next existing block (after reset) */
    if (blk->next) {
        blk = blk->next;
        if (aligned <= blk->cap) {
            blk->used = aligned;
            a->current = blk;
            return blk->data;
        }
    }

    /* allocate new block */
    size_t new_cap = a->block_size;
    if (new_cap < aligned) new_cap = aligned;

    ph_arena_block_t *new_blk = block_create(new_cap);
    if (!new_blk) return NULL;

    /* append after current */
    new_blk->next = a->current->next;
    a->current->next = new_blk;
    a->current = new_blk;

    new_blk->used = aligned;
    return new_blk->data;
}

void ph_arena_reset(ph_arena_t *a) {
    if (!a) return;

    ph_arena_block_t *blk = a->head;
    while (blk) {
        blk->used = 0;
        blk = blk->next;
    }
    a->current = a->head;
}

void ph_arena_destroy(ph_arena_t *a) {
    if (!a) return;
    block_destroy(a->head);
    a->head       = NULL;
    a->current    = NULL;
    a->block_size = 0;
}

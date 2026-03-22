#include "phosphor/alloc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static ph_alloc_mode_t g_mode = PH_ALLOC_DEFAULT;

/* debug canary pattern */
static const uint8_t CANARY[4] = { 0xCA, 0xFE, 0xBA, 0xBE };
static const uint8_t POISON    = 0xDD;

/*
 * debug layout:
 *   [size_t user_size][4 canary bytes][user data ...][4 canary bytes]
 *                                     ^-- returned pointer
 */
#define DEBUG_PREFIX (sizeof(size_t) + sizeof(CANARY))
#define DEBUG_SUFFIX (sizeof(CANARY))

static void *debug_alloc(size_t size) {
    size_t total = DEBUG_PREFIX + size + DEBUG_SUFFIX;
    uint8_t *raw = malloc(total);
    if (!raw) return NULL;

    memcpy(raw, &size, sizeof(size_t));
    memcpy(raw + sizeof(size_t), CANARY, sizeof(CANARY));
    memcpy(raw + DEBUG_PREFIX + size, CANARY, sizeof(CANARY));

    uint8_t *user = raw + DEBUG_PREFIX;
    memset(user, 0, size);
    return user;
}

static void *debug_raw_from_user(void *ptr, size_t *out_size) {
    uint8_t *user = ptr;
    uint8_t *raw  = user - DEBUG_PREFIX;

    memcpy(out_size, raw, sizeof(size_t));
    return raw;
}

static bool debug_check_canaries(void *ptr) {
    size_t size;
    uint8_t *raw = debug_raw_from_user(ptr, &size);

    if (memcmp(raw + sizeof(size_t), CANARY, sizeof(CANARY)) != 0) {
        fprintf(stderr, "phosphor: heap corruption (head canary)\n");
        return false;
    }
    uint8_t *user = ptr;
    if (memcmp(user + size, CANARY, sizeof(CANARY)) != 0) {
        fprintf(stderr, "phosphor: heap corruption (tail canary)\n");
        return false;
    }
    return true;
}

void ph_alloc_set_mode(ph_alloc_mode_t mode) {
    g_mode = mode;
}

void *ph_alloc(size_t size) {
    if (size == 0) return NULL;

    if (g_mode == PH_ALLOC_DEBUG) {
        return debug_alloc(size);
    }
    return malloc(size);
}

void *ph_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;

    if (g_mode == PH_ALLOC_DEBUG) {
        /* debug_alloc already zeroes the user region */
        return debug_alloc(count * size);
    }
    return calloc(count, size);
}

void *ph_realloc(void *ptr, size_t size) {
    if (!ptr) return ph_alloc(size);
    if (size == 0) { ph_free(ptr); return NULL; }

    if (g_mode == PH_ALLOC_DEBUG) {
        debug_check_canaries(ptr);

        size_t old_size;
        (void)debug_raw_from_user(ptr, &old_size);

        void *new_ptr = debug_alloc(size);
        if (!new_ptr) return NULL;

        size_t copy_size = old_size < size ? old_size : size;
        memcpy(new_ptr, ptr, copy_size);
        ph_free(ptr);
        return new_ptr;
    }
    return realloc(ptr, size);
}

void ph_free(void *ptr) {
    if (!ptr) return;

    if (g_mode == PH_ALLOC_DEBUG) {
        debug_check_canaries(ptr);

        size_t size;
        uint8_t *raw = debug_raw_from_user(ptr, &size);
        size_t total = DEBUG_PREFIX + size + DEBUG_SUFFIX;
        memset(raw, POISON, total);
        free(raw);
        return;
    }
    free(ptr);
}

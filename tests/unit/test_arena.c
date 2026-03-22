#include "unity.h"
#include "phosphor/arena.h"

#include <string.h>

TEST_SOURCE_FILE("src/core/arena.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

void test_arena_init(void) {
    ph_arena_t a;
    ph_result_t rc = ph_arena_init(&a, 256);

    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(a.head);
    TEST_ASSERT_NOT_NULL(a.current);
    TEST_ASSERT_EQUAL(256, a.block_size);

    ph_arena_destroy(&a);
}

void test_arena_init_null_returns_err(void) {
    TEST_ASSERT_EQUAL(PH_ERR, ph_arena_init(NULL, 256));
}

void test_arena_alloc_returns_non_null(void) {
    ph_arena_t a;
    ph_arena_init(&a, 256);

    void *p = ph_arena_alloc(&a, 32);
    TEST_ASSERT_NOT_NULL(p);

    ph_arena_destroy(&a);
}

void test_arena_alloc_zero_returns_null(void) {
    ph_arena_t a;
    ph_arena_init(&a, 256);

    TEST_ASSERT_NULL(ph_arena_alloc(&a, 0));

    ph_arena_destroy(&a);
}

void test_arena_multiple_allocs(void) {
    ph_arena_t a;
    ph_arena_init(&a, 64);

    void *p1 = ph_arena_alloc(&a, 16);
    void *p2 = ph_arena_alloc(&a, 16);
    void *p3 = ph_arena_alloc(&a, 16);

    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_NOT_NULL(p3);
    TEST_ASSERT_NOT_EQUAL(p1, p2);
    TEST_ASSERT_NOT_EQUAL(p2, p3);

    ph_arena_destroy(&a);
}

void test_arena_oversize_alloc(void) {
    ph_arena_t a;
    ph_arena_init(&a, 64);

    void *p = ph_arena_alloc(&a, 128);
    TEST_ASSERT_NOT_NULL(p);

    ph_arena_destroy(&a);
}

void test_arena_reset_reuses_blocks(void) {
    ph_arena_t a;
    ph_arena_init(&a, 256);

    ph_arena_alloc(&a, 32);
    ph_arena_reset(&a);

    TEST_ASSERT_NOT_NULL(a.head);
    TEST_ASSERT_EQUAL_PTR(a.head, a.current);
    TEST_ASSERT_EQUAL(0, a.head->used);

    void *p = ph_arena_alloc(&a, 16);
    TEST_ASSERT_NOT_NULL(p);

    ph_arena_destroy(&a);
}

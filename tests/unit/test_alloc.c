#include "unity.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {
    ph_alloc_set_mode(PH_ALLOC_DEFAULT);
}

void tearDown(void) {
    ph_alloc_set_mode(PH_ALLOC_DEFAULT);
}

void test_alloc_returns_non_null(void) {
    void *p = ph_alloc(64);
    TEST_ASSERT_NOT_NULL(p);
    ph_free(p);
}

void test_alloc_zero_returns_null(void) {
    void *p = ph_alloc(0);
    TEST_ASSERT_NULL(p);
}

void test_calloc_zeroes_memory(void) {
    uint8_t *p = ph_calloc(1, 16);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }
    ph_free(p);
}

void test_calloc_zero_count_returns_null(void) {
    void *p = ph_calloc(0, 16);
    TEST_ASSERT_NULL(p);
}

void test_realloc_null_acts_as_alloc(void) {
    void *p = ph_realloc(NULL, 32);
    TEST_ASSERT_NOT_NULL(p);
    ph_free(p);
}

void test_realloc_grows_buffer(void) {
    char *p = ph_alloc(8);
    TEST_ASSERT_NOT_NULL(p);
    memcpy(p, "hello", 6);

    p = ph_realloc(p, 64);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_STRING("hello", p);
    ph_free(p);
}

void test_free_null_is_safe(void) {
    ph_free(NULL);
}

void test_debug_mode_alloc_and_free(void) {
    ph_alloc_set_mode(PH_ALLOC_DEBUG);

    void *p = ph_alloc(128);
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0xAA, 128);
    ph_free(p);
}

void test_debug_mode_realloc(void) {
    ph_alloc_set_mode(PH_ALLOC_DEBUG);

    char *p = ph_alloc(16);
    TEST_ASSERT_NOT_NULL(p);
    memcpy(p, "test", 5);

    p = ph_realloc(p, 64);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_STRING("test", p);
    ph_free(p);
}

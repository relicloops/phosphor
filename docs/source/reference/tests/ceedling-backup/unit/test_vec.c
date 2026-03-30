#include "unity.h"
#include "phosphor/vec.h"

#include <string.h>

TEST_SOURCE_FILE("src/core/vec.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

void test_vec_init(void) {
    ph_vec_t v;
    ph_result_t rc = ph_vec_init(&v, sizeof(int), 4);

    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(v.data);
    TEST_ASSERT_EQUAL(0, v.len);
    TEST_ASSERT_EQUAL(4, v.cap);
    TEST_ASSERT_EQUAL(sizeof(int), v.elem_size);

    ph_vec_destroy(&v);
}

void test_vec_init_null_returns_err(void) {
    TEST_ASSERT_EQUAL(PH_ERR, ph_vec_init(NULL, sizeof(int), 4));
}

void test_vec_init_zero_elem_returns_err(void) {
    ph_vec_t v;
    TEST_ASSERT_EQUAL(PH_ERR, ph_vec_init(&v, 0, 4));
}

void test_vec_push_and_get(void) {
    ph_vec_t v;
    ph_vec_init(&v, sizeof(int), 4);

    int val = 42;
    TEST_ASSERT_EQUAL(PH_OK, ph_vec_push(&v, &val));
    TEST_ASSERT_EQUAL(1, v.len);

    int *got = PH_VEC_AT(&v, int, 0);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL(42, *got);

    ph_vec_destroy(&v);
}

void test_vec_push_grows(void) {
    ph_vec_t v;
    ph_vec_init(&v, sizeof(int), 2);

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL(PH_OK, ph_vec_push(&v, &i));
    }
    TEST_ASSERT_EQUAL(10, v.len);

    for (int i = 0; i < 10; i++) {
        int *got = PH_VEC_AT(&v, int, (size_t)i);
        TEST_ASSERT_EQUAL(i, *got);
    }

    ph_vec_destroy(&v);
}

void test_vec_set(void) {
    ph_vec_t v;
    ph_vec_init(&v, sizeof(int), 4);

    int val = 10;
    ph_vec_push(&v, &val);
    val = 20;
    ph_vec_push(&v, &val);

    val = 99;
    TEST_ASSERT_EQUAL(PH_OK, ph_vec_set(&v, 1, &val));
    TEST_ASSERT_EQUAL(99, *PH_VEC_AT(&v, int, 1));

    ph_vec_destroy(&v);
}

void test_vec_get_out_of_bounds(void) {
    ph_vec_t v;
    ph_vec_init(&v, sizeof(int), 4);
    TEST_ASSERT_NULL(ph_vec_get(&v, 0));

    ph_vec_destroy(&v);
}

void test_vec_pop(void) {
    ph_vec_t v;
    ph_vec_init(&v, sizeof(int), 4);

    int val = 1;
    ph_vec_push(&v, &val);
    val = 2;
    ph_vec_push(&v, &val);

    ph_vec_pop(&v);
    TEST_ASSERT_EQUAL(1, v.len);

    ph_vec_destroy(&v);
}

void test_vec_clear(void) {
    ph_vec_t v;
    ph_vec_init(&v, sizeof(int), 4);

    int val = 1;
    ph_vec_push(&v, &val);
    ph_vec_clear(&v);
    TEST_ASSERT_EQUAL(0, v.len);

    ph_vec_destroy(&v);
}

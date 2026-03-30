#include "unity.h"
#include "phosphor/str.h"

TEST_SOURCE_FILE("src/core/str.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

void test_str_from_cstr(void) {
    ph_str_t s;
    ph_result_t rc = ph_str_from_cstr(&s, "hello");

    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(5, s.len);
    TEST_ASSERT_NOT_NULL(s.data);
    TEST_ASSERT_EQUAL_STRING("hello", ph_str_cstr(&s));

    ph_str_destroy(&s);
}

void test_str_append_cstr(void) {
    ph_str_t s;
    ph_str_from_cstr(&s, "hello");

    ph_result_t rc = ph_str_append_cstr(&s, " world");

    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(11, s.len);
    TEST_ASSERT_EQUAL_STRING("hello world", ph_str_cstr(&s));

    ph_str_destroy(&s);
}

void test_str_equal(void) {
    ph_str_t a, b;
    ph_str_from_cstr(&a, "hello");
    ph_str_from_cstr(&b, "hello");

    TEST_ASSERT_TRUE(ph_str_equal(&a, &b));

    ph_str_destroy(&a);
    ph_str_destroy(&b);
}

void test_str_clear(void) {
    ph_str_t s;
    ph_str_from_cstr(&s, "hello");

    ph_str_clear(&s);

    TEST_ASSERT_EQUAL(0, s.len);
    TEST_ASSERT_NOT_NULL(s.data);
    TEST_ASSERT_EQUAL_STRING("", ph_str_cstr(&s));

    ph_str_destroy(&s);
}

void test_str_destroy(void) {
    ph_str_t s;
    ph_str_from_cstr(&s, "hello");

    ph_str_destroy(&s);

    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL(0, s.len);
    TEST_ASSERT_EQUAL(0, s.cap);
}

#include "unity.h"
#include "phosphor/error.h"

#include <string.h>

TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

void test_error_create(void) {
    ph_error_t *err = ph_error_create(PH_ERR_USAGE, 1, "test error");

    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);
    TEST_ASSERT_EQUAL(1, err->subcode);
    TEST_ASSERT_EQUAL_STRING("test error", err->message);
    TEST_ASSERT_NULL(err->context);
    TEST_ASSERT_NULL(err->cause);

    ph_error_destroy(err);
}

void test_error_createf(void) {
    ph_error_t *err = ph_error_createf(PH_ERR_GENERAL, 0,
        "value is %d", 42);

    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("value is 42", err->message);

    ph_error_destroy(err);
}

void test_error_set_context(void) {
    ph_error_t *err = ph_error_create(PH_ERR_USAGE, 0, "msg");
    ph_error_set_context(err, "while parsing");

    TEST_ASSERT_EQUAL_STRING("while parsing", err->context);

    ph_error_destroy(err);
}

void test_error_chain(void) {
    ph_error_t *outer = ph_error_create(PH_ERR_USAGE, 1, "outer");
    ph_error_t *inner = ph_error_create(PH_ERR_INTERNAL, 0, "inner");

    ph_error_chain(outer, inner);

    TEST_ASSERT_EQUAL_PTR(inner, outer->cause);
    TEST_ASSERT_EQUAL_STRING("inner", outer->cause->message);

    ph_error_destroy(outer);
}

void test_error_destroy_null_is_safe(void) {
    ph_error_destroy(NULL);
}

void test_error_create_null_message(void) {
    ph_error_t *err = ph_error_create(PH_ERR_GENERAL, 0, NULL);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NULL(err->message);
    ph_error_destroy(err);
}

void test_error_set_context_null_err(void) {
    ph_error_set_context(NULL, "context");
}

void test_error_chain_null_err(void) {
    ph_error_t *cause = ph_error_create(PH_ERR_GENERAL, 0, "cause");
    ph_error_chain(NULL, cause);
    ph_error_destroy(cause);
}

#include "unity.h"
#include "phosphor/render.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/template/transform.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- ph_transform_is_binary ---- */

void test_binary_by_extension_png(void) {
    uint8_t data[] = "not actually binary";
    TEST_ASSERT_TRUE(ph_transform_is_binary(data, sizeof(data) - 1, ".png"));
}

void test_binary_by_extension_exe(void) {
    uint8_t data[] = "irrelevant";
    TEST_ASSERT_TRUE(ph_transform_is_binary(data, sizeof(data) - 1, ".exe"));
}

void test_binary_by_extension_zip(void) {
    uint8_t data[] = "irrelevant";
    TEST_ASSERT_TRUE(ph_transform_is_binary(data, sizeof(data) - 1, ".zip"));
}

void test_not_binary_txt(void) {
    uint8_t data[] = "Hello world, plain text";
    TEST_ASSERT_FALSE(ph_transform_is_binary(data, sizeof(data) - 1, ".txt"));
}

void test_binary_by_nul_byte(void) {
    uint8_t data[] = { 'H', 'e', 'l', 0x00, 'o' };
    TEST_ASSERT_TRUE(ph_transform_is_binary(data, 5, ".txt"));
}

void test_not_binary_no_ext(void) {
    uint8_t data[] = "plain text content here";
    TEST_ASSERT_FALSE(ph_transform_is_binary(data, sizeof(data) - 1, NULL));
}

/* ---- ph_transform_newline ---- */

void test_newline_keep(void) {
    const char *input = "line1\r\nline2\nline3\rline4";
    uint8_t *out = NULL;
    size_t out_len = 0;

    ph_result_t rc = ph_transform_newline((const uint8_t *)input,
                                           strlen(input), "keep",
                                           &out, &out_len);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(strlen(input), out_len);
    TEST_ASSERT_EQUAL_MEMORY(input, out, out_len);
    ph_free(out);
}

void test_newline_null_mode(void) {
    const char *input = "line1\nline2";
    uint8_t *out = NULL;
    size_t out_len = 0;

    ph_result_t rc = ph_transform_newline((const uint8_t *)input,
                                           strlen(input), NULL,
                                           &out, &out_len);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(strlen(input), out_len);
    ph_free(out);
}

void test_newline_lf(void) {
    const char *input = "line1\r\nline2\rline3\nline4";
    uint8_t *out = NULL;
    size_t out_len = 0;

    ph_result_t rc = ph_transform_newline((const uint8_t *)input,
                                           strlen(input), "lf",
                                           &out, &out_len);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    /* all line endings should be \n */
    const char *expected = "line1\nline2\nline3\nline4";
    TEST_ASSERT_EQUAL(strlen(expected), out_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, out, out_len);
    ph_free(out);
}

void test_newline_crlf(void) {
    const char *input = "line1\nline2\rline3";
    uint8_t *out = NULL;
    size_t out_len = 0;

    ph_result_t rc = ph_transform_newline((const uint8_t *)input,
                                           strlen(input), "crlf",
                                           &out, &out_len);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    const char *expected = "line1\r\nline2\r\nline3";
    TEST_ASSERT_EQUAL(strlen(expected), out_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, out, out_len);
    ph_free(out);
}

void test_newline_crlf_pass_through(void) {
    const char *input = "line1\r\nline2\r\n";
    uint8_t *out = NULL;
    size_t out_len = 0;

    ph_result_t rc = ph_transform_newline((const uint8_t *)input,
                                           strlen(input), "crlf",
                                           &out, &out_len);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(strlen(input), out_len);
    TEST_ASSERT_EQUAL_MEMORY(input, out, out_len);
    ph_free(out);
}

void test_newline_null_args(void) {
    ph_result_t rc = ph_transform_newline(NULL, 0, "lf", NULL, NULL);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
}

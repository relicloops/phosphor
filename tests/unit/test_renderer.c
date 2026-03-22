#include "unity.h"
#include "phosphor/render.h"
#include "phosphor/template.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/template/renderer.c")
TEST_SOURCE_FILE("src/template/var_merge.c")
TEST_SOURCE_FILE("src/args-parser/args_helpers.c")
TEST_SOURCE_FILE("src/template/manifest_load.c")
TEST_SOURCE_FILE("src/core/config.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

void setUp(void) {}
void tearDown(void) {}

/* helper to create resolved vars */
static ph_resolved_var_t make_var(const char *name, const char *value) {
    ph_resolved_var_t v;
    v.name = (char *)name;
    v.value = (char *)value;
    v.type = PH_VAR_STRING;
    return v;
}

void test_render_no_placeholders(void) {
    const char *input = "Hello world";
    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         NULL, 0, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(strlen(input), out_len);
    TEST_ASSERT_EQUAL_MEMORY(input, out, out_len);
    ph_free(out);
}

void test_render_simple_variable(void) {
    const char *input = "Hello <<name>>!";
    ph_resolved_var_t vars[] = { make_var("name", "World") };

    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         vars, 1, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING_LEN("Hello World!", out, out_len);
    ph_free(out);
}

void test_render_multiple_variables(void) {
    const char *input = "<<greeting>> <<name>>!";
    ph_resolved_var_t vars[] = {
        make_var("greeting", "Hi"),
        make_var("name", "Alice"),
    };

    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         vars, 2, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING_LEN("Hi Alice!", out, out_len);
    ph_free(out);
}

void test_render_unresolved_kept(void) {
    const char *input = "<<unknown>> stays";
    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         NULL, 0, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING_LEN("<<unknown>> stays", out, out_len);
    ph_free(out);
}

void test_render_escaped_marker(void) {
    const char *input = "literal \\<<not_a_var>>";
    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         NULL, 0, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    /* should emit literal << followed by rest of text */
    TEST_ASSERT_EQUAL_STRING_LEN("literal <<not_a_var>>", out, out_len);
    ph_free(out);
}

void test_render_empty_input(void) {
    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)"", 0,
                                         NULL, 0, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(0, out_len);
    ph_free(out);
}

void test_render_adjacent_vars(void) {
    const char *input = "<<a>><<b>>";
    ph_resolved_var_t vars[] = {
        make_var("a", "X"),
        make_var("b", "Y"),
    };

    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         vars, 2, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING_LEN("XY", out, out_len);
    ph_free(out);
}

void test_render_var_at_boundaries(void) {
    const char *input = "<<name>>";
    ph_resolved_var_t vars[] = { make_var("name", "Value") };

    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         vars, 1, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING_LEN("Value", out, out_len);
    ph_free(out);
}

void test_render_null_data(void) {
    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template(NULL, 0,
                                         NULL, 0, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

void test_render_longer_value(void) {
    const char *input = "<<x>>";
    ph_resolved_var_t vars[] = {
        make_var("x", "this is a much longer replacement string"),
    };

    uint8_t *out = NULL;
    size_t out_len = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_render_template((const uint8_t *)input, strlen(input),
                                         vars, 1, &out, &out_len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING_LEN("this is a much longer replacement string",
                                  out, out_len);
    ph_free(out);
}

#include "unity.h"
#include "phosphor/args.h"

#include <string.h>

TEST_SOURCE_FILE("src/args-parser/kvp.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- helpers ---- */

static ph_result_t kvp(const char *input, ph_kvp_node_t **out,
                        size_t *count, ph_error_t **err) {
    return ph_kvp_parse(input, out, count, err);
}

/* ---- tests ---- */

void test_kvp_single_string(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!key:value", &nodes, &count, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("key", nodes[0].key);
    TEST_ASSERT_FALSE(nodes[0].is_object);
    TEST_ASSERT_EQUAL_STRING("value", nodes[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_single_int(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!port:8080", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(PH_KVP_INT, nodes[0].scalar_kind);
    TEST_ASSERT_EQUAL_STRING("8080", nodes[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_negative_int(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!offset:-42", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(PH_KVP_INT, nodes[0].scalar_kind);
    TEST_ASSERT_EQUAL_STRING("-42", nodes[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_bool_true(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!flag:true", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(PH_KVP_BOOL, nodes[0].scalar_kind);
    TEST_ASSERT_EQUAL_STRING("true", nodes[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_bool_false(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!flag:false", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(PH_KVP_BOOL, nodes[0].scalar_kind);
    TEST_ASSERT_EQUAL_STRING("false", nodes[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_quoted_string(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!msg:\"hello world\"", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(PH_KVP_STRING, nodes[0].scalar_kind);
    TEST_ASSERT_EQUAL_STRING("hello world", nodes[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_quoted_escape(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!msg:\"say \\\"hi\\\"\"",
                                   &nodes, &count, &err));
    TEST_ASSERT_EQUAL_STRING("say \"hi\"", nodes[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_multiple_pairs(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!a:1|b:2|c:3", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL_STRING("a", nodes[0].key);
    TEST_ASSERT_EQUAL_STRING("b", nodes[1].key);
    TEST_ASSERT_EQUAL_STRING("c", nodes[2].key);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_nested_object(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, kvp("!outer:{inner:42}", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_TRUE(nodes[0].is_object);
    TEST_ASSERT_EQUAL(1, nodes[0].child_count);
    TEST_ASSERT_EQUAL_STRING("inner", nodes[0].children[0].key);
    TEST_ASSERT_EQUAL_STRING("42", nodes[0].children[0].value);

    ph_kvp_destroy(nodes, count);
}

void test_kvp_missing_prefix(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp("key:value", &nodes, &count, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
}

void test_kvp_empty_after_prefix(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp("!", &nodes, &count, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
}

void test_kvp_missing_colon(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp("!key", &nodes, &count, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
}

void test_kvp_unterminated_quote(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp("!key:\"unterminated",
                                    &nodes, &count, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
}

void test_kvp_duplicate_key(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp("!a:1|a:2", &nodes, &count, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
}

void test_kvp_trailing_chars(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp("!a:1}", &nodes, &count, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
}

void test_kvp_null_input(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp(NULL, &nodes, &count, &err));
}

void test_kvp_unclosed_brace(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, kvp("!a:{b:1", &nodes, &count, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
}

void test_kvp_mixed_types(void) {
    ph_kvp_node_t *nodes = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK,
        kvp("!name:demo|port:3000|debug:true", &nodes, &count, &err));
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(PH_KVP_STRING, nodes[0].scalar_kind);
    TEST_ASSERT_EQUAL(PH_KVP_INT,    nodes[1].scalar_kind);
    TEST_ASSERT_EQUAL(PH_KVP_BOOL,   nodes[2].scalar_kind);

    ph_kvp_destroy(nodes, count);
}

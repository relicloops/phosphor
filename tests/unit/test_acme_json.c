#include "unity.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/certs/acme_json.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")

/* forward declarations -- internal header not on include path */
char *json_extract_string(const char *json, const char *key);
char **json_extract_string_array(const char *json, const char *key,
                                   size_t *out_count);

void setUp(void) {}
void tearDown(void) {}

/* ---- json_extract_string tests ---- */

void test_extract_string_basic(void) {
    const char *json = "{\"name\":\"phosphor\"}";
    char *val = json_extract_string(json, "name");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("phosphor", val);
    ph_free(val);
}

void test_extract_string_with_spaces(void) {
    const char *json = "{\"key\" : \"value\"}";
    char *val = json_extract_string(json, "key");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("value", val);
    ph_free(val);
}

void test_extract_string_missing_key(void) {
    const char *json = "{\"name\":\"phosphor\"}";
    char *val = json_extract_string(json, "missing");
    TEST_ASSERT_NULL(val);
}

void test_extract_string_null_json(void) {
    char *val = json_extract_string(NULL, "key");
    TEST_ASSERT_NULL(val);
}

void test_extract_string_null_key(void) {
    char *val = json_extract_string("{\"a\":\"b\"}", NULL);
    TEST_ASSERT_NULL(val);
}

void test_extract_string_multiple_keys(void) {
    const char *json = "{\"a\":\"1\",\"b\":\"2\",\"c\":\"3\"}";
    char *a = json_extract_string(json, "a");
    char *b = json_extract_string(json, "b");
    char *c = json_extract_string(json, "c");
    TEST_ASSERT_EQUAL_STRING("1", a);
    TEST_ASSERT_EQUAL_STRING("2", b);
    TEST_ASSERT_EQUAL_STRING("3", c);
    ph_free(a);
    ph_free(b);
    ph_free(c);
}

void test_extract_string_url_value(void) {
    const char *json = "{\"newNonce\":\"https://acme.example/new-nonce\"}";
    char *val = json_extract_string(json, "newNonce");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("https://acme.example/new-nonce", val);
    ph_free(val);
}

void test_extract_string_empty_value(void) {
    const char *json = "{\"empty\":\"\"}";
    char *val = json_extract_string(json, "empty");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("", val);
    ph_free(val);
}

void test_extract_string_non_string_value(void) {
    /* value is a number, not a string -- should return NULL */
    const char *json = "{\"count\":42}";
    char *val = json_extract_string(json, "count");
    TEST_ASSERT_NULL(val);
}

/* ---- json_extract_string_array tests ---- */

void test_extract_array_basic(void) {
    const char *json = "{\"items\":[\"a\",\"b\",\"c\"]}";
    size_t count = 0;
    char **arr = json_extract_string_array(json, "items", &count);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL_STRING("a", arr[0]);
    TEST_ASSERT_EQUAL_STRING("b", arr[1]);
    TEST_ASSERT_EQUAL_STRING("c", arr[2]);
    for (size_t i = 0; i < count; i++) ph_free(arr[i]);
    ph_free(arr);
}

void test_extract_array_single_element(void) {
    const char *json = "{\"urls\":[\"https://example.com\"]}";
    size_t count = 0;
    char **arr = json_extract_string_array(json, "urls", &count);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("https://example.com", arr[0]);
    ph_free(arr[0]);
    ph_free(arr);
}

void test_extract_array_missing_key(void) {
    const char *json = "{\"other\":[\"x\"]}";
    size_t count = 0;
    char **arr = json_extract_string_array(json, "missing", &count);
    TEST_ASSERT_NULL(arr);
    TEST_ASSERT_EQUAL(0, count);
}

void test_extract_array_empty(void) {
    const char *json = "{\"items\":[]}";
    size_t count = 0;
    char **arr = json_extract_string_array(json, "items", &count);
    TEST_ASSERT_NULL(arr);
    TEST_ASSERT_EQUAL(0, count);
}

void test_extract_array_null_json(void) {
    size_t count = 0;
    char **arr = json_extract_string_array(NULL, "key", &count);
    TEST_ASSERT_NULL(arr);
    TEST_ASSERT_EQUAL(0, count);
}

void test_extract_array_null_key(void) {
    size_t count = 0;
    char **arr = json_extract_string_array("{\"a\":[\"b\"]}", NULL, &count);
    TEST_ASSERT_NULL(arr);
    TEST_ASSERT_EQUAL(0, count);
}

void test_extract_array_acme_authorizations(void) {
    const char *json =
        "{\"status\":\"pending\","
        "\"authorizations\":[\"https://acme.example/authz/1\","
        "\"https://acme.example/authz/2\"],"
        "\"finalize\":\"https://acme.example/finalize/1\"}";
    size_t count = 0;
    char **arr = json_extract_string_array(json, "authorizations", &count);
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_STRING("https://acme.example/authz/1", arr[0]);
    TEST_ASSERT_EQUAL_STRING("https://acme.example/authz/2", arr[1]);
    for (size_t i = 0; i < count; i++) ph_free(arr[i]);
    ph_free(arr);
}

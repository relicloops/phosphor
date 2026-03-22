#include "unity.h"
#include "phosphor/certs.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/certs/acme_jws.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- test: NULL input returns empty string ---- */

void test_base64url_null_returns_empty(void) {
    char *out = ph_acme_base64url_encode(NULL, 0);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("", out);
    ph_free(out);
}

/* ---- test: empty data returns empty string ---- */

void test_base64url_empty_returns_empty(void) {
    uint8_t data[] = {0};
    char *out = ph_acme_base64url_encode(data, 0);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("", out);
    ph_free(out);
}

/* ---- test: single byte (padding edge case: 2 chars padding removed) ---- */

void test_base64url_one_byte(void) {
    /* 'f' = 0x66 -> base64url "Zg" (no padding) */
    uint8_t data[] = {'f'};
    char *out = ph_acme_base64url_encode(data, 1);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("Zg", out);
    ph_free(out);
}

/* ---- test: two bytes (padding edge case: 1 char padding removed) ---- */

void test_base64url_two_bytes(void) {
    /* 'fo' -> base64url "Zm8" (no padding) */
    uint8_t data[] = {'f', 'o'};
    char *out = ph_acme_base64url_encode(data, 2);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("Zm8", out);
    ph_free(out);
}

/* ---- test: three bytes (no padding needed) ---- */

void test_base64url_three_bytes(void) {
    /* 'foo' -> base64url "Zm9v" */
    uint8_t data[] = {'f', 'o', 'o'};
    char *out = ph_acme_base64url_encode(data, 3);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("Zm9v", out);
    ph_free(out);
}

/* ---- test: RFC 4648 test vectors ---- */

void test_base64url_rfc4648_foob(void) {
    /* 'foob' -> "Zm9vYg" */
    uint8_t data[] = {'f', 'o', 'o', 'b'};
    char *out = ph_acme_base64url_encode(data, 4);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("Zm9vYg", out);
    ph_free(out);
}

void test_base64url_rfc4648_fooba(void) {
    /* 'fooba' -> "Zm9vYmE" */
    uint8_t data[] = {'f', 'o', 'o', 'b', 'a'};
    char *out = ph_acme_base64url_encode(data, 5);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("Zm9vYmE", out);
    ph_free(out);
}

void test_base64url_rfc4648_foobar(void) {
    /* 'foobar' -> "Zm9vYmFy" */
    uint8_t data[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    char *out = ph_acme_base64url_encode(data, 6);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", out);
    ph_free(out);
}

/* ---- test: base64url uses - and _ instead of + and / ---- */

void test_base64url_uses_url_safe_chars(void) {
    /* bytes that produce + and / in standard base64 should use - and _ */
    /* 0xFB, 0xFF, 0xFE -> standard b64 "+//+" -> base64url "-__-" */
    uint8_t data[] = {0xFB, 0xFF, 0xFE};
    char *out = ph_acme_base64url_encode(data, 3);
    TEST_ASSERT_NOT_NULL(out);
    /* verify no + or / in output */
    TEST_ASSERT_NULL(strchr(out, '+'));
    TEST_ASSERT_NULL(strchr(out, '/'));
    /* verify - and _ are present */
    TEST_ASSERT_EQUAL_STRING("-__-", out);
    ph_free(out);
}

/* ---- test: no padding chars in output ---- */

void test_base64url_no_padding_chars(void) {
    uint8_t data[] = {'A'};
    char *out = ph_acme_base64url_encode(data, 1);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NULL(strchr(out, '='));
    ph_free(out);
}

#include "unity.h"
#include "phosphor/sha256.h"
#include "phosphor/alloc.h"
#include "phosphor/error.h"

#include <string.h>
#include <stdio.h>

TEST_SOURCE_FILE("src/crypto/sha256.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")

void setUp(void) {}
void tearDown(void) {}

/* helper: write string to a temp file, return path */
static const char *write_tmp(const char *content, size_t len) {
    static const char *path = "/tmp/phosphor_test_sha256.bin";
    FILE *fp = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(fp);
    if (len > 0) fwrite(content, 1, len, fp);
    fclose(fp);
    return path;
}

/* ---- known vectors ---- */

/* SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
void test_sha256_abc(void) {
    const char *path = write_tmp("abc", 3);
    char hex[PH_SHA256_HEX_LEN];
    ph_error_t *err = NULL;
    ph_result_t rc = ph_sha256_file(path, hex, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        hex);
}

/* SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 */
void test_sha256_empty(void) {
    const char *path = write_tmp("", 0);
    char hex[PH_SHA256_HEX_LEN];
    ph_error_t *err = NULL;
    ph_result_t rc = ph_sha256_file(path, hex, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        hex);
}

/* SHA256("hello world\n") = a948904f2f0f479b8f8564e9d7a7e6e5c6a5c6073ebfc69c2c7e37f61b0e2b28
 * (note: exactly "hello world" with no newline) */
/* SHA256("hello world") = b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9 */
void test_sha256_hello_world(void) {
    const char *path = write_tmp("hello world", 11);
    char hex[PH_SHA256_HEX_LEN];
    ph_error_t *err = NULL;
    ph_result_t rc = ph_sha256_file(path, hex, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING(
        "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9",
        hex);
}

/* ---- ph_sha256_verify ---- */

void test_verify_match(void) {
    const char *path = write_tmp("abc", 3);
    ph_error_t *err = NULL;
    ph_result_t rc = ph_sha256_verify(path,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
}

void test_verify_mismatch(void) {
    const char *path = write_tmp("abc", 3);
    ph_error_t *err = NULL;
    ph_result_t rc = ph_sha256_verify(path,
        "0000000000000000000000000000000000000000000000000000000000000000",
        &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, err->category);
    ph_error_destroy(err);
}

void test_verify_bad_hex_length(void) {
    const char *path = write_tmp("abc", 3);
    ph_error_t *err = NULL;
    ph_result_t rc = ph_sha256_verify(path, "abcdef", &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, err->category);
    ph_error_destroy(err);
}

/* ---- NULL handling ---- */

void test_sha256_file_null_path(void) {
    char hex[PH_SHA256_HEX_LEN];
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_sha256_file(NULL, hex, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_sha256_file_null_out(void) {
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_sha256_file("/tmp/x", NULL, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_sha256_file_nonexistent(void) {
    char hex[PH_SHA256_HEX_LEN];
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR,
        ph_sha256_file("/tmp/phosphor_no_such_file_ever", hex, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_FS, err->category);
    ph_error_destroy(err);
}

void test_verify_null_path(void) {
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_sha256_verify(NULL, "abc", &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_verify_null_expected(void) {
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_sha256_verify("/tmp/x", NULL, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

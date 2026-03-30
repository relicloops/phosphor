#include "unity.h"
#include "phosphor/git_fetch.h"
#include "phosphor/alloc.h"
#include "phosphor/error.h"

#include <string.h>

TEST_SOURCE_FILE("src/io/git_fetch.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("src/io/fs_copytree.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/platform/signal.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- ph_git_is_url ---- */

void test_is_url_https(void) {
    TEST_ASSERT_TRUE(ph_git_is_url("https://github.com/user/repo"));
}

void test_is_url_http(void) {
    TEST_ASSERT_TRUE(ph_git_is_url("http://example.com/repo"));
}

void test_is_url_local_absolute(void) {
    TEST_ASSERT_FALSE(ph_git_is_url("/home/user/templates/mytemplate"));
}

void test_is_url_local_relative(void) {
    TEST_ASSERT_FALSE(ph_git_is_url("./templates/mytemplate"));
}

void test_is_url_null(void) {
    TEST_ASSERT_FALSE(ph_git_is_url(NULL));
}

void test_is_url_empty(void) {
    TEST_ASSERT_FALSE(ph_git_is_url(""));
}

void test_is_url_ftp(void) {
    TEST_ASSERT_FALSE(ph_git_is_url("ftp://example.com/repo"));
}

void test_is_url_ssh(void) {
    TEST_ASSERT_FALSE(ph_git_is_url("git@github.com:user/repo.git"));
}

/* ---- ph_git_url_parse ---- */

void test_parse_https_no_ref(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "https://github.com/user/repo", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("https://github.com/user/repo", parsed.url);
    TEST_ASSERT_NULL(parsed.ref);
    ph_git_url_destroy(&parsed);
}

void test_parse_https_with_branch(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "https://github.com/user/repo#develop", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("https://github.com/user/repo", parsed.url);
    TEST_ASSERT_EQUAL_STRING("develop", parsed.ref);
    ph_git_url_destroy(&parsed);
}

void test_parse_https_with_tag(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "https://github.com/user/repo#v1.0.0", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("https://github.com/user/repo", parsed.url);
    TEST_ASSERT_EQUAL_STRING("v1.0.0", parsed.ref);
    ph_git_url_destroy(&parsed);
}

void test_parse_http_with_ref(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "http://gitlab.com/org/project#feature-x", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("http://gitlab.com/org/project", parsed.url);
    TEST_ASSERT_EQUAL_STRING("feature-x", parsed.ref);
    ph_git_url_destroy(&parsed);
}

void test_parse_empty_ref_rejected(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "https://github.com/user/repo#", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_CONFIG, err->category);
    ph_error_destroy(err);
}

void test_parse_traversal_ref_rejected(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "https://github.com/user/repo#../../etc/passwd", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_CONFIG, err->category);
    ph_error_destroy(err);
}

void test_parse_bad_scheme_ftp(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "ftp://example.com/repo", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_CONFIG, err->category);
    ph_error_destroy(err);
}

void test_parse_bad_scheme_ssh(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_git_url_parse(
        "git@github.com:user/repo.git", &parsed, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_parse_null_input(void) {
    ph_git_url_t parsed;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_git_url_parse(NULL, &parsed, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_parse_null_output(void) {
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR,
        ph_git_url_parse("https://example.com", NULL, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ---- ph_git_url_destroy ---- */

void test_destroy_null_noop(void) {
    ph_git_url_destroy(NULL);
}

void test_destroy_zeroed(void) {
    ph_git_url_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    ph_git_url_destroy(&parsed);
    TEST_ASSERT_NULL(parsed.url);
    TEST_ASSERT_NULL(parsed.ref);
}

/* ---- ph_git_cleanup_clone ---- */

void test_cleanup_null_noop(void) {
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_git_cleanup_clone(NULL, &err));
    TEST_ASSERT_NULL(err);
}

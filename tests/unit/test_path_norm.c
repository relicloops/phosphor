#include "unity.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- ph_path_normalize ---- */

void test_normalize_empty(void) {
    char *r = ph_path_normalize("");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING(".", r);
    ph_free(r);
}

void test_normalize_dot(void) {
    char *r = ph_path_normalize("./foo/./bar");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("foo/bar", r);
    ph_free(r);
}

void test_normalize_double_slash(void) {
    char *r = ph_path_normalize("foo//bar///baz");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("foo/bar/baz", r);
    ph_free(r);
}

void test_normalize_trailing_slash(void) {
    char *r = ph_path_normalize("foo/bar/");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("foo/bar", r);
    ph_free(r);
}

void test_normalize_absolute(void) {
    char *r = ph_path_normalize("/usr//local/./bin");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/usr/local/bin", r);
    ph_free(r);
}

void test_normalize_root(void) {
    char *r = ph_path_normalize("/");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/", r);
    ph_free(r);
}

/* ---- ph_path_has_traversal ---- */

void test_traversal_none(void) {
    TEST_ASSERT_FALSE(ph_path_has_traversal("foo/bar/baz"));
}

void test_traversal_present(void) {
    TEST_ASSERT_TRUE(ph_path_has_traversal("foo/../bar"));
}

void test_traversal_at_start(void) {
    TEST_ASSERT_TRUE(ph_path_has_traversal("../secret"));
}

void test_traversal_at_end(void) {
    TEST_ASSERT_TRUE(ph_path_has_traversal("foo/.."));
}

void test_traversal_not_in_name(void) {
    TEST_ASSERT_FALSE(ph_path_has_traversal("foo/bar..baz"));
}

void test_traversal_null(void) {
    TEST_ASSERT_FALSE(ph_path_has_traversal(NULL));
}

/* ---- ph_path_is_absolute ---- */

void test_absolute_yes(void) {
    TEST_ASSERT_TRUE(ph_path_is_absolute("/usr/local"));
}

void test_absolute_no(void) {
    TEST_ASSERT_FALSE(ph_path_is_absolute("foo/bar"));
}

void test_absolute_null(void) {
    TEST_ASSERT_FALSE(ph_path_is_absolute(NULL));
}

/* ---- ph_path_join ---- */

void test_join_simple(void) {
    char *r = ph_path_join("/usr", "local");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/usr/local", r);
    ph_free(r);
}

void test_join_rel_absolute(void) {
    char *r = ph_path_join("/usr", "/etc");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/etc", r);
    ph_free(r);
}

void test_join_trailing_slash(void) {
    char *r = ph_path_join("/usr/", "local");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/usr/local", r);
    ph_free(r);
}

void test_join_dot_rel(void) {
    char *r = ph_path_join("/usr", "./local");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/usr/local", r);
    ph_free(r);
}

/* ---- ph_path_dirname ---- */

void test_dirname_file(void) {
    char *r = ph_path_dirname("/usr/local/bin/foo");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/usr/local/bin", r);
    ph_free(r);
}

void test_dirname_no_slash(void) {
    char *r = ph_path_dirname("foo");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING(".", r);
    ph_free(r);
}

void test_dirname_root(void) {
    char *r = ph_path_dirname("/foo");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("/", r);
    ph_free(r);
}

/* ---- ph_path_basename ---- */

void test_basename_file(void) {
    char *r = ph_path_basename("/usr/local/bin/foo");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("foo", r);
    ph_free(r);
}

void test_basename_no_slash(void) {
    char *r = ph_path_basename("foo");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("foo", r);
    ph_free(r);
}

void test_basename_trailing_slash(void) {
    char *r = ph_path_basename("/usr/local/");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_STRING("local", r);
    ph_free(r);
}

/* ---- ph_path_extension ---- */

void test_extension_normal(void) {
    const char *ext = ph_path_extension("file.txt");
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQUAL_STRING(".txt", ext);
}

void test_extension_double(void) {
    const char *ext = ph_path_extension("archive.tar.gz");
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQUAL_STRING(".gz", ext);
}

void test_extension_none(void) {
    const char *ext = ph_path_extension("Makefile");
    TEST_ASSERT_NULL(ext);
}

void test_extension_hidden(void) {
    const char *ext = ph_path_extension(".gitignore");
    TEST_ASSERT_NULL(ext);
}

void test_extension_path(void) {
    const char *ext = ph_path_extension("/usr/local/lib/foo.so");
    TEST_ASSERT_NOT_NULL(ext);
    TEST_ASSERT_EQUAL_STRING(".so", ext);
}

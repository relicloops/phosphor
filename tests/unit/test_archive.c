#include "unity.h"
#include "phosphor/archive.h"
#include "phosphor/alloc.h"
#include "phosphor/error.h"

#include <string.h>

TEST_SOURCE_FILE("src/io/archive.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/io/fs_copytree.c")
TEST_SOURCE_FILE("src/crypto/sha256.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/platform/signal.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- ph_archive_detect ---- */

void test_detect_tar_gz(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_TAR_GZ,
        ph_archive_detect("template.tar.gz"));
}

void test_detect_tgz(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_TAR_GZ,
        ph_archive_detect("template.tgz"));
}

void test_detect_tar_zst(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_TAR_ZST,
        ph_archive_detect("template.tar.zst"));
}

void test_detect_zip(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_ZIP,
        ph_archive_detect("project.zip"));
}

void test_detect_case_insensitive_tar_gz(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_TAR_GZ,
        ph_archive_detect("TEMPLATE.TAR.GZ"));
}

void test_detect_case_insensitive_zip(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_ZIP,
        ph_archive_detect("Archive.ZIP"));
}

void test_detect_with_path(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_TAR_GZ,
        ph_archive_detect("/home/user/templates/my-template.tar.gz"));
}

void test_detect_relative_path(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_ZIP,
        ph_archive_detect("./templates/project.zip"));
}

void test_detect_none_directory(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_NONE,
        ph_archive_detect("/home/user/templates/my-template"));
}

void test_detect_none_toml(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_NONE,
        ph_archive_detect("template.phosphor.toml"));
}

void test_detect_none_tar_only(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_NONE,
        ph_archive_detect("template.tar"));
}

void test_detect_none_gz_only(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_NONE,
        ph_archive_detect("file.gz"));
}

void test_detect_null(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_NONE, ph_archive_detect(NULL));
}

void test_detect_empty(void) {
    TEST_ASSERT_EQUAL(PH_ARCHIVE_NONE, ph_archive_detect(""));
}

/* ---- ph_archive_cleanup_extract ---- */

void test_cleanup_null_noop(void) {
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_archive_cleanup_extract(NULL, &err));
    TEST_ASSERT_NULL(err);
}

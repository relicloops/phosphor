#include "unity.h"
#include "phosphor/fs.h"

#include <string.h>

TEST_SOURCE_FILE("src/io/metadata_filter.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")

void setUp(void) {}
void tearDown(void) {}

void test_deny_ds_store(void) {
    TEST_ASSERT_TRUE(ph_metadata_is_denied(".DS_Store"));
}

void test_deny_thumbs_db(void) {
    TEST_ASSERT_TRUE(ph_metadata_is_denied("Thumbs.db"));
}

void test_deny_spotlight(void) {
    TEST_ASSERT_TRUE(ph_metadata_is_denied(".Spotlight-V100"));
}

void test_deny_trashes(void) {
    TEST_ASSERT_TRUE(ph_metadata_is_denied(".Trashes"));
}

void test_deny_desktop_ini(void) {
    TEST_ASSERT_TRUE(ph_metadata_is_denied("desktop.ini"));
}

void test_deny_fseventsd(void) {
    TEST_ASSERT_TRUE(ph_metadata_is_denied(".fseventsd"));
}

void test_deny_apple_double(void) {
    TEST_ASSERT_TRUE(ph_metadata_is_denied("._resource_fork"));
}

void test_allow_normal_file(void) {
    TEST_ASSERT_FALSE(ph_metadata_is_denied("README.md"));
}

void test_allow_dotfile(void) {
    TEST_ASSERT_FALSE(ph_metadata_is_denied(".gitignore"));
}

void test_deny_null(void) {
    TEST_ASSERT_FALSE(ph_metadata_is_denied(NULL));
}

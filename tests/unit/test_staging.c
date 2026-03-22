#include "unity.h"
#include "phosphor/template.h"
#include "phosphor/platform.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

TEST_SOURCE_FILE("src/template/staging.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/io/fs_copytree.c")
TEST_SOURCE_FILE("src/io/metadata_filter.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/platform/signal.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")

static char tmpdir[256];
static int test_counter = 0;

void setUp(void) {
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/ph_test_staging_%d_%d",
             (int)getpid(), test_counter++);
    ph_fs_mkdir_p(tmpdir, 0755);
}

static void rmtree_helper(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    (void)system(cmd);
}

void tearDown(void) {
    rmtree_helper(tmpdir);
}

/* ---- create/destroy ---- */

void test_staging_create_and_destroy(void) {
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/my-project", tmpdir);

    ph_staging_t staging;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_staging_create(dest, &staging, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(staging.path);
    TEST_ASSERT_NOT_NULL(staging.dest_path);
    TEST_ASSERT_TRUE(staging.active);
    TEST_ASSERT_EQUAL_STRING(dest, staging.dest_path);

    /* verify staging dir exists */
    ph_fs_stat_t st;
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_stat(staging.path, &st));
    TEST_ASSERT_TRUE(st.is_dir);

    ph_staging_destroy(&staging);
}

void test_staging_naming(void) {
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/test-proj", tmpdir);

    ph_staging_t staging;
    ph_error_t *err = NULL;
    ph_staging_create(dest, &staging, &err);

    /* name should contain .phosphor-staging- prefix */
    char *base = ph_path_basename(staging.path);
    TEST_ASSERT_NOT_NULL(base);
    TEST_ASSERT_TRUE(strncmp(base, ".phosphor-staging-", 18) == 0);
    ph_free(base);

    ph_staging_cleanup(&staging, NULL);
    ph_staging_destroy(&staging);
}

/* ---- cleanup ---- */

void test_staging_cleanup(void) {
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/cleanup-test", tmpdir);

    ph_staging_t staging;
    ph_error_t *err = NULL;
    ph_staging_create(dest, &staging, &err);

    char *staging_path = ph_alloc(strlen(staging.path) + 1);
    memcpy(staging_path, staging.path, strlen(staging.path) + 1);

    ph_result_t rc = ph_staging_cleanup(&staging, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_FALSE(staging.active);

    /* staging dir should be gone */
    ph_fs_stat_t st;
    ph_fs_stat(staging_path, &st);
    TEST_ASSERT_FALSE(st.exists);

    ph_free(staging_path);
    ph_staging_destroy(&staging);
}

/* ---- commit ---- */

void test_staging_commit(void) {
    char dest[512];
    snprintf(dest, sizeof(dest), "%s/commit-test", tmpdir);

    ph_staging_t staging;
    ph_error_t *err = NULL;
    ph_staging_create(dest, &staging, &err);

    /* write a file into staging */
    char *file_path = ph_path_join(staging.path, "hello.txt");
    const char *content = "hello world";
    ph_fs_write_file(file_path, (const uint8_t *)content, strlen(content));
    ph_free(file_path);

    ph_result_t rc = ph_staging_commit(&staging, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_FALSE(staging.active);

    /* dest should now exist with the file */
    char *committed_file = ph_path_join(dest, "hello.txt");
    ph_fs_stat_t st;
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_stat(committed_file, &st));
    TEST_ASSERT_TRUE(st.exists);
    ph_free(committed_file);

    ph_staging_destroy(&staging);
}

/* ---- find_stale ---- */

void test_staging_find_stale_none(void) {
    char **paths = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_staging_find_stale(tmpdir, &paths, &count, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(0, count);
    ph_free(paths);
}

void test_staging_find_stale_present(void) {
    /* create a fake stale staging dir */
    char stale[512];
    snprintf(stale, sizeof(stale), "%s/.phosphor-staging-99999-1234567890",
             tmpdir);
    ph_fs_mkdir_p(stale, 0755);

    char **paths = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_staging_find_stale(tmpdir, &paths, &count, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_NOT_NULL(paths[0]);

    for (size_t i = 0; i < count; i++) ph_free(paths[i]);
    ph_free(paths);
}

/* ---- null args ---- */

void test_staging_create_null(void) {
    ph_staging_t staging;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_staging_create(NULL, &staging, &err));
    ph_error_destroy(err);
}

void test_staging_cleanup_inactive(void) {
    ph_staging_t staging;
    memset(&staging, 0, sizeof(staging));
    /* cleanup on inactive staging should be no-op */
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_staging_cleanup(&staging, &err));
}

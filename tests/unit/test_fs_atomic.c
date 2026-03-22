#include "unity.h"
#include "phosphor/fs.h"
#include "phosphor/platform.h"
#include "phosphor/alloc.h"
#include "phosphor/error.h"
#include "phosphor/path.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

TEST_SOURCE_FILE("src/io/fs_atomic.c")
TEST_SOURCE_FILE("src/io/fs_readwrite.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/platform/signal.c")

static char test_dir[PATH_MAX];

void setUp(void) {
    snprintf(test_dir, sizeof(test_dir),
             "/tmp/phosphor_test_atomic_%d", (int)getpid());
    mkdir(test_dir, 0755);
}

void tearDown(void) {
    /* best-effort cleanup */
    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    (void)system(cmd);
}

/* ---- helper: build path inside test_dir ---- */

static char path_buf[PATH_MAX];

static const char *test_path(const char *filename) {
    snprintf(path_buf, sizeof(path_buf), "%s/%s", test_dir, filename);
    return path_buf;
}

/* ---- helper: read file into buffer, return length ---- */

static size_t read_file(const char *path, uint8_t *buf, size_t bufsize) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    size_t n = fread(buf, 1, bufsize, fp);
    fclose(fp);
    return n;
}

/* ---- basic success: write + verify ---- */

void test_atomic_write_basic(void) {
    const char *path = test_path("basic.txt");
    const uint8_t data[] = "hello atomic world";
    size_t len = strlen((const char *)data);

    ph_error_t *err = NULL;
    ph_result_t rc = ph_fs_atomic_write(path, data, len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);

    /* verify contents */
    uint8_t buf[256];
    size_t n = read_file(path, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(len, n);
    TEST_ASSERT_EQUAL_MEMORY(data, buf, len);
}

/* ---- empty data ---- */

void test_atomic_write_empty(void) {
    const char *path = test_path("empty.txt");
    const uint8_t data[] = "";

    ph_error_t *err = NULL;
    ph_result_t rc = ph_fs_atomic_write(path, data, 0, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);

    /* file should exist but be empty */
    ph_fs_stat_t st;
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_stat(path, &st));
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_file);
    TEST_ASSERT_EQUAL(0, st.size);
}

/* ---- overwrite existing file ---- */

void test_atomic_write_overwrite(void) {
    const char *path = test_path("overwrite.txt");

    /* write initial content */
    const uint8_t data1[] = "original";
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_fs_atomic_write(path, data1, strlen((const char *)data1), &err));
    TEST_ASSERT_NULL(err);

    /* overwrite with new content */
    const uint8_t data2[] = "replacement content that is longer";
    size_t len2 = strlen((const char *)data2);
    TEST_ASSERT_EQUAL(PH_OK,
        ph_fs_atomic_write(path, data2, len2, &err));
    TEST_ASSERT_NULL(err);

    /* verify new contents */
    uint8_t buf[256];
    size_t n = read_file(path, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(len2, n);
    TEST_ASSERT_EQUAL_MEMORY(data2, buf, len2);
}

/* ---- binary data with NUL bytes ---- */

void test_atomic_write_binary(void) {
    const char *path = test_path("binary.bin");
    const uint8_t data[] = {0x00, 0x01, 0xFF, 0xFE, 0x00, 0x42, 0x00};
    size_t len = sizeof(data);

    ph_error_t *err = NULL;
    ph_result_t rc = ph_fs_atomic_write(path, data, len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);

    uint8_t buf[256];
    size_t n = read_file(path, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(len, n);
    TEST_ASSERT_EQUAL_MEMORY(data, buf, len);
}

/* ---- larger data (multiple write() calls possible) ---- */

void test_atomic_write_large(void) {
    const char *path = test_path("large.bin");
    size_t len = 128 * 1024; /* 128 KB */
    uint8_t *data = (uint8_t *)malloc(len);
    TEST_ASSERT_NOT_NULL(data);

    /* fill with pattern */
    for (size_t i = 0; i < len; i++)
        data[i] = (uint8_t)(i & 0xFF);

    ph_error_t *err = NULL;
    ph_result_t rc = ph_fs_atomic_write(path, data, len, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);

    /* verify */
    uint8_t *readback = (uint8_t *)malloc(len + 1);
    TEST_ASSERT_NOT_NULL(readback);
    size_t n = read_file(path, readback, len + 1);
    TEST_ASSERT_EQUAL(len, n);
    TEST_ASSERT_EQUAL_MEMORY(data, readback, len);

    free(readback);
    free(data);
}

/* ---- NULL path -> PH_ERR ---- */

void test_atomic_write_null_path(void) {
    const uint8_t data[] = "test";
    ph_error_t *err = NULL;
    ph_result_t rc = ph_fs_atomic_write(NULL, data, 4, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_INTERNAL, err->category);
    ph_error_destroy(err);
}

/* ---- NULL path with NULL err pointer (no crash) ---- */

void test_atomic_write_null_path_null_err(void) {
    const uint8_t data[] = "test";
    ph_result_t rc = ph_fs_atomic_write(NULL, data, 4, NULL);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
}

/* ---- write to nonexistent directory -> PH_ERR (mkstemp fails) ---- */

void test_atomic_write_bad_dir(void) {
    const char *path = "/tmp/phosphor_no_such_dir_ever/file.txt";
    const uint8_t data[] = "test";
    ph_error_t *err = NULL;
    ph_result_t rc = ph_fs_atomic_write(path, data, 4, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_FS, err->category);
    ph_error_destroy(err);
}

/* ---- no temp file left behind on success ---- */

void test_atomic_write_no_temp_residue(void) {
    const char *path = test_path("clean.txt");
    const uint8_t data[] = "clean write";

    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_fs_atomic_write(path, data, strlen((const char *)data), &err));
    TEST_ASSERT_NULL(err);

    /* check that no .clean.txt.XXXXXX files remain */
    char pattern[PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s/.clean.txt.*", test_dir);

    /* count matching files via a simple directory scan */
    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "ls %s/.clean.txt.* 2>/dev/null | wc -l",
             test_dir);
    FILE *fp = popen(cmd, "r");
    TEST_ASSERT_NOT_NULL(fp);
    int count = 0;
    (void)fscanf(fp, "%d", &count);
    pclose(fp);
    TEST_ASSERT_EQUAL(0, count);
}

/* ---- write preserves file on atomicity ---- */

void test_atomic_write_preserves_original_on_success(void) {
    const char *path = test_path("preserve.txt");

    /* create original file */
    const uint8_t orig[] = "original data";
    FILE *fp = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(fp);
    fwrite(orig, 1, strlen((const char *)orig), fp);
    fclose(fp);

    /* atomic write new data */
    const uint8_t newdata[] = "new data";
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_fs_atomic_write(path, newdata, strlen((const char *)newdata), &err));
    TEST_ASSERT_NULL(err);

    /* file should now have new data */
    uint8_t buf[256];
    size_t n = read_file(path, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(strlen((const char *)newdata), n);
    TEST_ASSERT_EQUAL_MEMORY(newdata, buf, n);
}

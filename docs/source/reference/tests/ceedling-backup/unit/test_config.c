#include "unity.h"
#include "phosphor/config.h"
#include "phosphor/platform.h"
#include "phosphor/alloc.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

TEST_SOURCE_FILE("src/core/config.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

static char tmpdir[256];
static int test_counter = 0;

void setUp(void) {
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/ph_test_config_%d_%d",
             (int)getpid(), test_counter++);
    ph_fs_mkdir_p(tmpdir, 0755);
}

void tearDown(void) {
    /* clean up config files */
    char path[512];
    snprintf(path, sizeof(path), "%s/.phosphor.toml", tmpdir);
    unlink(path);
    snprintf(path, sizeof(path), "%s/phosphor.toml", tmpdir);
    unlink(path);
    rmdir(tmpdir);
}

static void write_config(const char *name, const char *content) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", tmpdir, name);
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(content, f);
    fclose(f);
}

/* ---- discovery ---- */

void test_discover_dot_phosphor_toml(void) {
    write_config(".phosphor.toml",
        "author = \"Alice\"\n"
        "license = \"MIT\"\n"
    );

    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_config_discover(tmpdir, &cfg, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(cfg.file_path);
    TEST_ASSERT_EQUAL(2, cfg.count);
    ph_config_destroy(&cfg);
}

void test_discover_phosphor_toml(void) {
    write_config("phosphor.toml",
        "name = \"test-project\"\n"
    );

    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_config_discover(tmpdir, &cfg, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(cfg.file_path);
    TEST_ASSERT_EQUAL(1, cfg.count);
    ph_config_destroy(&cfg);
}

void test_discover_dot_takes_priority(void) {
    write_config(".phosphor.toml", "source = \"dot\"\n");
    write_config("phosphor.toml", "source = \"plain\"\n");

    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_config_discover(tmpdir, &cfg, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    /* .phosphor.toml should be found first */
    const char *val = ph_config_get(&cfg, "source");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("dot", val);
    ph_config_destroy(&cfg);
}

void test_discover_no_config(void) {
    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_config_discover(tmpdir, &cfg, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(cfg.file_path);
    TEST_ASSERT_EQUAL(0, cfg.count);
    ph_config_destroy(&cfg);
}

/* ---- ph_config_get ---- */

void test_config_get_found(void) {
    write_config(".phosphor.toml",
        "key1 = \"value1\"\n"
        "key2 = \"value2\"\n"
    );

    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_config_discover(tmpdir, &cfg, &err);

    TEST_ASSERT_EQUAL_STRING("value1", ph_config_get(&cfg, "key1"));
    TEST_ASSERT_EQUAL_STRING("value2", ph_config_get(&cfg, "key2"));
    ph_config_destroy(&cfg);
}

void test_config_get_not_found(void) {
    write_config(".phosphor.toml", "key = \"value\"\n");

    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_config_discover(tmpdir, &cfg, &err);

    TEST_ASSERT_NULL(ph_config_get(&cfg, "missing"));
    ph_config_destroy(&cfg);
}

void test_config_get_null(void) {
    TEST_ASSERT_NULL(ph_config_get(NULL, "key"));
    ph_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_NULL(ph_config_get(&cfg, NULL));
}

/* ---- edge cases ---- */

void test_discover_null_dir(void) {
    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_config_discover(NULL, &cfg, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(0, cfg.count);
}

void test_discover_nonexistent_dir(void) {
    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_config_discover("/tmp/nonexistent_ph_dir_xyz", &cfg, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(0, cfg.count);
}

void test_config_bool_value(void) {
    write_config(".phosphor.toml",
        "debug = true\n"
        "verbose = false\n"
    );

    ph_config_t cfg;
    ph_error_t *err = NULL;
    ph_config_discover(tmpdir, &cfg, &err);

    TEST_ASSERT_EQUAL_STRING("true", ph_config_get(&cfg, "debug"));
    TEST_ASSERT_EQUAL_STRING("false", ph_config_get(&cfg, "verbose"));
    ph_config_destroy(&cfg);
}

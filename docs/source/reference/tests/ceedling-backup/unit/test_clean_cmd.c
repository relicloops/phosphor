#include "unity.h"
#include "phosphor/commands.h"
#include "phosphor/alloc.h"
#include "phosphor/platform.h"
#include "phosphor/path.h"
#include "phosphor/fs.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>

TEST_SOURCE_FILE("src/cli/cli_dispatch.c")
TEST_SOURCE_FILE("src/cli/cli_help.c")
TEST_SOURCE_FILE("src/cli/cli_version.c")
TEST_SOURCE_FILE("src/commands/phosphor_commands.c")
TEST_SOURCE_FILE("src/commands/create_cmd.c")
TEST_SOURCE_FILE("src/commands/build_cmd.c")
TEST_SOURCE_FILE("src/commands/clean_cmd.c")
TEST_SOURCE_FILE("src/commands/rm_cmd.c")
TEST_SOURCE_FILE("src/commands/certs_cmd.c")
TEST_SOURCE_FILE("src/commands/doctor_cmd.c")
TEST_SOURCE_FILE("src/args-parser/args_helpers.c")
TEST_SOURCE_FILE("src/certs/certs_config.c")
TEST_SOURCE_FILE("src/certs/certs_san.c")
TEST_SOURCE_FILE("src/certs/certs_ca.c")
TEST_SOURCE_FILE("src/certs/certs_leaf.c")
TEST_SOURCE_FILE("src/proc/spawn.c")
TEST_SOURCE_FILE("src/proc/wait.c")
TEST_SOURCE_FILE("src/proc/env.c")
TEST_SOURCE_FILE("src/platform/posix/proc_posix.c")
TEST_SOURCE_FILE("src/args-parser/spec.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("src/core/config.c")
TEST_SOURCE_FILE("src/template/manifest_load.c")
TEST_SOURCE_FILE("src/template/var_merge.c")
TEST_SOURCE_FILE("src/template/planner.c")
TEST_SOURCE_FILE("src/template/renderer.c")
TEST_SOURCE_FILE("src/template/staging.c")
TEST_SOURCE_FILE("src/template/writer.c")
TEST_SOURCE_FILE("src/template/transform.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/io/fs_copytree.c")
TEST_SOURCE_FILE("src/io/fs_readwrite.c")
TEST_SOURCE_FILE("src/io/fs_atomic.c")
TEST_SOURCE_FILE("src/io/metadata_filter.c")
TEST_SOURCE_FILE("src/io/git_fetch.c")
TEST_SOURCE_FILE("src/io/archive.c")
TEST_SOURCE_FILE("src/crypto/sha256.c")
TEST_SOURCE_FILE("src/core/regex.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/platform/signal.c")
TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

static char test_dir[PATH_MAX];
static unsigned test_seq = 0;

void setUp(void) {
    /* create a unique temp directory for each test */
    snprintf(test_dir, sizeof(test_dir), "/tmp/ph_clean_test_%d_%u",
             (int)getpid(), test_seq++);
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(test_dir, 0755));
}

void tearDown(void) {
    /* best-effort cleanup */
    ph_error_t *err = NULL;
    ph_fs_rmtree(test_dir, &err);
    ph_error_destroy(err);
}

/* ---- helpers ---- */

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *c = ph_alloc(len + 1);
    memcpy(c, s, len + 1);
    return c;
}

static ph_parsed_flag_t make_flag(const char *name, const char *value) {
    ph_parsed_flag_t f = {
        .kind       = value ? PH_FLAG_VALUED : PH_FLAG_BOOL,
        .name       = dup_str(name),
        .value      = dup_str(value),
        .argv_index = 0,
    };
    return f;
}

static void free_flags(ph_parsed_flag_t *flags, size_t count) {
    for (size_t i = 0; i < count; i++) {
        ph_free(flags[i].name);
        ph_free(flags[i].value);
    }
}

/* ---- tests: clean with no existing dirs ---- */

void test_clean_nothing_to_remove(void) {
    ph_parsed_flag_t flags[1];
    flags[0] = make_flag("project", test_dir);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = flags,
        .flag_count = 1,
    };

    int rc = ph_cmd_clean(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    free_flags(flags, 1);
}

/* ---- tests: clean removes build/ and public/ ---- */

void test_clean_removes_build_and_public(void) {
    /* create build/ and public/ in test_dir */
    char *build_path = ph_path_join(test_dir, "build");
    char *public_path = ph_path_join(test_dir, "public");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(build_path, 0755));
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(public_path, 0755));

    ph_parsed_flag_t flags[1];
    flags[0] = make_flag("project", test_dir);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = flags,
        .flag_count = 1,
    };

    int rc = ph_cmd_clean(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    /* verify dirs are gone */
    ph_fs_stat_t st;
    ph_fs_stat(build_path, &st);
    TEST_ASSERT_FALSE(st.exists);
    ph_fs_stat(public_path, &st);
    TEST_ASSERT_FALSE(st.exists);

    free_flags(flags, 1);
    ph_free(build_path);
    ph_free(public_path);
}

/* ---- tests: --dry-run does not delete ---- */

void test_clean_dry_run_preserves_dirs(void) {
    /* create build/ in test_dir */
    char *build_path = ph_path_join(test_dir, "build");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(build_path, 0755));

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("dry-run", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_clean(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    /* build/ should still exist */
    ph_fs_stat_t st;
    ph_fs_stat(build_path, &st);
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_dir);

    free_flags(flags, 2);
    ph_free(build_path);
}

/* ---- tests: --stale removes staging dirs ---- */

void test_clean_stale_removes_staging(void) {
    /* create a fake staging dir */
    char *staging = ph_path_join(test_dir, ".phosphor-staging-12345-999");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(staging, 0755));

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("stale", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_clean(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    /* staging dir should be gone */
    ph_fs_stat_t st;
    ph_fs_stat(staging, &st);
    TEST_ASSERT_FALSE(st.exists);

    free_flags(flags, 2);
    ph_free(staging);
}

/* ---- tests: --stale with --dry-run ---- */

void test_clean_stale_dry_run_preserves(void) {
    char *staging = ph_path_join(test_dir, ".phosphor-staging-99-111");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(staging, 0755));

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("stale", NULL);
    flags[2] = make_flag("dry-run", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_clean(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    /* staging dir should still exist */
    ph_fs_stat_t st;
    ph_fs_stat(staging, &st);
    TEST_ASSERT_TRUE(st.exists);

    free_flags(flags, 3);
    ph_free(staging);
}

/* ---- tests: non-existent project root ---- */

void test_clean_bad_project_returns_validate(void) {
    ph_parsed_flag_t flags[1];
    flags[0] = make_flag("project", "/tmp/ph_clean_test_nonexistent_12345");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = flags,
        .flag_count = 1,
    };

    int rc = ph_cmd_clean(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, rc);

    free_flags(flags, 1);
}

/* ---- tests: --stale with no staging dirs ---- */

void test_clean_stale_none_found(void) {
    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("stale", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_clean(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    free_flags(flags, 2);
}

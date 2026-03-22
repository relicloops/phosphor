#include "unity.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/alloc.h"
#include "phosphor/fs.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
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
    snprintf(test_dir, sizeof(test_dir), "/tmp/ph_cli_test_%d_%u",
             (int)getpid(), test_seq++);
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(test_dir, 0755));
}

void tearDown(void) {
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

/* ---- ph_cli_version tests ---- */

void test_cli_version_returns_zero(void) {
    TEST_ASSERT_EQUAL(0, ph_cli_version());
}

/* ---- ph_cli_help tests ---- */

void test_cli_help_no_topic_returns_zero(void) {
    TEST_ASSERT_EQUAL(0, ph_cli_help(&phosphor_cli_config, NULL));
}

void test_cli_help_valid_topic_returns_zero(void) {
    TEST_ASSERT_EQUAL(0, ph_cli_help(&phosphor_cli_config, "create"));
}

void test_cli_help_unknown_topic_returns_usage(void) {
    TEST_ASSERT_EQUAL(PH_ERR_USAGE,
        ph_cli_help(&phosphor_cli_config, "nonexistent"));
}

void test_cli_help_version_topic(void) {
    TEST_ASSERT_EQUAL(0, ph_cli_help(&phosphor_cli_config, "version"));
}

/* ---- ph_cli_dispatch tests ---- */

void test_dispatch_version(void) {
    ph_parsed_args_t args = { .command_id = PHOSPHOR_CMD_VERSION };
    TEST_ASSERT_EQUAL(0,
        ph_cli_dispatch(&phosphor_cli_config, &args));
}

void test_dispatch_help_no_topic(void) {
    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_HELP,
        .positional = NULL,
    };
    TEST_ASSERT_EQUAL(0,
        ph_cli_dispatch(&phosphor_cli_config, &args));
}

void test_dispatch_help_with_topic(void) {
    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_HELP,
        .positional = dup_str("build"),
    };
    int rc = ph_cli_dispatch(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);
    ph_free(args.positional);
}

void test_dispatch_create_returns_nonzero_without_flags(void) {
    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flag_count = 0,
        .flags = NULL,
        .positional = NULL,
    };
    /* create requires --name and --template; missing flags → non-zero */
    int rc = ph_cli_dispatch(&phosphor_cli_config, &args);
    TEST_ASSERT_NOT_EQUAL(0, rc);
}

void test_dispatch_build_no_src_returns_validate(void) {
    /* use test_dir as --project (no src/ dir there) */
    ph_parsed_flag_t flag = {
        .kind       = PH_FLAG_VALUED,
        .name       = dup_str("project"),
        .value      = dup_str(test_dir),
        .argv_index = 0,
    };
    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = &flag,
        .flag_count = 1,
    };
    /* test_dir has no src/ -> PH_ERR_VALIDATE (6) */
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE,
        ph_cli_dispatch(&phosphor_cli_config, &args));
    ph_free(flag.name);
    ph_free(flag.value);
}

void test_dispatch_clean_no_dirs_returns_zero(void) {
    /* point clean at test_dir so it never touches the real build/ directory */
    ph_parsed_flag_t flag = {
        .kind       = PH_FLAG_VALUED,
        .name       = dup_str("project"),
        .value      = dup_str(test_dir),
        .argv_index = 0,
    };
    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CLEAN,
        .flags      = &flag,
        .flag_count = 1,
    };
    /* test_dir has no build/ or public/ -- clean reports nothing, returns 0 */
    TEST_ASSERT_EQUAL(0,
        ph_cli_dispatch(&phosphor_cli_config, &args));
    ph_free(flag.name);
    ph_free(flag.value);
}

void test_dispatch_unknown_command(void) {
    ph_parsed_args_t args = { .command_id = 999 };
    TEST_ASSERT_EQUAL(PH_ERR_INTERNAL,
        ph_cli_dispatch(&phosphor_cli_config, &args));
}

void test_dispatch_null_config(void) {
    ph_parsed_args_t args = { .command_id = PHOSPHOR_CMD_VERSION };
    TEST_ASSERT_EQUAL(PH_ERR_INTERNAL,
        ph_cli_dispatch(NULL, &args));
}

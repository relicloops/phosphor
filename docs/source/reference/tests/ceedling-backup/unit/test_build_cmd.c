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
#include <sys/stat.h>
#include <stdlib.h>

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
    snprintf(test_dir, sizeof(test_dir), "/tmp/ph_build_test_%d_%u",
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

static void write_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fputs(content, fp);
    fclose(fp);
}

/* ---- test: no src/ directory -> PH_ERR_VALIDATE ---- */

void test_build_missing_src_dir(void) {
    ph_parsed_flag_t flags[1];
    flags[0] = make_flag("project", test_dir);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 1,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, rc);

    free_flags(flags, 1);
}

/* ---- test: deploy-at absolute path escapes root -> PH_ERR_VALIDATE ---- */

void test_build_deploy_at_escapes_root(void) {
    char *src = ph_path_join(test_dir, "src");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(src, 0755));

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("deploy-at", "/tmp/ph_build_escape_target");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, rc);

    free_flags(flags, 2);
    ph_free(src);
}

/* ---- test: deploy-at inside root passes guard ---- */

void test_build_deploy_at_inside_root(void) {
    char *src = ph_path_join(test_dir, "src");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(src, 0755));

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("deploy-at", "output/deploy");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    /* passes deploy-at guard (step 4); fails later at esbuild/npm */
    TEST_ASSERT_NOT_EQUAL(PH_ERR_VALIDATE, rc);

    free_flags(flags, 2);
    ph_free(src);
}

/* ---- test: legacy mode, no scripts/_default/all.sh -> PH_ERR_VALIDATE ---- */

void test_build_legacy_missing_script(void) {
    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("legacy-scripts", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, rc);

    free_flags(flags, 2);
}

/* ---- test: legacy happy path (script exits 0) ---- */

void test_build_legacy_happy_path(void) {
    char *script_dir = ph_path_join(test_dir, "scripts/_default");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(script_dir, 0755));

    char *script = ph_path_join(script_dir, "all.sh");
    write_file(script, "#!/bin/sh\nexit 0\n");
    chmod(script, 0755);

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("legacy-scripts", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    free_flags(flags, 2);
    ph_free(script);
    ph_free(script_dir);
}

/* ---- test: legacy metadata cleanup removes .DS_Store ---- */

void test_build_legacy_metadata_cleanup(void) {
    char *deploy = ph_path_join(test_dir, "deploy");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(deploy, 0755));

    char *ds_store = ph_path_join(deploy, ".DS_Store");
    write_file(ds_store, "metadata");

    char *script_dir = ph_path_join(test_dir, "scripts/_default");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(script_dir, 0755));

    char *script = ph_path_join(script_dir, "all.sh");
    write_file(script, "#!/bin/sh\nexit 0\n");
    chmod(script, 0755);

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("legacy-scripts", NULL);
    flags[2] = make_flag("deploy-at", deploy);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    ph_fs_stat_t st;
    ph_fs_stat(ds_store, &st);
    TEST_ASSERT_FALSE(st.exists);

    free_flags(flags, 3);
    ph_free(ds_store);
    ph_free(deploy);
    ph_free(script);
    ph_free(script_dir);
}

/* ---- test: legacy strict mode (no warnings -> success) ---- */

void test_build_legacy_strict_mode(void) {
    char *script_dir = ph_path_join(test_dir, "scripts/_default");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(script_dir, 0755));

    char *script = ph_path_join(script_dir, "all.sh");
    write_file(script, "#!/bin/sh\nexit 0\n");
    chmod(script, 0755);

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("legacy-scripts", NULL);
    flags[2] = make_flag("strict", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    free_flags(flags, 3);
    ph_free(script);
    ph_free(script_dir);
}

/* ---- test: --clean-first removes deploy dir contents ---- */

void test_build_clean_first_removes_deploy(void) {
    char *src = ph_path_join(test_dir, "src");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(src, 0755));

    char *deploy = ph_path_join(test_dir, "deploy-output");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(deploy, 0755));

    char *marker = ph_path_join(deploy, "old-file.txt");
    write_file(marker, "should be removed by --clean-first");

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("clean-first", NULL);
    flags[2] = make_flag("deploy-at", "deploy-output");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 3,
    };

    /* will fail at esbuild step, but --clean-first runs first (step 5) */
    (void)ph_cmd_build(&phosphor_cli_config, &args);

    ph_fs_stat_t st;
    ph_fs_stat(marker, &st);
    TEST_ASSERT_FALSE(st.exists);

    free_flags(flags, 3);
    ph_free(marker);
    ph_free(deploy);
    ph_free(src);
}

/* ---- test: with src/ but no esbuild -> fails at process level ---- */

void test_build_with_src_no_esbuild(void) {
    char *src = ph_path_join(test_dir, "src");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(src, 0755));

    ph_parsed_flag_t flags[1];
    flags[0] = make_flag("project", test_dir);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 1,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    /* passes src/ check (step 3), fails at esbuild/npm step */
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NOT_EQUAL(PH_ERR_VALIDATE, rc);

    free_flags(flags, 1);
    ph_free(src);
}

/* ---- test: --tld flag affects deploy dir name ---- */

void test_build_tld_from_flag(void) {
    char *src = ph_path_join(test_dir, "src");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(src, 0755));

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("tld", ".test");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 2,
    };

    /* will fail at esbuild, but deploy dir is created at step 6 */
    (void)ph_cmd_build(&phosphor_cli_config, &args);

    const char *sni = getenv("SNI");
    if (!sni || !sni[0]) sni = "unknown";

    char expected[PATH_MAX];
    snprintf(expected, sizeof(expected), "%s/public/%s.test", test_dir, sni);

    ph_fs_stat_t st;
    ph_fs_stat(expected, &st);
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_dir);

    free_flags(flags, 2);
    ph_free(src);
}

/* ---- test: --normalize-eol is reserved, doesn't crash ---- */

void test_build_normalize_eol_reserved(void) {
    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("normalize-eol", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    /* no src/ -> PH_ERR_VALIDATE; flag was accepted without crash */
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, rc);

    free_flags(flags, 2);
}

/* ---- test: --verbose sets debug log level ---- */

void test_build_verbose_flag(void) {
    char *src = ph_path_join(test_dir, "src");
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(src, 0755));

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("project", test_dir);
    flags[1] = make_flag("verbose", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_BUILD,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_build(&phosphor_cli_config, &args);
    /* --verbose sets debug log level; still fails at esbuild step */
    TEST_ASSERT_NOT_EQUAL(0, rc);

    free_flags(flags, 2);
    ph_free(src);
}

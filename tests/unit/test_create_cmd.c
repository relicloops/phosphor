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
    snprintf(test_dir, sizeof(test_dir), "/tmp/ph_create_test_%d_%u",
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

static size_t read_file_buf(const char *path, uint8_t *buf, size_t bufsize) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    size_t n = fread(buf, 1, bufsize, fp);
    fclose(fp);
    return n;
}

/* ---- template fixture helpers ---- */

static void write_minimal_manifest(const char *tmpl_dir) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/template.phosphor.toml", tmpl_dir);
    write_file(path,
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test-minimal\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "name = \"test-minimal\"\n"
        "source_root = \"src\"\n"
        "\n"
        "[[variables]]\n"
        "name = \"name\"\n"
        "type = \"string\"\n"
        "required = true\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"copy\"\n"
        "from = \"hello.txt\"\n"
        "to = \"hello.txt\"\n"
    );
}

static void create_minimal_template(const char *tmpl_dir) {
    ph_fs_mkdir_p(tmpl_dir, 0755);
    write_minimal_manifest(tmpl_dir);

    char src_dir[PATH_MAX];
    snprintf(src_dir, sizeof(src_dir), "%s/src", tmpl_dir);
    ph_fs_mkdir_p(src_dir, 0755);

    char hello[PATH_MAX];
    snprintf(hello, sizeof(hello), "%s/hello.txt", src_dir);
    write_file(hello, "hello world\n");
}

/* ---- test: no --name flag -> PH_ERR_USAGE ---- */

void test_create_missing_name(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    create_minimal_template(tmpl);

    ph_parsed_flag_t flags[1];
    flags[0] = make_flag("template", tmpl);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 1,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, rc);

    free_flags(flags, 1);
}

/* ---- test: no --template flag -> PH_ERR_USAGE ---- */

void test_create_missing_template(void) {
    ph_parsed_flag_t flags[1];
    flags[0] = make_flag("name", "myproject");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 1,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, rc);

    free_flags(flags, 1);
}

/* ---- test: template path does not exist -> PH_ERR_FS ---- */

void test_create_template_not_found(void) {
    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", "/tmp/ph_nonexistent_template_99999");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_FS, rc);

    free_flags(flags, 2);
}

/* ---- test: template path is a file, not a directory -> PH_ERR_FS ---- */

void test_create_template_not_dir(void) {
    char tmpl_file[PATH_MAX];
    snprintf(tmpl_file, sizeof(tmpl_file), "%s/not-a-dir", test_dir);
    write_file(tmpl_file, "I am a file, not a directory");

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl_file);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_FS, rc);

    free_flags(flags, 2);
}

/* ---- test: template dir without manifest -> error ---- */

void test_create_manifest_missing(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/empty-template", test_dir);
    ph_fs_mkdir_p(tmpl, 0755);

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_NOT_EQUAL(0, rc);

    free_flags(flags, 2);
}

/* ---- test: invalid TOML in manifest -> PH_ERR_CONFIG ---- */

void test_create_manifest_invalid(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/bad-template", test_dir);
    ph_fs_mkdir_p(tmpl, 0755);

    char manifest[PATH_MAX];
    snprintf(manifest, sizeof(manifest),
             "%s/template.phosphor.toml", tmpl);
    write_file(manifest, "this is not valid TOML {{{}}}\n");

    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_CONFIG, rc);

    free_flags(flags, 2);
}

/* ---- test: --dry-run returns 0, no destination created ---- */

void test_create_dry_run(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    create_minimal_template(tmpl);

    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s/output", test_dir);

    ph_parsed_flag_t flags[4];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);
    flags[2] = make_flag("output", dest);
    flags[3] = make_flag("dry-run", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 4,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    ph_fs_stat_t st;
    ph_fs_stat(dest, &st);
    TEST_ASSERT_FALSE(st.exists);

    free_flags(flags, 4);
}

/* ---- test: minimal happy path -- mkdir + copy ---- */

void test_create_minimal_happy_path(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    create_minimal_template(tmpl);

    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s/output", test_dir);

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);
    flags[2] = make_flag("output", dest);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    /* verify destination exists */
    ph_fs_stat_t st;
    ph_fs_stat(dest, &st);
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_dir);

    /* verify hello.txt was copied */
    char *hello = ph_path_join(dest, "hello.txt");
    ph_fs_stat(hello, &st);
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_file);

    uint8_t buf[256];
    size_t n = read_file_buf(hello, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(12, n);
    TEST_ASSERT_EQUAL_MEMORY("hello world\n", buf, n);

    ph_free(hello);
    free_flags(flags, 3);
}

/* ---- test: destination exists, no --force -> PH_ERR_FS ---- */

void test_create_dest_exists_no_force(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    create_minimal_template(tmpl);

    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s/output", test_dir);
    ph_fs_mkdir_p(dest, 0755);

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);
    flags[2] = make_flag("output", dest);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(PH_ERR_FS, rc);

    free_flags(flags, 3);
}

/* ---- test: destination exists, --force bypasses preflight ---- */

void test_create_dest_exists_with_force(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    create_minimal_template(tmpl);

    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s/output", test_dir);
    ph_fs_mkdir_p(dest, 0755);

    ph_parsed_flag_t flags[4];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);
    flags[2] = make_flag("output", dest);
    flags[3] = make_flag("force", NULL);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 4,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    free_flags(flags, 4);
}

/* ---- test: --output flag changes destination directory ---- */

void test_create_output_flag(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    create_minimal_template(tmpl);

    char custom[PATH_MAX];
    snprintf(custom, sizeof(custom), "%s/custom-dir", test_dir);

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);
    flags[2] = make_flag("output", custom);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    ph_fs_stat_t st;
    ph_fs_stat(custom, &st);
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_dir);

    free_flags(flags, 3);
}

/* ---- test: render substitution replaces <<name>> ---- */

void test_create_render_substitution(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    ph_fs_mkdir_p(tmpl, 0755);

    char src_dir[PATH_MAX];
    snprintf(src_dir, sizeof(src_dir), "%s/src", tmpl);
    ph_fs_mkdir_p(src_dir, 0755);

    /* manifest with render op */
    char manifest[PATH_MAX];
    snprintf(manifest, sizeof(manifest),
             "%s/template.phosphor.toml", tmpl);
    write_file(manifest,
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test-render\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "name = \"test-render\"\n"
        "source_root = \"src\"\n"
        "\n"
        "[[variables]]\n"
        "name = \"name\"\n"
        "type = \"string\"\n"
        "required = true\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"render\"\n"
        "from = \"greeting.txt\"\n"
        "to = \"greeting.txt\"\n"
    );

    /* source file with placeholder */
    char greeting[PATH_MAX];
    snprintf(greeting, sizeof(greeting), "%s/greeting.txt", src_dir);
    write_file(greeting, "Hello <<name>>!\n");

    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s/output", test_dir);

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("name", "world");
    flags[1] = make_flag("template", tmpl);
    flags[2] = make_flag("output", dest);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    char *rendered = ph_path_join(dest, "greeting.txt");
    uint8_t buf[256];
    size_t n = read_file_buf(rendered, buf, sizeof(buf));

    const char *expected = "Hello world!\n";
    TEST_ASSERT_EQUAL(strlen(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, n);

    ph_free(rendered);
    free_flags(flags, 3);
}

/* ---- test: filter exclude glob skips matching files ---- */

void test_create_filter_exclude_glob(void) {
    char tmpl[PATH_MAX];
    snprintf(tmpl, sizeof(tmpl), "%s/template", test_dir);
    ph_fs_mkdir_p(tmpl, 0755);

    char src_dir[PATH_MAX];
    snprintf(src_dir, sizeof(src_dir), "%s/src", tmpl);
    ph_fs_mkdir_p(src_dir, 0755);

    /* manifest with directory copy + exclude filter */
    char manifest[PATH_MAX];
    snprintf(manifest, sizeof(manifest),
             "%s/template.phosphor.toml", tmpl);
    write_file(manifest,
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test-filter\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "name = \"test-filter\"\n"
        "source_root = \"src\"\n"
        "\n"
        "[[variables]]\n"
        "name = \"name\"\n"
        "type = \"string\"\n"
        "required = true\n"
        "\n"
        "[filters]\n"
        "exclude = [\"*.bak\"]\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"copy\"\n"
        "from = \".\"\n"
        "to = \".\"\n"
    );

    /* source files: one to keep, one to exclude */
    char keep[PATH_MAX], skip[PATH_MAX];
    snprintf(keep, sizeof(keep), "%s/keep.txt", src_dir);
    snprintf(skip, sizeof(skip), "%s/skip.bak", src_dir);
    write_file(keep, "keep me\n");
    write_file(skip, "skip me\n");

    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s/output", test_dir);

    ph_parsed_flag_t flags[3];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", tmpl);
    flags[2] = make_flag("output", dest);

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 3,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
    TEST_ASSERT_EQUAL(0, rc);

    ph_fs_stat_t st;
    char *keep_out = ph_path_join(dest, "keep.txt");
    char *skip_out = ph_path_join(dest, "skip.bak");

    ph_fs_stat(keep_out, &st);
    TEST_ASSERT_TRUE(st.exists);

    ph_fs_stat(skip_out, &st);
    TEST_ASSERT_FALSE(st.exists);

    ph_free(keep_out);
    ph_free(skip_out);
    free_flags(flags, 3);
}

/* ---- test: git URL without libgit2 -> PH_ERR_USAGE ---- */

void test_create_url_without_libgit2(void) {
    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", "https://github.com/user/repo");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
#ifndef PHOSPHOR_HAS_LIBGIT2
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, rc);
#else
    /* if libgit2 is enabled, this will fail at network level */
    TEST_ASSERT_NOT_EQUAL(0, rc);
#endif

    free_flags(flags, 2);
}

/* ---- test: archive path without libarchive -> PH_ERR_USAGE ---- */

void test_create_archive_without_libarchive(void) {
    ph_parsed_flag_t flags[2];
    flags[0] = make_flag("name", "myproject");
    flags[1] = make_flag("template", "/tmp/fake-template.tar.gz");

    ph_parsed_args_t args = {
        .command_id = PHOSPHOR_CMD_CREATE,
        .flags      = flags,
        .flag_count = 2,
    };

    int rc = ph_cmd_create(&phosphor_cli_config, &args);
#ifndef PHOSPHOR_HAS_LIBARCHIVE
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, rc);
#else
    /* if libarchive is enabled, this will fail at file not found */
    TEST_ASSERT_NOT_EQUAL(0, rc);
#endif

    free_flags(flags, 2);
}

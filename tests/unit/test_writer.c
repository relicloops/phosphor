#include "unity.h"
#include "phosphor/template.h"
#include "phosphor/render.h"
#include "phosphor/fs.h"
#include "phosphor/platform.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"
#include "phosphor/error.h"
#include "phosphor/regex.h"

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
    snprintf(test_dir, sizeof(test_dir), "/tmp/ph_writer_test_%d_%u",
             (int)getpid(), test_seq++);
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(test_dir, 0755));
}

void tearDown(void) {
    ph_error_t *err = NULL;
    ph_fs_rmtree(test_dir, &err);
    ph_error_destroy(err);
}

/* ---- helpers ---- */

static void write_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fputs(content, fp);
    fclose(fp);
}

static size_t read_file_buf(const char *path, char *buf, size_t bufsize) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    size_t n = fread(buf, 1, bufsize - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return n;
}

static ph_planned_op_t make_op(ph_op_kind_t kind) {
    ph_planned_op_t op;
    memset(&op, 0, sizeof(op));
    op.kind = kind;
    return op;
}

/* ==== NULL argument guards ==== */

void test_execute_null_plan(void) {
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_execute(NULL, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_execute_null_stats(void) {
    ph_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, NULL, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ==== empty plan ==== */

void test_execute_empty_plan(void) {
    ph_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(0, stats.files_copied);
    TEST_ASSERT_EQUAL(0, stats.files_rendered);
    TEST_ASSERT_EQUAL(0, stats.dirs_created);
    TEST_ASSERT_EQUAL(0, stats.bytes_written);
    TEST_ASSERT_EQUAL(0, stats.skipped);
}

/* ==== MKDIR ==== */

void test_execute_mkdir_basic(void) {
    char *dir_path = ph_path_join(test_dir, "subdir");

    ph_planned_op_t op = make_op(PH_OP_MKDIR);
    op.to_abs = dir_path;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(1, stats.dirs_created);

    ph_fs_stat_t st;
    ph_fs_stat(dir_path, &st);
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_dir);

    ph_free(dir_path);
}

void test_execute_mkdir_with_mode(void) {
    char *dir_path = ph_path_join(test_dir, "modedir");

    ph_planned_op_t op = make_op(PH_OP_MKDIR);
    op.to_abs = dir_path;
    op.mode = "0700";

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, stats.dirs_created);

    struct stat sb;
    TEST_ASSERT_EQUAL(0, stat(dir_path, &sb));
    TEST_ASSERT_EQUAL(0700, sb.st_mode & 0777);

    ph_free(dir_path);
}

void test_execute_mkdir_missing_to(void) {
    ph_planned_op_t op = make_op(PH_OP_MKDIR);
    op.to_abs = NULL;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ==== COPY ==== */

void test_execute_copy_file(void) {
    char *src = ph_path_join(test_dir, "source.txt");
    char *dst = ph_path_join(test_dir, "dest.txt");
    write_file(src, "hello world");

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src;
    op.to_abs = dst;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(1, stats.files_copied);
    TEST_ASSERT_TRUE(stats.bytes_written > 0);

    char buf[256];
    read_file_buf(dst, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("hello world", buf);

    ph_free(src);
    ph_free(dst);
}

void test_execute_copy_file_atomic(void) {
    char *src = ph_path_join(test_dir, "atomic_src.txt");
    char *dst = ph_path_join(test_dir, "atomic_dst.txt");
    write_file(src, "atomic content");

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src;
    op.to_abs = dst;
    op.atomic = true;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, stats.files_copied);

    char buf[256];
    read_file_buf(dst, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("atomic content", buf);

    ph_free(src);
    ph_free(dst);
}

void test_execute_copy_source_missing(void) {
    char *src = ph_path_join(test_dir, "nonexistent.txt");
    char *dst = ph_path_join(test_dir, "dest.txt");

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src;
    op.to_abs = dst;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);

    ph_free(src);
    ph_free(dst);
}

void test_execute_copy_missing_fields(void) {
    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = NULL;
    op.to_abs = NULL;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_execute_copy_directory(void) {
    /* create source dir with a file */
    char *src_dir = ph_path_join(test_dir, "srcdir");
    ph_fs_mkdir_p(src_dir, 0755);
    char *src_file = ph_path_join(src_dir, "file.txt");
    write_file(src_file, "dir copy content");

    char *dst_dir = ph_path_join(test_dir, "dstdir");

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src_dir;
    op.to_abs = dst_dir;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(1, stats.files_copied);

    /* verify the file was copied */
    char *copied = ph_path_join(dst_dir, "file.txt");
    ph_fs_stat_t st;
    ph_fs_stat(copied, &st);
    TEST_ASSERT_TRUE(st.exists);
    TEST_ASSERT_TRUE(st.is_file);

    ph_free(src_dir);
    ph_free(src_file);
    ph_free(dst_dir);
    ph_free(copied);
}

/* ==== RENDER ==== */

void test_execute_render_text(void) {
    char *src = ph_path_join(test_dir, "tmpl.txt");
    char *dst = ph_path_join(test_dir, "out.txt");
    write_file(src, "Hello <<name>>!");

    ph_resolved_var_t vars[1];
    vars[0].name = "name";
    vars[0].value = "World";
    vars[0].type = PH_VAR_STRING;

    ph_planned_op_t op = make_op(PH_OP_RENDER);
    op.from_abs = src;
    op.to_abs = dst;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, vars, 1, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(1, stats.files_rendered);

    char buf[256];
    read_file_buf(dst, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Hello World!", buf);

    ph_free(src);
    ph_free(dst);
}

void test_execute_render_binary_passthrough(void) {
    char *src = ph_path_join(test_dir, "bin.dat");
    char *dst = ph_path_join(test_dir, "out.dat");
    /* binary content with NUL bytes */
    FILE *fp = fopen(src, "wb");
    TEST_ASSERT_NOT_NULL(fp);
    uint8_t data[] = {0x00, 0x01, 0xFF, 0xFE, 0x00};
    fwrite(data, 1, sizeof(data), fp);
    fclose(fp);

    ph_planned_op_t op = make_op(PH_OP_RENDER);
    op.from_abs = src;
    op.to_abs = dst;
    op.is_binary = true;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, stats.files_rendered);

    /* verify output = input (no rendering) */
    uint8_t *out_data = NULL;
    size_t out_len = 0;
    ph_fs_read_file(dst, &out_data, &out_len);
    TEST_ASSERT_EQUAL(sizeof(data), out_len);
    TEST_ASSERT_EQUAL_MEMORY(data, out_data, sizeof(data));
    ph_free(out_data);

    ph_free(src);
    ph_free(dst);
}

void test_execute_render_missing_fields(void) {
    ph_planned_op_t op = make_op(PH_OP_RENDER);
    op.from_abs = NULL;
    op.to_abs = NULL;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ==== CHMOD ==== */

void test_execute_chmod(void) {
    char *file = ph_path_join(test_dir, "chmod_target.sh");
    write_file(file, "#!/bin/sh\necho hi");

    ph_planned_op_t op = make_op(PH_OP_CHMOD);
    op.from_abs = file;
    op.mode = "0755";

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);

    struct stat sb;
    stat(file, &sb);
    TEST_ASSERT_EQUAL(0755, sb.st_mode & 0777);

    ph_free(file);
}

void test_execute_chmod_missing_fields(void) {
    ph_planned_op_t op = make_op(PH_OP_CHMOD);
    op.from_abs = NULL;
    op.mode = NULL;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ==== REMOVE ==== */

void test_execute_remove(void) {
    char *dir = ph_path_join(test_dir, "removeme");
    ph_fs_mkdir_p(dir, 0755);
    char *file = ph_path_join(dir, "file.txt");
    write_file(file, "delete me");

    ph_planned_op_t op = make_op(PH_OP_REMOVE);
    op.to_abs = dir;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);

    ph_fs_stat_t st;
    ph_fs_stat(dir, &st);
    TEST_ASSERT_FALSE(st.exists);

    ph_free(dir);
    ph_free(file);
}

void test_execute_remove_via_from(void) {
    char *dir = ph_path_join(test_dir, "removefrom");
    ph_fs_mkdir_p(dir, 0755);

    ph_planned_op_t op = make_op(PH_OP_REMOVE);
    op.from_abs = dir;
    op.to_abs = NULL;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);

    ph_fs_stat_t st;
    ph_fs_stat(dir, &st);
    TEST_ASSERT_FALSE(st.exists);

    ph_free(dir);
}

/* ==== skip ==== */

void test_execute_skip_op(void) {
    ph_planned_op_t op = make_op(PH_OP_MKDIR);
    op.to_abs = NULL; /* would fail if not skipped */
    op.skip = true;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, NULL, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, stats.skipped);
}

/* ==== filters ==== */

void test_execute_filter_exclude_glob(void) {
    /* create source dir with two files */
    char *src_dir = ph_path_join(test_dir, "filter_src");
    ph_fs_mkdir_p(src_dir, 0755);
    char *keep = ph_path_join(src_dir, "keep.txt");
    char *skip = ph_path_join(src_dir, "skip.bak");
    write_file(keep, "keep me");
    write_file(skip, "skip me");

    char *dst_dir = ph_path_join(test_dir, "filter_dst");

    /* set up filters */
    char *excludes[] = { "*.bak" };
    ph_filters_t filters;
    memset(&filters, 0, sizeof(filters));
    filters.exclude = excludes;
    filters.exclude_count = 1;

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src_dir;
    op.to_abs = dst_dir;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, &filters, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NULL(err);

    /* keep.txt should exist, skip.bak should not */
    char *dst_keep = ph_path_join(dst_dir, "keep.txt");
    char *dst_skip = ph_path_join(dst_dir, "skip.bak");
    ph_fs_stat_t st;
    ph_fs_stat(dst_keep, &st);
    TEST_ASSERT_TRUE(st.exists);
    ph_fs_stat(dst_skip, &st);
    TEST_ASSERT_FALSE(st.exists);

    ph_free(src_dir);
    ph_free(keep);
    ph_free(skip);
    ph_free(dst_dir);
    ph_free(dst_keep);
    ph_free(dst_skip);
}

void test_execute_filter_deny_glob(void) {
    char *src = ph_path_join(test_dir, "denied.env");
    char *dst = ph_path_join(test_dir, "out.env");
    write_file(src, "SECRET=123");

    char *denies[] = { "*.env" };
    ph_filters_t filters;
    memset(&filters, 0, sizeof(filters));
    filters.deny = denies;
    filters.deny_count = 1;

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src;
    op.to_abs = dst;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, &filters, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, err->category);
    ph_error_destroy(err);

    ph_free(src);
    ph_free(dst);
}

void test_execute_filter_metadata_deny(void) {
    /* create source dir with .DS_Store */
    char *src_dir = ph_path_join(test_dir, "meta_src");
    ph_fs_mkdir_p(src_dir, 0755);
    char *normal = ph_path_join(src_dir, "normal.txt");
    char *dsstore = ph_path_join(src_dir, ".DS_Store");
    write_file(normal, "keep");
    write_file(dsstore, "metadata");

    char *dst_dir = ph_path_join(test_dir, "meta_dst");

    ph_filters_t filters;
    memset(&filters, 0, sizeof(filters));

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src_dir;
    op.to_abs = dst_dir;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, &filters, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);

    char *dst_normal = ph_path_join(dst_dir, "normal.txt");
    char *dst_ds = ph_path_join(dst_dir, ".DS_Store");
    ph_fs_stat_t st;
    ph_fs_stat(dst_normal, &st);
    TEST_ASSERT_TRUE(st.exists);
    ph_fs_stat(dst_ds, &st);
    TEST_ASSERT_FALSE(st.exists);

    ph_free(src_dir);
    ph_free(normal);
    ph_free(dsstore);
    ph_free(dst_dir);
    ph_free(dst_normal);
    ph_free(dst_ds);
}

/* ==== pcre2 filter tests ==== */

void test_execute_filter_exclude_regex(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    char *src_dir = ph_path_join(test_dir, "re_src");
    ph_fs_mkdir_p(src_dir, 0755);
    char *keep = ph_path_join(src_dir, "main.c");
    char *skip = ph_path_join(src_dir, "test_main.c");
    write_file(keep, "keep");
    write_file(skip, "skip");

    char *dst_dir = ph_path_join(test_dir, "re_dst");

    char *re_excludes[] = { "^test_" };
    ph_filters_t filters;
    memset(&filters, 0, sizeof(filters));
    filters.exclude_regex = re_excludes;
    filters.exclude_regex_count = 1;

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src_dir;
    op.to_abs = dst_dir;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, &filters, &stats, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);

    char *dst_keep = ph_path_join(dst_dir, "main.c");
    char *dst_skip = ph_path_join(dst_dir, "test_main.c");
    ph_fs_stat_t st;
    ph_fs_stat(dst_keep, &st);
    TEST_ASSERT_TRUE(st.exists);
    ph_fs_stat(dst_skip, &st);
    TEST_ASSERT_FALSE(st.exists);

    ph_free(src_dir);
    ph_free(keep);
    ph_free(skip);
    ph_free(dst_dir);
    ph_free(dst_keep);
    ph_free(dst_skip);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_execute_filter_deny_regex(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    char *src = ph_path_join(test_dir, "secret.key");
    char *dst = ph_path_join(test_dir, "out.key");
    write_file(src, "private key");

    char *re_denies[] = { "\\.key$" };
    ph_filters_t filters;
    memset(&filters, 0, sizeof(filters));
    filters.deny_regex = re_denies;
    filters.deny_regex_count = 1;

    ph_planned_op_t op = make_op(PH_OP_COPY);
    op.from_abs = src;
    op.to_abs = dst;

    ph_plan_t plan = { .ops = &op, .count = 1, .cap = 1 };
    ph_plan_stats_t stats;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_plan_execute(&plan, NULL, 0, &filters, &stats, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_VALIDATE, err->category);
    ph_error_destroy(err);

    ph_free(src);
    ph_free(dst);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

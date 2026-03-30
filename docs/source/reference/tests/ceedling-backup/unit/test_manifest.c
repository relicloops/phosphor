#include "unity.h"
#include "phosphor/manifest.h"
#include "phosphor/platform.h"
#include "phosphor/alloc.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

TEST_SOURCE_FILE("src/template/manifest_load.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

static char tmpdir[256];
static char tmpfile_path[512];
static int test_counter = 0;

void setUp(void) {
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/ph_test_manifest_%d_%d",
             (int)getpid(), test_counter++);
    ph_fs_mkdir_p(tmpdir, 0755);
    snprintf(tmpfile_path, sizeof(tmpfile_path),
             "%s/template.phosphor.toml", tmpdir);
}

void tearDown(void) {
    unlink(tmpfile_path);
    rmdir(tmpdir);
}

static void write_toml(const char *content) {
    FILE *f = fopen(tmpfile_path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(content, f);
    fclose(f);
}

/* ---- type helpers ---- */

void test_var_type_roundtrip(void) {
    TEST_ASSERT_EQUAL_STRING("string", ph_var_type_name(PH_VAR_STRING));
    TEST_ASSERT_EQUAL_STRING("bool", ph_var_type_name(PH_VAR_BOOL));
    TEST_ASSERT_EQUAL_STRING("int", ph_var_type_name(PH_VAR_INT));
    TEST_ASSERT_EQUAL_STRING("enum", ph_var_type_name(PH_VAR_ENUM));
    TEST_ASSERT_EQUAL_STRING("path", ph_var_type_name(PH_VAR_PATH));
    TEST_ASSERT_EQUAL_STRING("url", ph_var_type_name(PH_VAR_URL));
}

void test_var_type_from_str(void) {
    TEST_ASSERT_EQUAL(PH_VAR_STRING, ph_var_type_from_str("string"));
    TEST_ASSERT_EQUAL(PH_VAR_INT, ph_var_type_from_str("int"));
    TEST_ASSERT_EQUAL(PH_VAR_BOOL, ph_var_type_from_str("bool"));
    TEST_ASSERT_EQUAL(PH_VAR_ENUM, ph_var_type_from_str("enum"));
    TEST_ASSERT_EQUAL(PH_VAR_STRING, ph_var_type_from_str("unknown"));
    TEST_ASSERT_EQUAL(PH_VAR_STRING, ph_var_type_from_str(NULL));
}

void test_op_kind_roundtrip(void) {
    TEST_ASSERT_EQUAL_STRING("mkdir", ph_op_kind_name(PH_OP_MKDIR));
    TEST_ASSERT_EQUAL_STRING("copy", ph_op_kind_name(PH_OP_COPY));
    TEST_ASSERT_EQUAL_STRING("render", ph_op_kind_name(PH_OP_RENDER));
    TEST_ASSERT_EQUAL_STRING("chmod", ph_op_kind_name(PH_OP_CHMOD));
    TEST_ASSERT_EQUAL_STRING("remove", ph_op_kind_name(PH_OP_REMOVE));
}

void test_op_kind_from_str(void) {
    ph_op_kind_t k;
    TEST_ASSERT_EQUAL(PH_OK, ph_op_kind_from_str("mkdir", &k));
    TEST_ASSERT_EQUAL(PH_OP_MKDIR, k);
    TEST_ASSERT_EQUAL(PH_OK, ph_op_kind_from_str("render", &k));
    TEST_ASSERT_EQUAL(PH_OP_RENDER, k);
    TEST_ASSERT_EQUAL(PH_ERR, ph_op_kind_from_str("invalid", &k));
    TEST_ASSERT_EQUAL(PH_ERR, ph_op_kind_from_str(NULL, &k));
}

/* ---- manifest loading ---- */

void test_load_minimal(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \"src\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"output\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, m.manifest.schema);
    TEST_ASSERT_EQUAL_STRING("test", m.manifest.id);
    TEST_ASSERT_EQUAL_STRING("1.0.0", m.manifest.version);
    TEST_ASSERT_EQUAL_STRING("src", m.tmpl.source_root);
    TEST_ASSERT_EQUAL(1, m.op_count);
    TEST_ASSERT_EQUAL(PH_OP_MKDIR, m.ops[0].kind);
    TEST_ASSERT_EQUAL_STRING("output", m.ops[0].to);
    ph_manifest_destroy(&m);
}

void test_load_with_variables(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"vars-test\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[variables]]\n"
        "name = \"project_name\"\n"
        "type = \"string\"\n"
        "required = true\n"
        "\n"
        "[[variables]]\n"
        "name = \"license\"\n"
        "type = \"enum\"\n"
        "default = \"MIT\"\n"
        "choices = [\"MIT\", \"Apache-2.0\", \"GPL-3.0\"]\n"
        "\n"
        "[[ops]]\n"
        "kind = \"render\"\n"
        "from = \"README.md\"\n"
        "to = \"README.md\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(2, m.variable_count);
    TEST_ASSERT_EQUAL_STRING("project_name", m.variables[0].name);
    TEST_ASSERT_EQUAL(PH_VAR_STRING, m.variables[0].type);
    TEST_ASSERT_TRUE(m.variables[0].required);
    TEST_ASSERT_EQUAL_STRING("license", m.variables[1].name);
    TEST_ASSERT_EQUAL(PH_VAR_ENUM, m.variables[1].type);
    TEST_ASSERT_EQUAL(3, m.variables[1].choice_count);
    TEST_ASSERT_EQUAL_STRING("MIT", m.variables[1].default_val);
    ph_manifest_destroy(&m);
}

void test_load_with_filters(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"filter-test\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[filters]\n"
        "exclude = [\"*.bak\", \".git\"]\n"
        "binary_ext = [\".png\", \".jpg\"]\n"
        "\n"
        "[[ops]]\n"
        "kind = \"copy\"\n"
        "from = \".\"\n"
        "to = \".\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(2, m.filters.exclude_count);
    TEST_ASSERT_EQUAL_STRING("*.bak", m.filters.exclude[0]);
    TEST_ASSERT_EQUAL(2, m.filters.binary_ext_count);
    ph_manifest_destroy(&m);
}

void test_load_with_hooks(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"hook-test\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[hooks]]\n"
        "when = \"post-create\"\n"
        "run = [\"npm\", \"install\"]\n"
        "allow_failure = true\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, m.hook_count);
    TEST_ASSERT_EQUAL_STRING("post-create", m.hooks[0].when);
    TEST_ASSERT_EQUAL(2, m.hooks[0].run_count);
    TEST_ASSERT_EQUAL_STRING("npm", m.hooks[0].run[0]);
    TEST_ASSERT_TRUE(m.hooks[0].allow_failure);
    ph_manifest_destroy(&m);
}

void test_load_with_defaults(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"def-test\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[defaults]\n"
        "author = \"Alice\"\n"
        "year = \"2026\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(2, m.defaults.count);
    ph_manifest_destroy(&m);
}

void test_load_multiple_ops(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"multi\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"build\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"copy\"\n"
        "from = \"src\"\n"
        "to = \"build/src\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"render\"\n"
        "from = \"config.toml\"\n"
        "to = \"build/config.toml\"\n"
        "atomic = true\n"
        "newline = \"lf\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(3, m.op_count);
    TEST_ASSERT_EQUAL(PH_OP_MKDIR, m.ops[0].kind);
    TEST_ASSERT_EQUAL(PH_OP_COPY, m.ops[1].kind);
    TEST_ASSERT_EQUAL(PH_OP_RENDER, m.ops[2].kind);
    TEST_ASSERT_TRUE(m.ops[2].atomic);
    TEST_ASSERT_EQUAL_STRING("lf", m.ops[2].newline);
    ph_manifest_destroy(&m);
}

/* ---- error cases ---- */

void test_load_missing_manifest_section(void) {
    write_toml(
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_load_missing_schema(void) {
    write_toml(
        "[manifest]\n"
        "id = \"test\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

void test_load_missing_source_root(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "name = \"Test\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

void test_load_missing_ops(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

void test_load_invalid_op_kind(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test\"\n"
        "version = \"1.0.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"invalid_kind\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

void test_load_nonexistent_file(void) {
    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load("/tmp/nonexistent_ph_manifest.toml",
                                       &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

void test_load_invalid_toml(void) {
    write_toml("invalid = [[[broken syntax\n");

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

void test_load_null_args(void) {
    ph_manifest_t m;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_manifest_load(NULL, &m, &err));
    ph_error_destroy(err);

    err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_manifest_load("test", NULL, &err));
    ph_error_destroy(err);
}

void test_load_op_with_condition(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"cond\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"render\"\n"
        "from = \"ci.yml\"\n"
        "to = \".github/ci.yml\"\n"
        "condition = \"var.use_ci == \\\"true\\\"\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, m.op_count);
    TEST_ASSERT_NOT_NULL(m.ops[0].condition);
    ph_manifest_destroy(&m);
}

/* ---- [build] section tests ---- */

void test_load_no_build_section(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"no-build\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_FALSE(m.build.present);
    TEST_ASSERT_NULL(m.build.entry);
    TEST_ASSERT_EQUAL(0, m.build.define_count);
    ph_manifest_destroy(&m);
}

void test_load_build_entry_only(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"build-entry\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[build]\n"
        "entry = \"src/main.tsx\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_TRUE(m.build.present);
    TEST_ASSERT_EQUAL_STRING("src/main.tsx", m.build.entry);
    TEST_ASSERT_EQUAL(0, m.build.define_count);
    ph_manifest_destroy(&m);
}

void test_load_build_with_defines(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"build-defines\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[build]\n"
        "entry = \"src/app.tsx\"\n"
        "\n"
        "[[build.defines]]\n"
        "name = \"__DEV__\"\n"
        "env = \"PROJECT_DEV\"\n"
        "default = \"false\"\n"
        "\n"
        "[[build.defines]]\n"
        "name = \"__PUBLIC_DIR__\"\n"
        "env = \"PROJECT_PUBLIC_DIR\"\n"
        "default = \"\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_TRUE(m.build.present);
    TEST_ASSERT_EQUAL_STRING("src/app.tsx", m.build.entry);
    TEST_ASSERT_EQUAL(2, m.build.define_count);

    TEST_ASSERT_EQUAL_STRING("__DEV__", m.build.defines[0].name);
    TEST_ASSERT_EQUAL_STRING("PROJECT_DEV", m.build.defines[0].env);
    TEST_ASSERT_EQUAL_STRING("false", m.build.defines[0].default_val);

    TEST_ASSERT_EQUAL_STRING("__PUBLIC_DIR__", m.build.defines[1].name);
    TEST_ASSERT_EQUAL_STRING("PROJECT_PUBLIC_DIR", m.build.defines[1].env);
    TEST_ASSERT_EQUAL_STRING("", m.build.defines[1].default_val);

    ph_manifest_destroy(&m);
}

void test_load_build_define_name_only(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"build-name-only\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[build]\n"
        "\n"
        "[[build.defines]]\n"
        "name = \"__FEATURE_X__\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_TRUE(m.build.present);
    TEST_ASSERT_NULL(m.build.entry);
    TEST_ASSERT_EQUAL(1, m.build.define_count);
    TEST_ASSERT_EQUAL_STRING("__FEATURE_X__", m.build.defines[0].name);
    TEST_ASSERT_NULL(m.build.defines[0].env);
    TEST_ASSERT_NULL(m.build.defines[0].default_val);
    ph_manifest_destroy(&m);
}

void test_load_build_define_missing_name(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"build-no-name\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[build]\n"
        "\n"
        "[[build.defines]]\n"
        "env = \"SOME_ENV\"\n"
        "default = \"x\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

void test_load_build_empty_section(void) {
    write_toml(
        "[manifest]\n"
        "schema = 1\n"
        "id = \"build-empty\"\n"
        "version = \"0.1.0\"\n"
        "\n"
        "[template]\n"
        "source_root = \".\"\n"
        "\n"
        "[build]\n"
        "\n"
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"out\"\n"
    );

    ph_manifest_t m;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_manifest_load(tmpfile_path, &m, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_TRUE(m.build.present);
    TEST_ASSERT_NULL(m.build.entry);
    TEST_ASSERT_EQUAL(0, m.build.define_count);
    ph_manifest_destroy(&m);
}

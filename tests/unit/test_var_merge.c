#include "unity.h"
#include "phosphor/template.h"
#include "phosphor/manifest.h"
#include "phosphor/alloc.h"

#include <string.h>
#include <stdlib.h>

TEST_SOURCE_FILE("src/template/var_merge.c")
TEST_SOURCE_FILE("src/args-parser/args_helpers.c")
TEST_SOURCE_FILE("src/template/manifest_load.c")
TEST_SOURCE_FILE("src/core/config.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- helpers ---- */

static ph_var_def_t make_var_def(const char *name, ph_var_type_t type,
                                  bool required, const char *def_val) {
    ph_var_def_t v;
    memset(&v, 0, sizeof(v));
    v.name = (char *)name;
    v.type = type;
    v.required = required;
    v.default_val = (char *)def_val;
    return v;
}

static ph_parsed_flag_t make_flag(const char *name, const char *value) {
    ph_parsed_flag_t f;
    memset(&f, 0, sizeof(f));
    f.kind = PH_FLAG_VALUED;
    f.name = (char *)name;
    f.value = (char *)value;
    return f;
}

/* ---- resolved_var_get ---- */

void test_var_get_found(void) {
    ph_resolved_var_t vars[2];
    vars[0].name = (char *)"foo";
    vars[0].value = (char *)"bar";
    vars[1].name = (char *)"baz";
    vars[1].value = (char *)"qux";

    TEST_ASSERT_EQUAL_STRING("bar", ph_resolved_var_get(vars, 2, "foo"));
    TEST_ASSERT_EQUAL_STRING("qux", ph_resolved_var_get(vars, 2, "baz"));
}

void test_var_get_not_found(void) {
    ph_resolved_var_t vars[1];
    vars[0].name = (char *)"foo";
    vars[0].value = (char *)"bar";

    TEST_ASSERT_NULL(ph_resolved_var_get(vars, 1, "missing"));
}

void test_var_get_null(void) {
    TEST_ASSERT_NULL(ph_resolved_var_get(NULL, 0, "foo"));
    ph_resolved_var_t v;
    v.name = (char *)"x";
    v.value = (char *)"y";
    TEST_ASSERT_NULL(ph_resolved_var_get(&v, 1, NULL));
}

/* ---- merge: default values ---- */

void test_merge_default_value(void) {
    ph_var_def_t defs[] = {
        make_var_def("name", PH_VAR_STRING, false, "default_name"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = {
        .manifest = &manifest,
        .args = NULL,
        .cli_config = NULL,
        .config = NULL,
    };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("name", vars[0].name);
    TEST_ASSERT_EQUAL_STRING("default_name", vars[0].value);
    ph_resolved_vars_destroy(vars, count);
}

/* ---- merge: CLI flag overrides default ---- */

void test_merge_cli_overrides_default(void) {
    ph_var_def_t defs[] = {
        make_var_def("name", PH_VAR_STRING, false, "default_name"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_parsed_flag_t flags[] = {
        make_flag("name", "cli_name"),
    };
    ph_parsed_args_t args;
    memset(&args, 0, sizeof(args));
    args.flags = flags;
    args.flag_count = 1;

    ph_var_merge_ctx_t ctx = {
        .manifest = &manifest,
        .args = &args,
        .cli_config = NULL,
        .config = NULL,
    };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING("cli_name", vars[0].value);
    ph_resolved_vars_destroy(vars, count);
}

/* ---- merge: env var precedence ---- */

void test_merge_env_overrides_default(void) {
    ph_var_def_t defs[] = {
        make_var_def("name", PH_VAR_STRING, false, "default_name"),
    };
    defs[0].env = (char *)"PH_TEST_NAME_ENV_VAR";

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    setenv("PH_TEST_NAME_ENV_VAR", "env_name", 1);

    ph_var_merge_ctx_t ctx = {
        .manifest = &manifest,
        .args = NULL,
        .cli_config = NULL,
        .config = NULL,
    };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING("env_name", vars[0].value);
    ph_resolved_vars_destroy(vars, count);
    unsetenv("PH_TEST_NAME_ENV_VAR");
}

/* ---- merge: required variable missing ---- */

void test_merge_required_missing(void) {
    ph_var_def_t defs[] = {
        make_var_def("name", PH_VAR_STRING, true, NULL),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = {
        .manifest = &manifest,
        .args = NULL,
        .cli_config = NULL,
        .config = NULL,
    };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ---- validation: enum ---- */

void test_validate_enum_valid(void) {
    static const char *choices_arr[] = { "MIT", "Apache-2.0" };
    ph_var_def_t defs[1];
    memset(defs, 0, sizeof(defs));
    defs[0].name = (char *)"license";
    defs[0].type = PH_VAR_ENUM;
    defs[0].required = false;
    defs[0].default_val = (char *)"MIT";
    defs[0].choices = (char **)choices_arr;
    defs[0].choice_count = 2;

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = {
        .manifest = &manifest,
        .args = NULL,
        .cli_config = NULL,
        .config = NULL,
    };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING("MIT", vars[0].value);
    ph_resolved_vars_destroy(vars, count);
}

void test_validate_enum_invalid(void) {
    static const char *choices_arr[] = { "MIT", "Apache-2.0" };
    ph_var_def_t defs[1];
    memset(defs, 0, sizeof(defs));
    defs[0].name = (char *)"license";
    defs[0].type = PH_VAR_ENUM;
    defs[0].required = false;
    defs[0].default_val = (char *)"GPL";
    defs[0].choices = (char **)choices_arr;
    defs[0].choice_count = 2;

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = {
        .manifest = &manifest,
        .args = NULL,
        .cli_config = NULL,
        .config = NULL,
    };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

/* ---- validation: int bounds ---- */

void test_validate_int_in_range(void) {
    ph_var_def_t defs[1];
    memset(defs, 0, sizeof(defs));
    defs[0].name = (char *)"port";
    defs[0].type = PH_VAR_INT;
    defs[0].default_val = (char *)"8080";
    defs[0].has_min = true;
    defs[0].min = 1;
    defs[0].has_max = true;
    defs[0].max = 65535;

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = { .manifest = &manifest };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    ph_resolved_vars_destroy(vars, count);
}

void test_validate_int_out_of_range(void) {
    ph_var_def_t defs[1];
    memset(defs, 0, sizeof(defs));
    defs[0].name = (char *)"port";
    defs[0].type = PH_VAR_INT;
    defs[0].default_val = (char *)"99999";
    defs[0].has_max = true;
    defs[0].max = 65535;

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = { .manifest = &manifest };

    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    ph_result_t rc = ph_var_merge(&ctx, &vars, &count, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    ph_error_destroy(err);
}

/* ---- validation: URL ---- */

void test_validate_url_valid(void) {
    ph_var_def_t defs[] = {
        make_var_def("repo", PH_VAR_URL, false, "https://github.com/test"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = { .manifest = &manifest };
    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_var_merge(&ctx, &vars, &count, &err));
    ph_resolved_vars_destroy(vars, count);
}

void test_validate_url_invalid(void) {
    ph_var_def_t defs[] = {
        make_var_def("repo", PH_VAR_URL, false, "ftp://example.com"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = { .manifest = &manifest };
    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_var_merge(&ctx, &vars, &count, &err));
    ph_error_destroy(err);
}

/* ---- validation: path traversal ---- */

void test_validate_path_safe(void) {
    ph_var_def_t defs[] = {
        make_var_def("dir", PH_VAR_PATH, false, "output/build"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = { .manifest = &manifest };
    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_var_merge(&ctx, &vars, &count, &err));
    ph_resolved_vars_destroy(vars, count);
}

void test_validate_path_traversal(void) {
    ph_var_def_t defs[] = {
        make_var_def("dir", PH_VAR_PATH, false, "../../../etc/passwd"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.variables = defs;
    manifest.variable_count = 1;

    ph_var_merge_ctx_t ctx = { .manifest = &manifest };
    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_var_merge(&ctx, &vars, &count, &err));
    ph_error_destroy(err);
}

/* ---- merge: zero variables ---- */

void test_merge_zero_variables(void) {
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));

    ph_var_merge_ctx_t ctx = { .manifest = &manifest };
    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_var_merge(&ctx, &vars, &count, &err));
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(vars);
}

/* ---- merge: null ctx ---- */

void test_merge_null_ctx(void) {
    ph_resolved_var_t *vars = NULL;
    size_t count = 0;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_var_merge(NULL, &vars, &count, &err));
    ph_error_destroy(err);
}

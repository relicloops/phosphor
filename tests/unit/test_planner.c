#include "unity.h"
#include "phosphor/template.h"
#include "phosphor/manifest.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/template/planner.c")
TEST_SOURCE_FILE("src/template/renderer.c")
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

static ph_op_def_t make_op(ph_op_kind_t kind, const char *from,
                            const char *to) {
    ph_op_def_t op;
    memset(&op, 0, sizeof(op));
    op.kind = kind;
    op.from = (char *)from;
    op.to = (char *)to;
    return op;
}

/* ---- basic plan build ---- */

void test_plan_build_mkdir(void) {
    ph_op_def_t ops[] = {
        make_op(PH_OP_MKDIR, NULL, "output"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 1;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, NULL, 0,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, plan.count);
    TEST_ASSERT_EQUAL(PH_OP_MKDIR, plan.ops[0].kind);
    TEST_ASSERT_NOT_NULL(plan.ops[0].to_abs);
    ph_plan_destroy(&plan);
}

void test_plan_build_copy(void) {
    ph_op_def_t ops[] = {
        make_op(PH_OP_COPY, "src/main.c", "main.c"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 1;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, NULL, 0,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, plan.count);
    TEST_ASSERT_NOT_NULL(plan.ops[0].from_abs);
    TEST_ASSERT_NOT_NULL(plan.ops[0].to_abs);
    TEST_ASSERT_EQUAL_STRING("/tmpl/src/main.c", plan.ops[0].from_abs);
    TEST_ASSERT_EQUAL_STRING("/dest/main.c", plan.ops[0].to_abs);
    ph_plan_destroy(&plan);
}

void test_plan_build_multiple_ops(void) {
    ph_op_def_t ops[] = {
        make_op(PH_OP_MKDIR, NULL, "build"),
        make_op(PH_OP_COPY, "README.md", "build/README.md"),
        make_op(PH_OP_RENDER, "config.toml", "build/config.toml"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 3;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, NULL, 0,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(3, plan.count);
    TEST_ASSERT_EQUAL(PH_OP_MKDIR, plan.ops[0].kind);
    TEST_ASSERT_EQUAL(PH_OP_COPY, plan.ops[1].kind);
    TEST_ASSERT_EQUAL(PH_OP_RENDER, plan.ops[2].kind);
    ph_plan_destroy(&plan);
}

/* ---- variable substitution in paths ---- */

void test_plan_path_variable_substitution(void) {
    ph_op_def_t ops[] = {
        make_op(PH_OP_MKDIR, NULL, "<<name>>"),
    };
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 1;

    ph_resolved_var_t vars[1];
    vars[0].name = (char *)"name";
    vars[0].value = (char *)"my-project";
    vars[0].type = PH_VAR_STRING;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, vars, 1,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL_STRING("/dest/my-project", plan.ops[0].to_abs);
    ph_plan_destroy(&plan);
}

/* ---- condition evaluation ---- */

void test_plan_condition_true(void) {
    ph_op_def_t ops[1];
    memset(ops, 0, sizeof(ops));
    ops[0].kind = PH_OP_MKDIR;
    ops[0].to = (char *)"output";
    ops[0].condition = (char *)"var.enabled";

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 1;

    ph_resolved_var_t vars[1];
    vars[0].name = (char *)"enabled";
    vars[0].value = (char *)"true";
    vars[0].type = PH_VAR_STRING;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, vars, 1,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_FALSE(plan.ops[0].skip);
    ph_plan_destroy(&plan);
}

void test_plan_condition_false(void) {
    ph_op_def_t ops[1];
    memset(ops, 0, sizeof(ops));
    ops[0].kind = PH_OP_MKDIR;
    ops[0].to = (char *)"output";
    ops[0].condition = (char *)"var.missing_var";

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 1;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, NULL, 0,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_TRUE(plan.ops[0].skip);
    ph_plan_destroy(&plan);
}

void test_plan_condition_eq(void) {
    ph_op_def_t ops[1];
    memset(ops, 0, sizeof(ops));
    ops[0].kind = PH_OP_MKDIR;
    ops[0].to = (char *)"ci";
    ops[0].condition = (char *)"var.ci == \"github\"";

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 1;

    ph_resolved_var_t vars[1];
    vars[0].name = (char *)"ci";
    vars[0].value = (char *)"github";
    vars[0].type = PH_VAR_STRING;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, vars, 1,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_FALSE(plan.ops[0].skip);
    ph_plan_destroy(&plan);
}

void test_plan_condition_neq(void) {
    ph_op_def_t ops[1];
    memset(ops, 0, sizeof(ops));
    ops[0].kind = PH_OP_MKDIR;
    ops[0].to = (char *)"ci";
    ops[0].condition = (char *)"var.ci != \"gitlab\"";

    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.ops = ops;
    manifest.op_count = 1;

    ph_resolved_var_t vars[1];
    vars[0].name = (char *)"ci";
    vars[0].value = (char *)"github";
    vars[0].type = PH_VAR_STRING;

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, vars, 1,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_FALSE(plan.ops[0].skip);
    ph_plan_destroy(&plan);
}

/* ---- null args ---- */

void test_plan_build_null(void) {
    ph_plan_t plan;
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR,
        ph_plan_build(NULL, NULL, 0, "/t", "/d", &plan, &err));
    ph_error_destroy(err);
}

/* ---- empty manifest ---- */

void test_plan_build_empty(void) {
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));

    ph_plan_t plan;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_plan_build(&manifest, NULL, 0,
                                    "/tmpl", "/dest", &plan, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(0, plan.count);
    ph_plan_destroy(&plan);
}

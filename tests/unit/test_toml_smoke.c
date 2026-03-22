#include "unity.h"
#include "toml.h"

#include <string.h>
#include <stdlib.h>

TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

void setUp(void) {}
void tearDown(void) {}

void test_toml_parse_table(void) {
    char doc[] =
        "[manifest]\n"
        "schema = 1\n"
        "id = \"test-template\"\n"
        "version = \"0.1.0\"\n";

    char errbuf[256];
    toml_table_t *root = toml_parse(doc, errbuf, sizeof(errbuf));
    TEST_ASSERT_NOT_NULL(root);

    toml_table_t *manifest = toml_table_table(root, "manifest");
    TEST_ASSERT_NOT_NULL(manifest);

    toml_value_t schema = toml_table_int(manifest, "schema");
    TEST_ASSERT_TRUE(schema.ok);
    TEST_ASSERT_EQUAL_INT64(1, schema.u.i);

    toml_value_t id = toml_table_string(manifest, "id");
    TEST_ASSERT_TRUE(id.ok);
    TEST_ASSERT_EQUAL_STRING("test-template", id.u.s);
    free(id.u.s);

    toml_value_t ver = toml_table_string(manifest, "version");
    TEST_ASSERT_TRUE(ver.ok);
    TEST_ASSERT_EQUAL_STRING("0.1.0", ver.u.s);
    free(ver.u.s);

    toml_free(root);
}

void test_toml_parse_array(void) {
    char doc[] =
        "[[ops]]\n"
        "kind = \"mkdir\"\n"
        "to = \"output\"\n"
        "\n"
        "[[ops]]\n"
        "kind = \"copy\"\n"
        "from = \"src\"\n"
        "to = \"output/src\"\n";

    char errbuf[256];
    toml_table_t *root = toml_parse(doc, errbuf, sizeof(errbuf));
    TEST_ASSERT_NOT_NULL(root);

    toml_array_t *ops = toml_table_array(root, "ops");
    TEST_ASSERT_NOT_NULL(ops);
    TEST_ASSERT_EQUAL(2, toml_array_len(ops));

    toml_table_t *op0 = toml_array_table(ops, 0);
    TEST_ASSERT_NOT_NULL(op0);
    toml_value_t kind0 = toml_table_string(op0, "kind");
    TEST_ASSERT_TRUE(kind0.ok);
    TEST_ASSERT_EQUAL_STRING("mkdir", kind0.u.s);
    free(kind0.u.s);

    toml_table_t *op1 = toml_array_table(ops, 1);
    TEST_ASSERT_NOT_NULL(op1);
    toml_value_t from1 = toml_table_string(op1, "from");
    TEST_ASSERT_TRUE(from1.ok);
    TEST_ASSERT_EQUAL_STRING("src", from1.u.s);
    free(from1.u.s);

    toml_free(root);
}

void test_toml_parse_bool_and_string_array(void) {
    char doc[] =
        "[filters]\n"
        "exclude = [\"*.bak\", \".git\"]\n"
        "\n"
        "[[variables]]\n"
        "name = \"debug\"\n"
        "type = \"bool\"\n"
        "required = false\n";

    char errbuf[256];
    toml_table_t *root = toml_parse(doc, errbuf, sizeof(errbuf));
    TEST_ASSERT_NOT_NULL(root);

    toml_table_t *filters = toml_table_table(root, "filters");
    TEST_ASSERT_NOT_NULL(filters);
    toml_array_t *exclude = toml_table_array(filters, "exclude");
    TEST_ASSERT_NOT_NULL(exclude);
    TEST_ASSERT_EQUAL(2, toml_array_len(exclude));

    toml_value_t e0 = toml_array_string(exclude, 0);
    TEST_ASSERT_TRUE(e0.ok);
    TEST_ASSERT_EQUAL_STRING("*.bak", e0.u.s);
    free(e0.u.s);

    toml_array_t *vars = toml_table_array(root, "variables");
    TEST_ASSERT_NOT_NULL(vars);
    toml_table_t *v0 = toml_array_table(vars, 0);
    TEST_ASSERT_NOT_NULL(v0);

    toml_value_t req = toml_table_bool(v0, "required");
    TEST_ASSERT_TRUE(req.ok);
    TEST_ASSERT_FALSE(req.u.b);

    toml_free(root);
}

void test_toml_parse_invalid(void) {
    char doc[] = "invalid = [[[broken\n";

    char errbuf[256];
    toml_table_t *root = toml_parse(doc, errbuf, sizeof(errbuf));
    TEST_ASSERT_NULL(root);
    TEST_ASSERT_TRUE(strlen(errbuf) > 0);
}

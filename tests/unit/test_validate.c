#include "unity.h"
#include "phosphor/args.h"
#include "phosphor/commands.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/args-parser/validate.c")
TEST_SOURCE_FILE("src/args-parser/parser.c")
TEST_SOURCE_FILE("src/args-parser/spec.c")
TEST_SOURCE_FILE("src/args-parser/kvp.c")
TEST_SOURCE_FILE("src/commands/phosphor_commands.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- test-local config with INT and KVP typed flags ---- */

static const ph_argspec_t test_specs[] = {
    { "name",   PH_TYPE_STRING, PH_FORM_VALUED, true,  NULL, NULL, 0 },
    { "count",  PH_TYPE_INT,    PH_FORM_VALUED, false, NULL, NULL, 0 },
    { "data",   PH_TYPE_KVP,    PH_FORM_VALUED, false, NULL, NULL, 0 },
    { "force",  PH_TYPE_BOOL,   PH_FORM_ACTION, false, NULL, NULL, 0 },
};

static const char *const test_choices[] = { "alpha", "beta" };

static const ph_argspec_t test_enum_specs[] = {
    { "name",   PH_TYPE_STRING, PH_FORM_VALUED, true,  NULL, NULL,         0 },
    { "mode",   PH_TYPE_ENUM,   PH_FORM_VALUED, false, NULL, test_choices, 2 },
};

enum { TEST_CMD_ID = 100, TEST_ENUM_CMD_ID = 101 };

static const ph_cmd_def_t test_commands[] = {
    { "testcmd",     TEST_CMD_ID,      test_specs,      4, false },
    { "testenumcmd", TEST_ENUM_CMD_ID, test_enum_specs, 2, false },
};

static const ph_cli_config_t test_config = {
    .tool_name     = "test",
    .commands      = test_commands,
    .command_count = 2,
};

/* ---- helpers ---- */

static char *dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *c = ph_alloc(len + 1);
    memcpy(c, s, len + 1);
    return c;
}

/*
 * Build a ph_parsed_args_t with heap-allocated flags array.
 * The src array is shallow-copied; name/value pointers are moved
 * (caller must not free them separately).
 */
static ph_parsed_args_t make_args(int cmd_id, ph_parsed_flag_t *src,
                                   size_t count) {
    ph_parsed_args_t a = {0};
    a.command_id = cmd_id;
    a.flag_count = count;
    a.flag_cap   = count;
    if (count > 0) {
        a.flags = ph_alloc(count * sizeof(ph_parsed_flag_t));
        memcpy(a.flags, src, count * sizeof(ph_parsed_flag_t));
    }
    return a;
}

/* ---- tests using phosphor_cli_config ---- */

void test_validate_ok_with_required(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 1);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NULL(err);

    ph_parsed_args_destroy(&args);
}

void test_validate_missing_required_flag(void) {
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, NULL, 0);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX002_MISSING_VALUE, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

void test_validate_unknown_flag(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("bogus"), dup("xyz"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX001_UNKNOWN_FLAG, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

void test_validate_action_flag_with_value(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("force"), dup("yes"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX002_MISSING_VALUE, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

void test_validate_url_valid(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("github"),
          dup("https://github.com/user"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NULL(err);

    ph_parsed_args_destroy(&args);
}

void test_validate_url_invalid(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("github"), dup("not-a-url"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX005_TYPE_MISMATCH, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

void test_validate_path_absolute_accepted(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("template"), dup("/absolute/path"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NULL(err);

    ph_parsed_args_destroy(&args);
}

void test_validate_path_traversal_rejected(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("template"), dup("../escape/path"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX005_TYPE_MISMATCH, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

void test_validate_path_relative_ok(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("template"), dup("templates/default"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NULL(err);

    ph_parsed_args_destroy(&args);
}

void test_validate_enum_valid(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("normalize-eol"), dup("lf"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NULL(err);

    ph_parsed_args_destroy(&args);
}

void test_validate_enum_invalid(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("normalize-eol"), dup("bad"), 3 },
    };
    ph_parsed_args_t args = make_args(PHOSPHOR_CMD_CREATE, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&phosphor_cli_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX007_ENUM_VIOLATION, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

/* ---- tests using test-local config for INT and KVP ---- */

void test_validate_int_valid(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("count"), dup("42"), 3 },
    };
    ph_parsed_args_t args = make_args(TEST_CMD_ID, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_validate(&test_config, &args, &err));
    TEST_ASSERT_NULL(err);

    ph_parsed_args_destroy(&args);
}

void test_validate_int_invalid(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("count"), dup("abc"), 3 },
    };
    ph_parsed_args_t args = make_args(TEST_CMD_ID, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&test_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX005_TYPE_MISMATCH, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

void test_validate_kvp_valid(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("data"), dup("!key:value"), 3 },
    };
    ph_parsed_args_t args = make_args(TEST_CMD_ID, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, ph_validate(&test_config, &args, &err));
    TEST_ASSERT_NULL(err);

    ph_parsed_args_destroy(&args);
}

void test_validate_kvp_malformed(void) {
    ph_parsed_flag_t src[] = {
        { PH_FLAG_VALUED, dup("name"), dup("demo"), 2 },
        { PH_FLAG_VALUED, dup("data"), dup("not-kvp"), 3 },
    };
    ph_parsed_args_t args = make_args(TEST_CMD_ID, src, 2);
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, ph_validate(&test_config, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX006_MALFORMED_KVP, err->subcode);

    ph_error_destroy(err);
    ph_parsed_args_destroy(&args);
}

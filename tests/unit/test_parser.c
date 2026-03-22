#include "unity.h"
#include "phosphor/args.h"
#include "phosphor/commands.h"

TEST_SOURCE_FILE("src/args-parser/parser.c")
TEST_SOURCE_FILE("src/args-parser/lexer.c")
TEST_SOURCE_FILE("src/args-parser/spec.c")
TEST_SOURCE_FILE("src/commands/phosphor_commands.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- helpers ---- */

static ph_result_t parse(const char *const *argv, int argc,
                          ph_parsed_args_t *out, ph_error_t **err) {
    ph_token_stream_t tokens = {0};
    if (ph_lexer_tokenize(argc, argv, &tokens, err) != PH_OK) {
        return PH_ERR;
    }
    ph_result_t rc = ph_parser_parse(&phosphor_cli_config, &tokens,
                                      out, err);
    ph_token_stream_destroy(&tokens);
    return rc;
}

/* ---- tests ---- */

void test_parser_create_command(void) {
    const char *argv[] = { "phosphor", "create", "--name=demo" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, parse(argv, 3, &args, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(PHOSPHOR_CMD_CREATE, args.command_id);
    TEST_ASSERT_EQUAL(1, args.flag_count);
    TEST_ASSERT_EQUAL_STRING("name", args.flags[0].name);
    TEST_ASSERT_EQUAL_STRING("demo", args.flags[0].value);

    ph_parsed_args_destroy(&args);
}

void test_parser_version_command(void) {
    const char *argv[] = { "phosphor", "version" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, parse(argv, 2, &args, &err));
    TEST_ASSERT_EQUAL(PHOSPHOR_CMD_VERSION, args.command_id);
    TEST_ASSERT_EQUAL(0, args.flag_count);

    ph_parsed_args_destroy(&args);
}

void test_parser_help_with_topic(void) {
    const char *argv[] = { "phosphor", "help", "create" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, parse(argv, 3, &args, &err));
    TEST_ASSERT_EQUAL(PHOSPHOR_CMD_HELP, args.command_id);
    TEST_ASSERT_EQUAL_STRING("create", args.positional);

    ph_parsed_args_destroy(&args);
}

void test_parser_help_no_topic(void) {
    const char *argv[] = { "phosphor", "help" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, parse(argv, 2, &args, &err));
    TEST_ASSERT_EQUAL(PHOSPHOR_CMD_HELP, args.command_id);
    TEST_ASSERT_NULL(args.positional);

    ph_parsed_args_destroy(&args);
}

void test_parser_unknown_command(void) {
    const char *argv[] = { "phosphor", "bogus" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, parse(argv, 2, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);

    ph_error_destroy(err);
}

void test_parser_duplicate_flag(void) {
    const char *argv[] = { "phosphor", "create", "--name=a", "--name=b" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, parse(argv, 4, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX003_DUPLICATE_FLAG, err->subcode);

    ph_error_destroy(err);
}

void test_parser_polarity_conflict(void) {
    const char *argv[] = { "phosphor", "create",
                            "--enable-hooks", "--disable-hooks" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, parse(argv, 4, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_UX004_POLARITY_CONFLICT, err->subcode);

    ph_error_destroy(err);
}

void test_parser_unexpected_positional(void) {
    const char *argv[] = { "phosphor", "create", "extra" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, parse(argv, 3, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);

    ph_error_destroy(err);
}

void test_parser_bool_flag(void) {
    const char *argv[] = { "phosphor", "create", "--name=demo", "--force" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, parse(argv, 4, &args, &err));
    TEST_ASSERT_EQUAL(2, args.flag_count);
    TEST_ASSERT_EQUAL(PH_FLAG_BOOL, args.flags[1].kind);
    TEST_ASSERT_EQUAL_STRING("force", args.flags[1].name);

    ph_parsed_args_destroy(&args);
}

void test_parser_no_command(void) {
    const char *argv[] = { "phosphor" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, parse(argv, 1, &args, &err));
    TEST_ASSERT_NOT_NULL(err);

    ph_error_destroy(err);
}

void test_parser_flag_before_command(void) {
    const char *argv[] = { "phosphor", "--name=demo" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_ERR, parse(argv, 2, &args, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);

    ph_error_destroy(err);
}

void test_parser_multiple_flags(void) {
    const char *argv[] = { "phosphor", "create",
                            "--name=demo", "--force", "--verbose" };
    ph_parsed_args_t args = {0};
    ph_error_t *err = NULL;

    TEST_ASSERT_EQUAL(PH_OK, parse(argv, 5, &args, &err));
    TEST_ASSERT_EQUAL(3, args.flag_count);

    ph_parsed_args_destroy(&args);
}

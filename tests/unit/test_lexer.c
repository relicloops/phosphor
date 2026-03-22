#include "unity.h"
#include "phosphor/args.h"

TEST_SOURCE_FILE("src/args-parser/lexer.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- helpers ---- */

static ph_token_stream_t lex(int argc, const char *const *argv,
                              ph_error_t **err) {
    ph_token_stream_t stream = {0};
    ph_lexer_tokenize(argc, argv, &stream, err);
    return stream;
}

/* ---- tests ---- */

void test_lexer_positional(void) {
    const char *argv[] = { "phosphor", "create" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(2, argv, &err);

    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(2, s.count);
    TEST_ASSERT_EQUAL(PH_TOK_POSITIONAL, s.tokens[0].kind);
    TEST_ASSERT_EQUAL_STRING("create", s.tokens[0].name);
    TEST_ASSERT_EQUAL(PH_TOK_END, s.tokens[1].kind);

    ph_token_stream_destroy(&s);
}

void test_lexer_valued_flag(void) {
    const char *argv[] = { "phosphor", "create", "--name=demo" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(3, argv, &err);

    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(PH_TOK_VALUED_FLAG, s.tokens[1].kind);
    TEST_ASSERT_EQUAL_STRING("name", s.tokens[1].name);
    TEST_ASSERT_EQUAL_STRING("demo", s.tokens[1].value);

    ph_token_stream_destroy(&s);
}

void test_lexer_bool_flag(void) {
    const char *argv[] = { "phosphor", "create", "--force" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(3, argv, &err);

    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(PH_TOK_BOOL_FLAG, s.tokens[1].kind);
    TEST_ASSERT_EQUAL_STRING("force", s.tokens[1].name);
    TEST_ASSERT_NULL(s.tokens[1].value);

    ph_token_stream_destroy(&s);
}

void test_lexer_enable_flag(void) {
    const char *argv[] = { "phosphor", "create", "--enable-hooks" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(3, argv, &err);

    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(PH_TOK_ENABLE_FLAG, s.tokens[1].kind);
    TEST_ASSERT_EQUAL_STRING("hooks", s.tokens[1].name);

    ph_token_stream_destroy(&s);
}

void test_lexer_disable_flag(void) {
    const char *argv[] = { "phosphor", "create", "--disable-hooks" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(3, argv, &err);

    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(PH_TOK_DISABLE_FLAG, s.tokens[1].kind);
    TEST_ASSERT_EQUAL_STRING("hooks", s.tokens[1].name);

    ph_token_stream_destroy(&s);
}

void test_lexer_valued_empty_value(void) {
    const char *argv[] = { "phosphor", "create", "--name=" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(3, argv, &err);

    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(PH_TOK_VALUED_FLAG, s.tokens[1].kind);
    TEST_ASSERT_EQUAL_STRING("", s.tokens[1].value);

    ph_token_stream_destroy(&s);
}

void test_lexer_short_flag_rejected(void) {
    const char *argv[] = { "phosphor", "-f" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(2, argv, &err);

    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);
    TEST_ASSERT_EQUAL(PH_UX001_UNKNOWN_FLAG, err->subcode);

    ph_error_destroy(err);
    ph_token_stream_destroy(&s);
}

void test_lexer_bare_double_dash_rejected(void) {
    const char *argv[] = { "phosphor", "--" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(2, argv, &err);

    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);

    ph_error_destroy(err);
    ph_token_stream_destroy(&s);
}

void test_lexer_empty_enable_name_rejected(void) {
    const char *argv[] = { "phosphor", "--enable-" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(2, argv, &err);

    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);

    ph_error_destroy(err);
    ph_token_stream_destroy(&s);
}

void test_lexer_empty_disable_name_rejected(void) {
    const char *argv[] = { "phosphor", "--disable-" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(2, argv, &err);

    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);

    ph_error_destroy(err);
    ph_token_stream_destroy(&s);
}

void test_lexer_invalid_flag_ident(void) {
    const char *argv[] = { "phosphor", "--123bad" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(2, argv, &err);

    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_USAGE, err->category);

    ph_error_destroy(err);
    ph_token_stream_destroy(&s);
}

void test_lexer_multiple_tokens(void) {
    const char *argv[] = { "phosphor", "create", "--name=demo", "--force" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(4, argv, &err);

    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL(4, s.count);
    TEST_ASSERT_EQUAL(PH_TOK_POSITIONAL, s.tokens[0].kind);
    TEST_ASSERT_EQUAL(PH_TOK_VALUED_FLAG, s.tokens[1].kind);
    TEST_ASSERT_EQUAL(PH_TOK_BOOL_FLAG, s.tokens[2].kind);
    TEST_ASSERT_EQUAL(PH_TOK_END, s.tokens[3].kind);

    ph_token_stream_destroy(&s);
}

void test_lexer_argv_index_tracking(void) {
    const char *argv[] = { "phosphor", "create", "--name=demo" };
    ph_error_t *err = NULL;
    ph_token_stream_t s = lex(3, argv, &err);

    TEST_ASSERT_EQUAL(1, s.tokens[0].argv_index);
    TEST_ASSERT_EQUAL(2, s.tokens[1].argv_index);

    ph_token_stream_destroy(&s);
}

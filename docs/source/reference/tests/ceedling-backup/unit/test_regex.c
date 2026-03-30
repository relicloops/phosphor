#include "unity.h"
#include "phosphor/regex.h"
#include "phosphor/alloc.h"
#include "phosphor/error.h"

#include <string.h>

TEST_SOURCE_FILE("src/core/regex.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")

void setUp(void) {}
void tearDown(void) {}

/* ==== ph_regex_available ==== */

void test_regex_available_returns_bool(void) {
    bool avail = ph_regex_available();
#ifdef PHOSPHOR_HAS_PCRE2
    TEST_ASSERT_TRUE(avail);
#else
    TEST_ASSERT_FALSE(avail);
#endif
}

void test_regex_available_idempotent(void) {
    bool a = ph_regex_available();
    bool b = ph_regex_available();
    TEST_ASSERT_EQUAL(a, b);
}

/* ==== ph_regex_compile: basic ==== */

void test_compile_valid_pattern(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_regex_compile("^test\\.txt$", &re, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(re);
    TEST_ASSERT_NULL(err);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_invalid_pattern(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_regex_compile("[invalid", &re, &err);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NULL(re);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL(PH_ERR_CONFIG, err->category);
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_null_pattern(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_regex_compile(NULL, &re, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_null_output(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_regex_compile("test", NULL, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_empty_pattern(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_result_t rc = ph_regex_compile("", &re, &err);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_null_err_pointer(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_result_t rc = ph_regex_compile("[bad", &re, NULL);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
    TEST_ASSERT_NULL(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_null_pattern_null_err(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_result_t rc = ph_regex_compile(NULL, &re, NULL);
    TEST_ASSERT_EQUAL(PH_ERR, rc);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_compile: error message content ==== */

void test_compile_error_contains_pattern(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("[unclosed", &re, &err);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL(err->message);
    TEST_ASSERT_NOT_NULL(strstr(err->message, "[unclosed"));
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_error_contains_offset(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("abc(def", &re, &err);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL(err->message);
    TEST_ASSERT_NOT_NULL(strstr(err->message, "offset"));
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_compile: complex pattern syntax ==== */

void test_compile_alternation(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_regex_compile("foo|bar|baz", &re, &err));
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_character_class(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_regex_compile("[a-zA-Z0-9_]+", &re, &err));
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_quantifiers(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_regex_compile("a{2,5}b*c+d?", &re, &err));
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_lookahead(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_regex_compile("foo(?=\\.txt$)", &re, &err));
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_lookbehind(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_regex_compile("(?<=src/).*\\.c$", &re, &err));
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_negative_lookahead(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_regex_compile("^(?!test_).*\\.c$", &re, &err));
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_nested_groups(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_regex_compile("((a|b)(c|d))+", &re, &err));
    TEST_ASSERT_NOT_NULL(re);
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_compile: invalid patterns ==== */

void test_compile_unbalanced_parens(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_regex_compile("(abc", &re, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_invalid_quantifier(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_regex_compile("*abc", &re, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_invalid_escape(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_regex_compile("abc\\", &re, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: basic ==== */

void test_match_simple_exact(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^foo\\.txt$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "foo.txt"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "bar.txt"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "foo.txt.bak"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_extension_suffix(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("\\.log$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "app.log"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "path/to/debug.log"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "logfile"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_path_prefix(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^node_modules/", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "node_modules/foo/bar.js"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "src/node_modules/foo.js"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_partial(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("secret", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "my-secret-file.txt"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "secret"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "public"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_null_regex(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    TEST_ASSERT_FALSE(ph_regex_match(NULL, "test"));
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_null_subject(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("test", &re, &err);
    TEST_ASSERT_FALSE(ph_regex_match(re, NULL));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: empty pattern / empty subject ==== */

void test_match_empty_pattern(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "anything"));
    TEST_ASSERT_TRUE(ph_regex_match(re, ""));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_empty_subject(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, ""));
    TEST_ASSERT_FALSE(ph_regex_match(re, "notempty"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: alternation ==== */

void test_match_alternation(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^(foo|bar|baz)\\.txt$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "foo.txt"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "bar.txt"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "baz.txt"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "qux.txt"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: PCRE2 character classes ==== */

void test_match_digit_class(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^\\d{3}$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "123"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "000"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "12"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "1234"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "abc"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_word_class(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^\\w+$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "hello"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "test_123"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "has space"));
    TEST_ASSERT_FALSE(ph_regex_match(re, ""));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_whitespace_class(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("\\s", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "has space"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "has\ttab"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "has\nnewline"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "nospaces"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_word_boundary(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("\\bsecret\\b", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "my secret file"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "secret"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "secretive"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "nosecrets"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: quantifiers ==== */

void test_match_quantifier_plus(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^a+$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "a"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "aaa"));
    TEST_ASSERT_FALSE(ph_regex_match(re, ""));
    TEST_ASSERT_FALSE(ph_regex_match(re, "b"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_quantifier_star(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^ab*c$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "ac"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "abc"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "abbbbc"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "adc"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_quantifier_range(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^x{2,4}$", &re, &err);
    TEST_ASSERT_FALSE(ph_regex_match(re, "x"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "xx"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "xxx"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "xxxx"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "xxxxx"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: lookahead / lookbehind ==== */

void test_match_positive_lookahead(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("foo(?=\\.txt)", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "foo.txt"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "foo.log"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "bar.txt"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_negative_lookahead(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^(?!\\.).*\\.c$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "main.c"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "foo.c"));
    TEST_ASSERT_FALSE(ph_regex_match(re, ".hidden.c"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "main.h"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_positive_lookbehind(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("(?<=src/)\\w+\\.c$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "src/main.c"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "lib/main.c"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: Unicode (PCRE2_UTF + PCRE2_UCP) ==== */

void test_match_unicode_literal(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("caf\xc3\xa9", &re, &err);
    TEST_ASSERT_EQUAL(PH_OK, err ? PH_ERR : PH_OK);
    TEST_ASSERT_TRUE(ph_regex_match(re, "caf\xc3\xa9"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "cafe"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_unicode_property(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^\\p{L}+$", &re, &err);
    TEST_ASSERT_EQUAL(PH_OK, err ? PH_ERR : PH_OK);
    TEST_ASSERT_TRUE(ph_regex_match(re, "hello"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "caf\xc3\xa9"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "test123"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "has space"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_unicode_dot_matches_multibyte(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^.{4}$", &re, &err);
    TEST_ASSERT_EQUAL(PH_OK, err ? PH_ERR : PH_OK);
    TEST_ASSERT_TRUE(ph_regex_match(re, "abcd"));
    /* 3 ASCII + 1 two-byte char = 4 codepoints */
    TEST_ASSERT_TRUE(ph_regex_match(re, "abc\xc3\xa9"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_unicode_cjk(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("\\p{Han}", &re, &err);
    TEST_ASSERT_EQUAL(PH_OK, err ? PH_ERR : PH_OK);
    /* U+4E16 = 0xE4 0xB8 0x96 */
    TEST_ASSERT_TRUE(ph_regex_match(re, "\xe4\xb8\x96"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "hello"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: manifest filter patterns (real-world) ==== */

void test_match_exclude_hidden_files(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("(^|/)\\.(?!phosphor)", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, ".gitignore"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "src/.hidden"));
    TEST_ASSERT_TRUE(ph_regex_match(re, ".DS_Store"));
    TEST_ASSERT_FALSE(ph_regex_match(re, ".phosphor.toml"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "normal.txt"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_exclude_build_dirs(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^(build|dist|out|target)/", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "build/output.o"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "dist/bundle.js"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "out/release/app"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "target/debug/main"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "src/build.c"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "rebuild/main.c"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_deny_sensitive_files(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("(\\.env|\\.pem|\\.key|id_rsa)$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, ".env"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "config/.env"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "server.pem"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "ssl/private.key"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "~/.ssh/id_rsa"));
    TEST_ASSERT_FALSE(ph_regex_match(re, ".envrc"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "README.md"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_multi_extension_filter(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("\\.(o|a|so|dylib|obj|lib|dll)$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "main.o"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "lib/libfoo.a"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "lib/libbar.so"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "lib/libqux.dylib"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "main.c"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "solid.txt"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_nested_dir_pattern(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("(^|/)node_modules(/|$)", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "node_modules/foo"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "node_modules"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "packages/app/node_modules/bar"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "src/node_modulesX"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_path_with_slashes(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^src/template/.*\\.c$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "src/template/writer.c"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "src/template/manifest_load.c"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "src/core/alloc.c"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "src/template/writer.h"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_case_sensitive_by_default(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^README$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "README"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "readme"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "Readme"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_case_insensitive_inline(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("(?i)^readme$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "README"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "readme"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "Readme"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "ReAdMe"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_match: edge cases ==== */

void test_match_very_long_subject(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("needle", &re, &err);

    char subject[4097];
    memset(subject, 'x', 4090);
    memcpy(subject + 4090, "needle", 6);
    subject[4096] = '\0';

    TEST_ASSERT_TRUE(ph_regex_match(re, subject));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_no_match_long_subject(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("needle", &re, &err);

    char subject[4097];
    memset(subject, 'x', 4096);
    subject[4096] = '\0';

    TEST_ASSERT_FALSE(ph_regex_match(re, subject));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_special_regex_chars_in_path(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("\\[draft\\]\\.txt$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "document[draft].txt"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "documentdraft.txt"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_match_dot_not_literal(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("^foo.bar$", &re, &err);
    TEST_ASSERT_TRUE(ph_regex_match(re, "foo.bar"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "foo-bar"));
    TEST_ASSERT_TRUE(ph_regex_match(re, "foo_bar"));
    TEST_ASSERT_FALSE(ph_regex_match(re, "foobar"));
    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== ph_regex_destroy ==== */

void test_destroy_null_noop(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_destroy(NULL);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== compile/destroy cycles (leak detection via ASan) ==== */

void test_compile_destroy_cycle(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    for (int i = 0; i < 100; i++) {
        ph_regex_t *re = NULL;
        ph_error_t *err = NULL;
        ph_result_t rc = ph_regex_compile("^test\\d+\\.txt$", &re, &err);
        TEST_ASSERT_EQUAL(PH_OK, rc);
        TEST_ASSERT_NOT_NULL(re);
        ph_regex_destroy(re);
    }
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_compile_fail_cycle(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    for (int i = 0; i < 50; i++) {
        ph_regex_t *re = NULL;
        ph_error_t *err = NULL;
        ph_result_t rc = ph_regex_compile("[bad", &re, &err);
        TEST_ASSERT_EQUAL(PH_ERR, rc);
        TEST_ASSERT_NULL(re);
        TEST_ASSERT_NOT_NULL(err);
        ph_error_destroy(err);
    }
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

void test_multiple_regex_concurrent(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re1 = NULL;
    ph_regex_t *re2 = NULL;
    ph_regex_t *re3 = NULL;
    ph_error_t *err = NULL;

    ph_regex_compile("^foo", &re1, &err);
    ph_regex_compile("\\.log$", &re2, &err);
    ph_regex_compile("secret", &re3, &err);

    TEST_ASSERT_TRUE(ph_regex_match(re1, "foobar"));
    TEST_ASSERT_FALSE(ph_regex_match(re1, "barfoo"));

    TEST_ASSERT_TRUE(ph_regex_match(re2, "app.log"));
    TEST_ASSERT_FALSE(ph_regex_match(re2, "app.txt"));

    TEST_ASSERT_TRUE(ph_regex_match(re3, "my-secret"));
    TEST_ASSERT_FALSE(ph_regex_match(re3, "public"));

    ph_regex_destroy(re1);
    ph_regex_destroy(re2);
    ph_regex_destroy(re3);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

/* ==== reuse: match the same regex against many subjects ==== */

void test_match_reuse_compiled_regex(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t *re = NULL;
    ph_error_t *err = NULL;
    ph_regex_compile("\\.(js|ts|tsx|jsx)$", &re, &err);

    const char *yes[] = {
        "app.js", "main.ts", "Component.tsx", "page.jsx",
        "src/lib/util.js", "deep/nested/file.ts"
    };
    const char *no[] = {
        "style.css", "main.c", "README.md", "Makefile",
        "image.png", "data.json"
    };

    for (size_t i = 0; i < sizeof(yes) / sizeof(yes[0]); i++)
        TEST_ASSERT_TRUE_MESSAGE(ph_regex_match(re, yes[i]), yes[i]);

    for (size_t i = 0; i < sizeof(no) / sizeof(no[0]); i++)
        TEST_ASSERT_FALSE_MESSAGE(ph_regex_match(re, no[i]), no[i]);

    ph_regex_destroy(re);
#else
    TEST_IGNORE_MESSAGE("pcre2 not available");
#endif
}

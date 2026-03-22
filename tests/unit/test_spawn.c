#include "unity.h"
#include "phosphor/proc.h"
#include "phosphor/alloc.h"

#include <string.h>

TEST_SOURCE_FILE("src/proc/spawn.c")
TEST_SOURCE_FILE("src/proc/wait.c")
TEST_SOURCE_FILE("src/proc/env.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("src/platform/posix/proc_posix.c")
TEST_SOURCE_FILE("src/platform/signal.c")

void setUp(void) {}
void tearDown(void) {}

/* --- argv builder tests ------------------------------------------------- */

void test_argv_init(void) {
    ph_argv_builder_t b;
    ph_result_t rc = ph_argv_init(&b, 4);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_NOT_NULL(b.items);
    TEST_ASSERT_EQUAL(0, b.count);
    TEST_ASSERT_EQUAL(4, b.cap);
    TEST_ASSERT_NULL(b.items[0]);
    ph_argv_destroy(&b);
}

void test_argv_init_null(void) {
    TEST_ASSERT_EQUAL(PH_ERR, ph_argv_init(NULL, 4));
}

void test_argv_init_zero_cap(void) {
    ph_argv_builder_t b;
    ph_result_t rc = ph_argv_init(&b, 0);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_TRUE(b.cap > 0);
    ph_argv_destroy(&b);
}

void test_argv_push(void) {
    ph_argv_builder_t b;
    ph_argv_init(&b, 4);

    TEST_ASSERT_EQUAL(PH_OK, ph_argv_push(&b, "hello"));
    TEST_ASSERT_EQUAL(1, b.count);
    TEST_ASSERT_EQUAL_STRING("hello", b.items[0]);
    TEST_ASSERT_NULL(b.items[1]);

    ph_argv_destroy(&b);
}

void test_argv_push_grows(void) {
    ph_argv_builder_t b;
    ph_argv_init(&b, 2);

    TEST_ASSERT_EQUAL(PH_OK, ph_argv_push(&b, "a"));
    TEST_ASSERT_EQUAL(PH_OK, ph_argv_push(&b, "b"));
    TEST_ASSERT_EQUAL(PH_OK, ph_argv_push(&b, "c"));
    TEST_ASSERT_EQUAL(3, b.count);
    TEST_ASSERT_TRUE(b.cap >= 3);

    TEST_ASSERT_EQUAL_STRING("a", b.items[0]);
    TEST_ASSERT_EQUAL_STRING("b", b.items[1]);
    TEST_ASSERT_EQUAL_STRING("c", b.items[2]);
    TEST_ASSERT_NULL(b.items[3]);

    ph_argv_destroy(&b);
}

void test_argv_push_null_arg(void) {
    ph_argv_builder_t b;
    ph_argv_init(&b, 4);
    TEST_ASSERT_EQUAL(PH_ERR, ph_argv_push(&b, NULL));
    ph_argv_destroy(&b);
}

void test_argv_pushf(void) {
    ph_argv_builder_t b;
    ph_argv_init(&b, 4);

    TEST_ASSERT_EQUAL(PH_OK, ph_argv_pushf(&b, "--count=%d", 42));
    TEST_ASSERT_EQUAL(1, b.count);
    TEST_ASSERT_EQUAL_STRING("--count=42", b.items[0]);

    ph_argv_destroy(&b);
}

void test_argv_finalize(void) {
    ph_argv_builder_t b;
    ph_argv_init(&b, 4);

    ph_argv_push(&b, "cmd");
    ph_argv_push(&b, "arg1");

    char **argv = ph_argv_finalize(&b);
    TEST_ASSERT_NOT_NULL(argv);
    TEST_ASSERT_EQUAL_STRING("cmd", argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg1", argv[1]);
    TEST_ASSERT_NULL(argv[2]);

    /* builder should be invalidated */
    TEST_ASSERT_NULL(b.items);
    TEST_ASSERT_EQUAL(0, b.count);
    TEST_ASSERT_EQUAL(0, b.cap);

    ph_argv_free(argv);
}

void test_argv_finalize_null(void) {
    TEST_ASSERT_NULL(ph_argv_finalize(NULL));

    ph_argv_builder_t b = { .items = NULL, .count = 0, .cap = 0 };
    TEST_ASSERT_NULL(ph_argv_finalize(&b));
}

void test_argv_free_null(void) {
    /* should not crash */
    ph_argv_free(NULL);
}

void test_argv_destroy_null(void) {
    /* should not crash */
    ph_argv_destroy(NULL);
}

/* --- exec integration tests --------------------------------------------- */

void test_exec_echo_success(void) {
    char *argv[] = { "echo", "hello", NULL };
    ph_proc_opts_t opts = { .argv = argv, .cwd = NULL, .env = NULL,
                            .timeout_sec = 0 };

    int exit_code = -1;
    ph_result_t rc = ph_proc_exec(&opts, &exit_code);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(0, exit_code);
}

void test_exec_false_returns_general(void) {
    char *argv[] = { "false", NULL };
    ph_proc_opts_t opts = { .argv = argv, .cwd = NULL, .env = NULL,
                            .timeout_sec = 0 };

    int exit_code = -1;
    ph_result_t rc = ph_proc_exec(&opts, &exit_code);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_EQUAL(1, exit_code);
}

void test_exec_null_opts(void) {
    int exit_code;
    TEST_ASSERT_EQUAL(PH_ERR, ph_proc_exec(NULL, &exit_code));
}

void test_exec_null_argv(void) {
    ph_proc_opts_t opts = { .argv = NULL, .cwd = NULL, .env = NULL,
                            .timeout_sec = 0 };
    int exit_code;
    TEST_ASSERT_EQUAL(PH_ERR, ph_proc_exec(&opts, &exit_code));
}

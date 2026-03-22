#include "unity.h"
#include "phosphor/proc.h"

TEST_SOURCE_FILE("src/proc/wait.c")

void setUp(void) {}
void tearDown(void) {}

void test_map_exit_success(void) {
    ph_proc_result_t r = { .exit_code = 0, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(0, ph_proc_map_exit(&r));
}

void test_map_exit_general(void) {
    ph_proc_result_t r = { .exit_code = 1, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(1, ph_proc_map_exit(&r));
}

void test_map_exit_usage(void) {
    ph_proc_result_t r = { .exit_code = 2, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(2, ph_proc_map_exit(&r));
}

void test_map_exit_config(void) {
    ph_proc_result_t r = { .exit_code = 3, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(3, ph_proc_map_exit(&r));
}

void test_map_exit_fs(void) {
    ph_proc_result_t r = { .exit_code = 4, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(4, ph_proc_map_exit(&r));
}

void test_map_exit_process(void) {
    ph_proc_result_t r = { .exit_code = 5, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(5, ph_proc_map_exit(&r));
}

void test_map_exit_validate(void) {
    ph_proc_result_t r = { .exit_code = 6, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(6, ph_proc_map_exit(&r));
}

void test_map_exit_internal(void) {
    ph_proc_result_t r = { .exit_code = 7, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(7, ph_proc_map_exit(&r));
}

void test_map_exit_unmapped_42(void) {
    ph_proc_result_t r = { .exit_code = 42, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(1, ph_proc_map_exit(&r));
}

void test_map_exit_unmapped_100(void) {
    ph_proc_result_t r = { .exit_code = 100, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(1, ph_proc_map_exit(&r));
}

void test_map_exit_signal_130(void) {
    ph_proc_result_t r = { .exit_code = 130, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(8, ph_proc_map_exit(&r));
}

void test_map_exit_signal_128(void) {
    ph_proc_result_t r = { .exit_code = 128, .signaled = false, .signal_num = 0 };
    TEST_ASSERT_EQUAL(8, ph_proc_map_exit(&r));
}

void test_map_exit_signaled_flag(void) {
    ph_proc_result_t r = { .exit_code = 2, .signaled = true, .signal_num = 2 };
    TEST_ASSERT_EQUAL(8, ph_proc_map_exit(&r));
}

void test_map_exit_null_input(void) {
    TEST_ASSERT_EQUAL(1, ph_proc_map_exit(NULL));
}

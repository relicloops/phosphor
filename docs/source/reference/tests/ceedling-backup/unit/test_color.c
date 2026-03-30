#include "unity.h"
#include "phosphor/color.h"

#include <stdlib.h>
#include <string.h>

TEST_SOURCE_FILE("src/core/color.c")

void setUp(void) {}
void tearDown(void) {}

/* ---- ph_color_init ---- */

void test_color_init_never_disables(void) {
    ph_color_init(PH_COLOR_NEVER);
    TEST_ASSERT_FALSE(ph_color_enabled(stdout));
    TEST_ASSERT_FALSE(ph_color_enabled(stderr));
}

void test_color_init_always_enables(void) {
    unsetenv("NO_COLOR");
    unsetenv("FORCE_COLOR");
    ph_color_init(PH_COLOR_ALWAYS);
    TEST_ASSERT_TRUE(ph_color_enabled(stdout));
    TEST_ASSERT_TRUE(ph_color_enabled(stderr));
}

/* ---- NO_COLOR / FORCE_COLOR ---- */

void test_no_color_env_disables(void) {
    setenv("NO_COLOR", "1", 1);
    unsetenv("FORCE_COLOR");
    ph_color_init(PH_COLOR_ALWAYS);
    TEST_ASSERT_FALSE(ph_color_enabled(stdout));
    TEST_ASSERT_FALSE(ph_color_enabled(stderr));
    unsetenv("NO_COLOR");
}

void test_no_color_empty_is_ignored(void) {
    setenv("NO_COLOR", "", 1);
    unsetenv("FORCE_COLOR");
    ph_color_init(PH_COLOR_ALWAYS);
    TEST_ASSERT_TRUE(ph_color_enabled(stdout));
    TEST_ASSERT_TRUE(ph_color_enabled(stderr));
    unsetenv("NO_COLOR");
}

void test_force_color_env_enables(void) {
    unsetenv("NO_COLOR");
    setenv("FORCE_COLOR", "1", 1);
    ph_color_init(PH_COLOR_NEVER);
    TEST_ASSERT_TRUE(ph_color_enabled(stdout));
    TEST_ASSERT_TRUE(ph_color_enabled(stderr));
    unsetenv("FORCE_COLOR");
}

void test_no_color_beats_force_color(void) {
    setenv("NO_COLOR", "1", 1);
    setenv("FORCE_COLOR", "1", 1);
    ph_color_init(PH_COLOR_ALWAYS);
    TEST_ASSERT_FALSE(ph_color_enabled(stdout));
    TEST_ASSERT_FALSE(ph_color_enabled(stderr));
    unsetenv("NO_COLOR");
    unsetenv("FORCE_COLOR");
}

/* ---- ph_color_for ---- */

void test_color_for_returns_seq_when_enabled(void) {
    unsetenv("NO_COLOR");
    unsetenv("FORCE_COLOR");
    ph_color_init(PH_COLOR_ALWAYS);
    const char *s = ph_color_for(stdout, PH_BOLD);
    TEST_ASSERT_EQUAL_STRING(PH_BOLD, s);
}

void test_color_for_returns_empty_when_disabled(void) {
    ph_color_init(PH_COLOR_NEVER);
    const char *s = ph_color_for(stdout, PH_BOLD);
    TEST_ASSERT_EQUAL_STRING("", s);
}

/* ---- unknown stream ---- */

void test_color_enabled_unknown_stream_returns_false(void) {
    unsetenv("NO_COLOR");
    unsetenv("FORCE_COLOR");
    ph_color_init(PH_COLOR_ALWAYS);
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_FALSE(ph_color_enabled(f));
    fclose(f);
}

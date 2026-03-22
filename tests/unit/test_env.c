#include "unity.h"
#include "phosphor/proc.h"

#include <string.h>
#include <stdlib.h>

TEST_SOURCE_FILE("src/proc/env.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

/* helper: check if an entry with the given key exists in env */
static const char *env_find(const ph_env_t *env, const char *key) {
    size_t klen = strlen(key);
    for (size_t i = 0; i < env->count; i++) {
        if (strncmp(env->entries[i], key, klen) == 0 &&
            env->entries[i][klen] == '=') {
            return env->entries[i] + klen + 1;
        }
    }
    return NULL;
}

/* helper: check if any entry with the given key prefix exists */
static bool env_has_key(const ph_env_t *env, const char *key) {
    return env_find(env, key) != NULL;
}

void test_env_build_path_passes(void) {
    /* PATH should be in the system allowlist */
    ph_env_t env;
    ph_result_t rc = ph_env_build(NULL, &env);
    TEST_ASSERT_EQUAL(PH_OK, rc);

    /* PATH is almost certainly set in test environment */
    TEST_ASSERT_TRUE(env_has_key(&env, "PATH"));

    ph_env_destroy(&env);
}

void test_env_build_unknown_blocked(void) {
    /* set an unknown var, verify it's blocked */
    setenv("PH_TEST_JUNK_XYZ_999", "should_not_pass", 1);

    ph_env_t env;
    ph_result_t rc = ph_env_build(NULL, &env);
    TEST_ASSERT_EQUAL(PH_OK, rc);
    TEST_ASSERT_FALSE(env_has_key(&env, "PH_TEST_JUNK_XYZ_999"));

    ph_env_destroy(&env);
    unsetenv("PH_TEST_JUNK_XYZ_999");
}

void test_env_build_phosphor_prefix(void) {
    setenv("PHOSPHOR_TEST_VAR", "hello", 1);

    ph_env_t env;
    ph_result_t rc = ph_env_build(NULL, &env);
    TEST_ASSERT_EQUAL(PH_OK, rc);

    const char *val = env_find(&env, "PHOSPHOR_TEST_VAR");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("hello", val);

    ph_env_destroy(&env);
    unsetenv("PHOSPHOR_TEST_VAR");
}

void test_env_build_extras_exact(void) {
    setenv("MY_CUSTOM_VAR", "custom_val", 1);

    const char *extras[] = { "MY_CUSTOM_VAR", NULL };
    ph_env_t env;
    ph_result_t rc = ph_env_build(extras, &env);
    TEST_ASSERT_EQUAL(PH_OK, rc);

    const char *val = env_find(&env, "MY_CUSTOM_VAR");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("custom_val", val);

    ph_env_destroy(&env);
    unsetenv("MY_CUSTOM_VAR");
}

void test_env_build_extras_prefix(void) {
    setenv("NPM_CONFIG_REGISTRY", "https://example.com", 1);

    /* trailing _ means prefix match */
    const char *extras[] = { "NPM_", NULL };
    ph_env_t env;
    ph_result_t rc = ph_env_build(extras, &env);
    TEST_ASSERT_EQUAL(PH_OK, rc);

    TEST_ASSERT_TRUE(env_has_key(&env, "NPM_CONFIG_REGISTRY"));

    ph_env_destroy(&env);
    unsetenv("NPM_CONFIG_REGISTRY");
}

void test_env_set_new_key(void) {
    ph_env_t env;
    ph_env_build(NULL, &env);

    ph_result_t rc = ph_env_set(&env, "NEW_KEY", "new_val");
    TEST_ASSERT_EQUAL(PH_OK, rc);

    const char *val = env_find(&env, "NEW_KEY");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("new_val", val);

    ph_env_destroy(&env);
}

void test_env_set_override(void) {
    ph_env_t env;
    ph_env_build(NULL, &env);

    ph_env_set(&env, "OVERRIDE_ME", "original");
    ph_env_set(&env, "OVERRIDE_ME", "replaced");

    const char *val = env_find(&env, "OVERRIDE_ME");
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_STRING("replaced", val);

    ph_env_destroy(&env);
}

void test_env_null_terminated(void) {
    ph_env_t env;
    ph_env_build(NULL, &env);

    /* the array should be NULL-terminated after count entries */
    TEST_ASSERT_NULL(env.entries[env.count]);

    ph_env_destroy(&env);
}

void test_env_destroy_null_safe(void) {
    /* should not crash */
    ph_env_destroy(NULL);

    ph_env_t env = { .entries = NULL, .count = 0 };
    ph_env_destroy(&env);
}

void test_env_build_null_out_returns_err(void) {
    TEST_ASSERT_EQUAL(PH_ERR, ph_env_build(NULL, NULL));
}

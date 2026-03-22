#include "unity.h"
#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/fs.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

TEST_SOURCE_FILE("src/certs/certs_config.c")
TEST_SOURCE_FILE("src/core/alloc.c")
TEST_SOURCE_FILE("src/core/error.c")
TEST_SOURCE_FILE("src/core/log.c")
TEST_SOURCE_FILE("src/core/color.c")
TEST_SOURCE_FILE("src/io/fs_readwrite.c")
TEST_SOURCE_FILE("src/io/fs_copytree.c")
TEST_SOURCE_FILE("src/io/fs_atomic.c")
TEST_SOURCE_FILE("src/io/path_norm.c")
TEST_SOURCE_FILE("src/io/metadata_filter.c")
TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
TEST_SOURCE_FILE("src/platform/signal.c")
TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

static char test_dir[PATH_MAX];
static unsigned test_seq = 0;

void setUp(void) {
    snprintf(test_dir, sizeof(test_dir), "/tmp/ph_certs_cfg_%d_%u",
             (int)getpid(), test_seq++);
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(test_dir, 0755));
}

void tearDown(void) {
    ph_error_t *err = NULL;
    ph_fs_rmtree(test_dir, &err);
    ph_error_destroy(err);
}

/* ---- helpers ---- */

static char *write_toml(const char *content) {
    char *path = ph_path_join(test_dir, "template.phosphor.toml");
    ph_io_write_file(path, (const uint8_t *)content, strlen(content), NULL);
    return path;
}

/* ---- test: no [certs] section ---- */

void test_no_certs_section(void) {
    char *path = write_toml(
        "[manifest]\nschema = 1\nid = \"test\"\nversion = \"1.0\"\n");
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, &err));
    TEST_ASSERT_FALSE(cfg.present);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: minimal [certs] ---- */

void test_minimal_certs_section(void) {
    char *path = write_toml(
        "[certs]\n"
        "output_dir = \"my-certs\"\n");
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, &err));
    TEST_ASSERT_TRUE(cfg.present);
    TEST_ASSERT_EQUAL_STRING("my-certs", cfg.output_dir);
    TEST_ASSERT_EQUAL_STRING("phosphor-local-CA", cfg.ca_cn);
    TEST_ASSERT_EQUAL(4096, cfg.ca_bits);
    TEST_ASSERT_EQUAL(3650, cfg.ca_days);
    TEST_ASSERT_EQUAL(2048, cfg.leaf_bits);
    TEST_ASSERT_EQUAL(825, cfg.leaf_days);
    TEST_ASSERT_EQUAL(0, cfg.domain_count);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: defaults ---- */

void test_defaults(void) {
    char *path = write_toml("[certs]\n");
    ph_certs_config_t cfg;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, NULL));
    TEST_ASSERT_TRUE(cfg.present);
    TEST_ASSERT_EQUAL_STRING("certs", cfg.output_dir);
    TEST_ASSERT_EQUAL_STRING("phosphor-local-CA", cfg.ca_cn);
    TEST_ASSERT_EQUAL(4096, cfg.ca_bits);
    TEST_ASSERT_EQUAL(3650, cfg.ca_days);
    TEST_ASSERT_EQUAL(2048, cfg.leaf_bits);
    TEST_ASSERT_EQUAL(825, cfg.leaf_days);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: custom scalar values ---- */

void test_custom_scalars(void) {
    char *path = write_toml(
        "[certs]\n"
        "output_dir = \"ssl\"\n"
        "ca_cn = \"my-ca\"\n"
        "ca_bits = 2048\n"
        "ca_days = 365\n"
        "leaf_bits = 4096\n"
        "leaf_days = 90\n"
        "account_key = \"/path/to/key\"\n");
    ph_certs_config_t cfg;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, NULL));
    TEST_ASSERT_EQUAL_STRING("ssl", cfg.output_dir);
    TEST_ASSERT_EQUAL_STRING("my-ca", cfg.ca_cn);
    TEST_ASSERT_EQUAL(2048, cfg.ca_bits);
    TEST_ASSERT_EQUAL(365, cfg.ca_days);
    TEST_ASSERT_EQUAL(4096, cfg.leaf_bits);
    TEST_ASSERT_EQUAL(90, cfg.leaf_days);
    TEST_ASSERT_EQUAL_STRING("/path/to/key", cfg.account_key);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: local domain ---- */

void test_local_domain(void) {
    char *path = write_toml(
        "[certs]\n"
        "[[certs.domains]]\n"
        "name = \"example.host\"\n"
        "mode = \"local\"\n"
        "san = [\"example.host\", \"www.example.host\"]\n");
    ph_certs_config_t cfg;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, NULL));
    TEST_ASSERT_EQUAL(1, cfg.domain_count);
    TEST_ASSERT_EQUAL_STRING("example.host", cfg.domains[0].name);
    TEST_ASSERT_EQUAL(PH_CERT_LOCAL, cfg.domains[0].mode);
    TEST_ASSERT_EQUAL(2, cfg.domains[0].san_count);
    TEST_ASSERT_EQUAL_STRING("example.host", cfg.domains[0].san[0]);
    TEST_ASSERT_EQUAL_STRING("www.example.host", cfg.domains[0].san[1]);
    TEST_ASSERT_NULL(cfg.domains[0].dir_name);
    TEST_ASSERT_NULL(cfg.domains[0].email);
    TEST_ASSERT_NULL(cfg.domains[0].webroot);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: domain with dir_name override ---- */

void test_domain_dir_name(void) {
    char *path = write_toml(
        "[certs]\n"
        "[[certs.domains]]\n"
        "name = \"10.0.0.1\"\n"
        "mode = \"local\"\n"
        "san = [\"10.0.0.1\"]\n"
        "dir_name = \"_default\"\n");
    ph_certs_config_t cfg;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, NULL));
    TEST_ASSERT_EQUAL(1, cfg.domain_count);
    TEST_ASSERT_EQUAL_STRING("_default", cfg.domains[0].dir_name);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: letsencrypt domain ---- */

void test_letsencrypt_domain(void) {
    char *path = write_toml(
        "[certs]\n"
        "[[certs.domains]]\n"
        "name = \"example.com\"\n"
        "mode = \"letsencrypt\"\n"
        "san = [\"example.com\", \"www.example.com\"]\n"
        "email = \"admin@example.com\"\n"
        "webroot = \"/var/acme\"\n");
    ph_certs_config_t cfg;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, NULL));
    TEST_ASSERT_EQUAL(1, cfg.domain_count);
    TEST_ASSERT_EQUAL(PH_CERT_LETSENCRYPT, cfg.domains[0].mode);
    TEST_ASSERT_EQUAL_STRING("admin@example.com", cfg.domains[0].email);
    TEST_ASSERT_EQUAL_STRING("/var/acme", cfg.domains[0].webroot);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: letsencrypt missing email ---- */

void test_letsencrypt_missing_email(void) {
    char *path = write_toml(
        "[certs]\n"
        "[[certs.domains]]\n"
        "name = \"example.com\"\n"
        "mode = \"letsencrypt\"\n"
        "san = [\"example.com\"]\n"
        "webroot = \"/var/acme\"\n");
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_certs_config_parse(path, &cfg, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL(strstr(err->message, "email"));
    ph_error_destroy(err);
    ph_free(path);
}

/* ---- test: letsencrypt missing webroot ---- */

void test_letsencrypt_missing_webroot(void) {
    char *path = write_toml(
        "[certs]\n"
        "[[certs.domains]]\n"
        "name = \"example.com\"\n"
        "mode = \"letsencrypt\"\n"
        "san = [\"example.com\"]\n"
        "email = \"admin@example.com\"\n");
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_certs_config_parse(path, &cfg, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL(strstr(err->message, "webroot"));
    ph_error_destroy(err);
    ph_free(path);
}

/* ---- test: domain missing name ---- */

void test_domain_missing_name(void) {
    char *path = write_toml(
        "[certs]\n"
        "[[certs.domains]]\n"
        "mode = \"local\"\n"
        "san = [\"example.host\"]\n");
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_certs_config_parse(path, &cfg, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL(strstr(err->message, "name"));
    ph_error_destroy(err);
    ph_free(path);
}

/* ---- test: multiple domains ---- */

void test_multiple_domains(void) {
    char *path = write_toml(
        "[certs]\n"
        "[[certs.domains]]\n"
        "name = \"a.host\"\n"
        "mode = \"local\"\n"
        "san = [\"a.host\"]\n"
        "[[certs.domains]]\n"
        "name = \"b.com\"\n"
        "mode = \"letsencrypt\"\n"
        "san = [\"b.com\"]\n"
        "email = \"a@b.com\"\n"
        "webroot = \"/acme\"\n");
    ph_certs_config_t cfg;
    TEST_ASSERT_EQUAL(PH_OK, ph_certs_config_parse(path, &cfg, NULL));
    TEST_ASSERT_EQUAL(2, cfg.domain_count);
    TEST_ASSERT_EQUAL(PH_CERT_LOCAL, cfg.domains[0].mode);
    TEST_ASSERT_EQUAL(PH_CERT_LETSENCRYPT, cfg.domains[1].mode);
    ph_certs_config_destroy(&cfg);
    ph_free(path);
}

/* ---- test: NULL arguments ---- */

void test_null_arguments(void) {
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_certs_config_parse(NULL, &cfg, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ---- test: missing file ---- */

void test_missing_file(void) {
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR,
        ph_certs_config_parse("/nonexistent/file.toml", &cfg, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

/* ---- test: invalid TOML ---- */

void test_invalid_toml(void) {
    char *path = write_toml("this is not valid toml {{{}}}");
    ph_certs_config_t cfg;
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR, ph_certs_config_parse(path, &cfg, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
    ph_free(path);
}

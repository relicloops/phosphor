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

TEST_SOURCE_FILE("src/certs/certs_san.c")
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

static char test_dir[PATH_MAX];
static unsigned test_seq = 0;

void setUp(void) {
    snprintf(test_dir, sizeof(test_dir), "/tmp/ph_certs_san_%d_%u",
             (int)getpid(), test_seq++);
    TEST_ASSERT_EQUAL(PH_OK, ph_fs_mkdir_p(test_dir, 0755));
}

void tearDown(void) {
    ph_error_t *err = NULL;
    ph_fs_rmtree(test_dir, &err);
    ph_error_destroy(err);
}

/* ---- ph_cert_san_is_ip tests ---- */

void test_ipv4_detected(void) {
    TEST_ASSERT_TRUE(ph_cert_san_is_ip("10.0.0.1"));
    TEST_ASSERT_TRUE(ph_cert_san_is_ip("192.168.1.100"));
    TEST_ASSERT_TRUE(ph_cert_san_is_ip("127.0.0.1"));
    TEST_ASSERT_TRUE(ph_cert_san_is_ip("0.0.0.0"));
}

void test_ipv6_detected(void) {
    TEST_ASSERT_TRUE(ph_cert_san_is_ip("::1"));
    TEST_ASSERT_TRUE(ph_cert_san_is_ip("fe80::1"));
    TEST_ASSERT_TRUE(ph_cert_san_is_ip("2001:db8::1"));
}

void test_hostnames_not_ip(void) {
    TEST_ASSERT_FALSE(ph_cert_san_is_ip("example.com"));
    TEST_ASSERT_FALSE(ph_cert_san_is_ip("www.example.host"));
    TEST_ASSERT_FALSE(ph_cert_san_is_ip("localhost"));
    TEST_ASSERT_FALSE(ph_cert_san_is_ip("my-server.local"));
}

void test_null_empty(void) {
    TEST_ASSERT_FALSE(ph_cert_san_is_ip(NULL));
    TEST_ASSERT_FALSE(ph_cert_san_is_ip(""));
}

/* ---- ph_cert_san_write_cnf tests ---- */

void test_write_cnf_dns(void) {
    char *cnf_path = ph_path_join(test_dir, "san.cnf");
    const char *san[] = {"example.com", "www.example.com"};

    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_OK,
        ph_cert_san_write_cnf(cnf_path, san, 2, &err));
    TEST_ASSERT_NULL(err);

    /* read and verify contents */
    uint8_t *data = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(PH_OK, ph_io_read_file(cnf_path, &data, &len, NULL));
    TEST_ASSERT_NOT_NULL(data);

    char *text = (char *)data;
    TEST_ASSERT_NOT_NULL(strstr(text, "DNS.1 = example.com"));
    TEST_ASSERT_NOT_NULL(strstr(text, "DNS.2 = www.example.com"));
    TEST_ASSERT_NOT_NULL(strstr(text, "[v3_req]"));
    TEST_ASSERT_NOT_NULL(strstr(text, "[alt_names]"));
    TEST_ASSERT_NULL(strstr(text, "IP."));

    ph_free(data);
    ph_free(cnf_path);
}

void test_write_cnf_ip(void) {
    char *cnf_path = ph_path_join(test_dir, "san_ip.cnf");
    const char *san[] = {"10.0.0.1", "192.168.1.1"};

    TEST_ASSERT_EQUAL(PH_OK,
        ph_cert_san_write_cnf(cnf_path, san, 2, NULL));

    uint8_t *data = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(PH_OK, ph_io_read_file(cnf_path, &data, &len, NULL));

    char *text = (char *)data;
    TEST_ASSERT_NOT_NULL(strstr(text, "IP.1 = 10.0.0.1"));
    TEST_ASSERT_NOT_NULL(strstr(text, "IP.2 = 192.168.1.1"));
    TEST_ASSERT_NULL(strstr(text, "DNS."));

    ph_free(data);
    ph_free(cnf_path);
}

void test_write_cnf_mixed(void) {
    char *cnf_path = ph_path_join(test_dir, "san_mixed.cnf");
    const char *san[] = {"example.com", "10.0.0.1", "www.example.com"};

    TEST_ASSERT_EQUAL(PH_OK,
        ph_cert_san_write_cnf(cnf_path, san, 3, NULL));

    uint8_t *data = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(PH_OK, ph_io_read_file(cnf_path, &data, &len, NULL));

    char *text = (char *)data;
    TEST_ASSERT_NOT_NULL(strstr(text, "DNS.1 = example.com"));
    TEST_ASSERT_NOT_NULL(strstr(text, "IP.1 = 10.0.0.1"));
    TEST_ASSERT_NOT_NULL(strstr(text, "DNS.2 = www.example.com"));

    ph_free(data);
    ph_free(cnf_path);
}

void test_write_cnf_null_args(void) {
    ph_error_t *err = NULL;
    TEST_ASSERT_EQUAL(PH_ERR,
        ph_cert_san_write_cnf(NULL, NULL, 0, &err));
    TEST_ASSERT_NOT_NULL(err);
    ph_error_destroy(err);
}

#include "unity.h"
#include "phosphor/bytes.h"

#include <string.h>

TEST_SOURCE_FILE("src/core/bytes.c")
TEST_SOURCE_FILE("src/core/alloc.c")

void setUp(void) {}
void tearDown(void) {}

void test_bytes_from(void) {
    const uint8_t data[] = { 0x01, 0x02, 0x03 };
    ph_bytes_t b = ph_bytes_from(data, 3);

    TEST_ASSERT_EQUAL_PTR(data, b.data);
    TEST_ASSERT_EQUAL(3, b.len);
}

void test_bytes_slice_basic(void) {
    const uint8_t data[] = { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E };
    ph_bytes_t b = ph_bytes_from(data, 5);

    ph_bytes_t s = ph_bytes_slice(b, 1, 4);
    TEST_ASSERT_EQUAL(3, s.len);
    TEST_ASSERT_EQUAL_UINT8(0x0B, s.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x0D, s.data[2]);
}

void test_bytes_slice_clamps(void) {
    const uint8_t data[] = { 0x01 };
    ph_bytes_t b = ph_bytes_from(data, 1);

    ph_bytes_t s = ph_bytes_slice(b, 0, 100);
    TEST_ASSERT_EQUAL(1, s.len);
}

void test_bytes_slice_start_past_end(void) {
    const uint8_t data[] = { 0x01, 0x02 };
    ph_bytes_t b = ph_bytes_from(data, 2);

    ph_bytes_t s = ph_bytes_slice(b, 5, 10);
    TEST_ASSERT_EQUAL(0, s.len);
}

void test_bytes_equal_true(void) {
    const uint8_t a[] = { 0x01, 0x02 };
    const uint8_t b[] = { 0x01, 0x02 };

    TEST_ASSERT_TRUE(ph_bytes_equal(
        ph_bytes_from(a, 2), ph_bytes_from(b, 2)));
}

void test_bytes_equal_false(void) {
    const uint8_t a[] = { 0x01, 0x02 };
    const uint8_t b[] = { 0x01, 0x03 };

    TEST_ASSERT_FALSE(ph_bytes_equal(
        ph_bytes_from(a, 2), ph_bytes_from(b, 2)));
}

void test_bytes_equal_different_lengths(void) {
    const uint8_t a[] = { 0x01, 0x02 };
    const uint8_t b[] = { 0x01 };

    TEST_ASSERT_FALSE(ph_bytes_equal(
        ph_bytes_from(a, 2), ph_bytes_from(b, 1)));
}

void test_bytes_equal_empty(void) {
    TEST_ASSERT_TRUE(ph_bytes_equal(
        ph_bytes_from(NULL, 0), ph_bytes_from(NULL, 0)));
}

void test_bytes_find_present(void) {
    const uint8_t hay[] = { 0x01, 0x02, 0x03, 0x04 };
    const uint8_t nee[] = { 0x02, 0x03 };

    ptrdiff_t pos = ph_bytes_find(
        ph_bytes_from(hay, 4), ph_bytes_from(nee, 2));
    TEST_ASSERT_EQUAL(1, pos);
}

void test_bytes_find_absent(void) {
    const uint8_t hay[] = { 0x01, 0x02, 0x03 };
    const uint8_t nee[] = { 0xFF };

    ptrdiff_t pos = ph_bytes_find(
        ph_bytes_from(hay, 3), ph_bytes_from(nee, 1));
    TEST_ASSERT_EQUAL(-1, pos);
}

void test_bytes_find_empty_needle(void) {
    const uint8_t hay[] = { 0x01 };
    TEST_ASSERT_EQUAL(0, ph_bytes_find(
        ph_bytes_from(hay, 1), ph_bytes_from(NULL, 0)));
}

void test_bytebuf_init_and_append(void) {
    ph_bytebuf_t buf;
    TEST_ASSERT_EQUAL(PH_OK, ph_bytebuf_init(&buf, 8));
    TEST_ASSERT_EQUAL(0, buf.len);

    const uint8_t data[] = { 0xAA, 0xBB };
    TEST_ASSERT_EQUAL(PH_OK, ph_bytebuf_append(&buf, data, 2));
    TEST_ASSERT_EQUAL(2, buf.len);
    TEST_ASSERT_EQUAL_UINT8(0xAA, buf.data[0]);

    ph_bytebuf_destroy(&buf);
}

void test_bytebuf_as_bytes(void) {
    ph_bytebuf_t buf;
    ph_bytebuf_init(&buf, 4);

    const uint8_t data[] = { 0x01, 0x02, 0x03 };
    ph_bytebuf_append(&buf, data, 3);

    ph_bytes_t b = ph_bytebuf_as_bytes(&buf);
    TEST_ASSERT_EQUAL(3, b.len);
    TEST_ASSERT_EQUAL_UINT8(0x01, b.data[0]);

    ph_bytebuf_destroy(&buf);
}

void test_bytebuf_clear(void) {
    ph_bytebuf_t buf;
    ph_bytebuf_init(&buf, 4);

    const uint8_t data[] = { 0x01 };
    ph_bytebuf_append(&buf, data, 1);
    ph_bytebuf_clear(&buf);
    TEST_ASSERT_EQUAL(0, buf.len);

    ph_bytebuf_destroy(&buf);
}

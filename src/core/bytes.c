#include "phosphor/bytes.h"
#include "phosphor/alloc.h"

#include <string.h>

ph_bytes_t ph_bytes_from(const uint8_t *data, size_t len) {
    ph_bytes_t b = { .data = data, .len = len };
    return b;
}

ph_bytes_t ph_bytes_slice(ph_bytes_t b, size_t start, size_t end) {
    if (start > b.len) start = b.len;
    if (end   > b.len) end   = b.len;
    if (start > end)   start = end;

    ph_bytes_t s = { .data = b.data + start, .len = end - start };
    return s;
}

bool ph_bytes_equal(ph_bytes_t a, ph_bytes_t b) {
    if (a.len != b.len) return false;
    if (a.len == 0)     return true;
    return memcmp(a.data, b.data, a.len) == 0;
}

ptrdiff_t ph_bytes_find(ph_bytes_t haystack, ph_bytes_t needle) {
    if (needle.len == 0)            return 0;
    if (needle.len > haystack.len)  return -1;

    size_t limit = haystack.len - needle.len;
    for (size_t i = 0; i <= limit; i++) {
        if (memcmp(haystack.data + i, needle.data, needle.len) == 0) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

ph_result_t ph_bytebuf_init(ph_bytebuf_t *buf, size_t cap) {
    if (!buf) return PH_ERR;
    if (cap == 0) cap = 64;

    buf->data = ph_alloc(cap);
    if (!buf->data) { buf->len = 0; buf->cap = 0; return PH_ERR; }

    buf->len = 0;
    buf->cap = cap;
    return PH_OK;
}

ph_result_t ph_bytebuf_append(ph_bytebuf_t *buf,
                              const uint8_t *data, size_t len) {
    if (!buf) return PH_ERR;
    if (len == 0) return PH_OK;

    size_t needed = buf->len + len;
    if (needed > buf->cap) {
        size_t new_cap = buf->cap * 2;
        if (new_cap < needed) new_cap = needed;

        uint8_t *new_data = ph_realloc(buf->data, new_cap);
        if (!new_data) return PH_ERR;

        buf->data = new_data;
        buf->cap  = new_cap;
    }

    memcpy(buf->data + buf->len, data, len);
    buf->len = needed;
    return PH_OK;
}

ph_bytes_t ph_bytebuf_as_bytes(const ph_bytebuf_t *buf) {
    if (!buf) return ph_bytes_from(NULL, 0);
    return ph_bytes_from(buf->data, buf->len);
}

void ph_bytebuf_clear(ph_bytebuf_t *buf) {
    if (!buf) return;
    buf->len = 0;
}

void ph_bytebuf_destroy(ph_bytebuf_t *buf) {
    if (!buf) return;
    ph_free(buf->data);
    buf->data = NULL;
    buf->len  = 0;
    buf->cap  = 0;
}

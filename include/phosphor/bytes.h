#ifndef PHOSPHOR_BYTES_H
#define PHOSPHOR_BYTES_H

#include "phosphor/types.h"

/*
 * ph_bytes_t -- non-owning byte slice.
 *
 * ownership:
 *   data -- NOT owned, points into external buffer. caller must ensure
 *           the backing buffer outlives the slice.
 */
typedef struct {
    const uint8_t *data;
    size_t         len;
} ph_bytes_t;

/*
 * ph_bytebuf_t -- owning byte buffer with dynamic growth.
 *
 * ownership:
 *   data -- owner: self (heap-allocated, freed by ph_bytebuf_destroy)
 */
typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} ph_bytebuf_t;

/* non-owning slice constructors */
ph_bytes_t  ph_bytes_from(const uint8_t *data, size_t len);
ph_bytes_t  ph_bytes_slice(ph_bytes_t b, size_t start, size_t end);

/* slice operations */
bool        ph_bytes_equal(ph_bytes_t a, ph_bytes_t b);
ptrdiff_t   ph_bytes_find(ph_bytes_t haystack, ph_bytes_t needle);

/* owning buffer */
ph_result_t ph_bytebuf_init(ph_bytebuf_t *buf, size_t cap);
ph_result_t ph_bytebuf_append(ph_bytebuf_t *buf,
                              const uint8_t *data, size_t len);
ph_bytes_t  ph_bytebuf_as_bytes(const ph_bytebuf_t *buf);
void        ph_bytebuf_clear(ph_bytebuf_t *buf);
void        ph_bytebuf_destroy(ph_bytebuf_t *buf);

#endif /* PHOSPHOR_BYTES_H */

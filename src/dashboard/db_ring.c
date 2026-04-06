#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/alloc.h"

#include <string.h>

/* ---- UTF-8 helper ---- */

int utf8_seq_len(unsigned char c) {
    if (c < 0x80)            return 1;
    if ((c & 0xE0) == 0xC0)  return 2;
    if ((c & 0xF0) == 0xE0)  return 3;
    if ((c & 0xF8) == 0xF0)  return 4;
    return 0;
}

/* ---- line cleaner ---- */

/*
 * clean_line -- clean a raw line for dashboard storage.
 * preserves SGR sequences (\033[...m) and valid UTF-8.
 * strips OSC, non-SGR CSI sequences, and non-printable bytes.
 * writes to dst (must be >= srclen+1). returns byte length.
 * sets *vis_width to the visual column count (excluding escape bytes).
 */
int clean_line(char *dst, int *vis_width, const char *src, int srclen) {
    int d = 0;
    int vw = 0;
    for (int i = 0; i < srclen; ) {
        unsigned char c = (unsigned char)src[i];

        if (src[i] == '\033') {
            if (i + 1 < srclen && src[i+1] == '[') {
                /* CSI -- check if it ends with 'm' (SGR) */
                int start = i;
                int j = i + 2;
                while (j < srclen && (unsigned char)src[j] < 0x40)
                    j++;
                if (j < srclen && src[j] == 'm') {
                    /* SGR -- keep it */
                    int slen = j + 1 - start;
                    memcpy(dst + d, src + start, (size_t)slen);
                    d += slen;
                    i = j + 1;
                } else {
                    /* non-SGR CSI -- strip */
                    i = (j < srclen) ? j + 1 : srclen;
                }
            } else if (i + 1 < srclen && src[i+1] == ']') {
                /* OSC -- strip until ST or BEL */
                i += 2;
                while (i < srclen) {
                    if (src[i] == '\007') { i++; break; }
                    if (src[i] == '\033' && i + 1 < srclen && src[i+1] == '\\') {
                        i += 2; break;
                    }
                    i++;
                }
            } else {
                /* other ESC -- skip ESC + one char */
                i += 2;
                if (i > srclen) i = srclen;
            }
        } else if (c >= 0x80) {
            /* multi-byte UTF-8 */
            int slen = utf8_seq_len(c);
            if (slen >= 2 && i + slen <= srclen) {
                bool valid = true;
                for (int k = 1; k < slen; k++) {
                    if (((unsigned char)src[i+k] & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    memcpy(dst + d, src + i, (size_t)slen);
                    d += slen;
                    vw++;
                    i += slen;
                } else {
                    i++;
                }
            } else {
                i++;
            }
        } else if (c >= 0x20 || c == '\t') {
            dst[d++] = src[i++];
            vw++;
        } else {
            i++;
        }
    }
    dst[d] = '\0';
    *vis_width = vw;
    return d;
}

/* ---- ANSI stripper (for :save) ---- */

/*
 * strip_ansi -- strip ALL escape sequences from a line.
 * preserves printable ASCII, valid UTF-8, tabs.
 * writes to dst (must be >= srclen+1). returns byte length.
 */
int strip_ansi(char *dst, const char *src, int srclen) {
    int d = 0;
    for (int i = 0; i < srclen; ) {
        unsigned char c = (unsigned char)src[i];

        if (src[i] == '\033') {
            if (i + 1 < srclen && src[i+1] == '[') {
                /* CSI -- skip to terminator */
                i += 2;
                while (i < srclen && (unsigned char)src[i] < 0x40)
                    i++;
                if (i < srclen) i++; /* skip final byte */
            } else if (i + 1 < srclen && src[i+1] == ']') {
                /* OSC -- skip until ST or BEL */
                i += 2;
                while (i < srclen) {
                    if (src[i] == '\007') { i++; break; }
                    if (src[i] == '\033' && i + 1 < srclen && src[i+1] == '\\') {
                        i += 2; break;
                    }
                    i++;
                }
            } else {
                i += 2;
                if (i > srclen) i = srclen;
            }
        } else if (c >= 0x80) {
            int slen = utf8_seq_len(c);
            if (slen >= 2 && i + slen <= srclen) {
                bool valid = true;
                for (int k = 1; k < slen; k++) {
                    if (((unsigned char)src[i+k] & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    memcpy(dst + d, src + i, (size_t)slen);
                    d += slen;
                    i += slen;
                } else {
                    i++;
                }
            } else {
                i++;
            }
        } else if (c >= 0x20 || c == '\t') {
            dst[d++] = src[i++];
        } else {
            i++;
        }
    }
    dst[d] = '\0';
    return d;
}

/* ---- ring buffer ---- */

void ringbuf_push(db_ringbuf_t *rb, const char *text, int len,
                   bool is_stderr) {
    int idx = (rb->head + rb->count) % MAX_LINES;
    if (rb->count == MAX_LINES) {
        ph_free(rb->lines[rb->head].text);
        rb->head = (rb->head + 1) % MAX_LINES;
    } else {
        rb->count++;
    }
    char *tmp = ph_alloc((size_t)len + 1);
    if (tmp) {
        int vis_w = 0;
        int clean_len = clean_line(tmp, &vis_w, text, len);
        rb->lines[idx].text = tmp;
        rb->lines[idx].len = clean_len;
        rb->lines[idx].vis_width = vis_w;
    } else {
        rb->lines[idx].text = NULL;
        rb->lines[idx].len = 0;
        rb->lines[idx].vis_width = 0;
    }
    rb->lines[idx].is_stderr = is_stderr;
}

db_line_t *ringbuf_get(db_ringbuf_t *rb, int i) {
    if (i < 0 || i >= rb->count) return NULL;
    return &rb->lines[(rb->head + i) % MAX_LINES];
}

void ringbuf_destroy(db_ringbuf_t *rb) {
    for (int i = 0; i < rb->count; i++) {
        int idx = (rb->head + i) % MAX_LINES;
        ph_free(rb->lines[idx].text);
    }
    rb->head = 0;
    rb->count = 0;
}

/* ---- line accumulator ---- */

void feed_accum_multi(db_accum_t *acc, db_ringbuf_t **targets,
                      int ntargets, const char *buf, int n,
                      bool is_stderr) {
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            if (acc->pos > 0) {
                for (int t = 0; t < ntargets; t++)
                    ringbuf_push(targets[t], acc->buf, acc->pos, is_stderr);
                acc->pos = 0;
            }
        } else {
            if (acc->pos < MAX_LINE_LEN - 1)
                acc->buf[acc->pos++] = buf[i];
        }
    }
}

void feed_accum(db_accum_t *acc, db_ringbuf_t *ring,
                const char *buf, int n, bool is_stderr) {
    feed_accum_multi(acc, &ring, 1, buf, n, is_stderr);
}

#endif /* PHOSPHOR_HAS_NCURSES */

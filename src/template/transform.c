#include "phosphor/render.h"
#include "phosphor/alloc.h"

#include <string.h>

/* binary extensions (always copy, never render) */
static const char *binary_exts[] = {
    ".png", ".jpg", ".jpeg", ".gif", ".ico", ".bmp", ".tiff",
    ".webp", ".svg",
    ".woff", ".woff2", ".ttf", ".otf", ".eot",
    ".zip", ".gz", ".tar", ".bz2", ".xz", ".7z",
    ".pdf", ".doc", ".docx", ".xls", ".xlsx",
    ".exe", ".dll", ".so", ".dylib", ".a", ".o",
    ".mp3", ".mp4", ".wav", ".ogg", ".flac",
    ".avi", ".mkv", ".mov", ".wmv",
    ".sqlite", ".db",
};

bool ph_transform_is_binary(const uint8_t *data, size_t len,
                             const char *extension) {
    /* check extension table */
    if (extension) {
        for (size_t i = 0; i < sizeof(binary_exts) / sizeof(binary_exts[0]); i++) {
            if (strcmp(extension, binary_exts[i]) == 0) return true;
        }
    }

    /* NUL-byte heuristic: check first 8192 bytes */
    size_t check_len = len < 8192 ? len : 8192;
    for (size_t i = 0; i < check_len; i++) {
        if (data[i] == 0) return true;
    }

    return false;
}

ph_result_t ph_transform_newline(const uint8_t *data, size_t len,
                                  const char *mode,
                                  uint8_t **out_data, size_t *out_len) {
    if (!data || !out_data || !out_len) return PH_ERR;

    /* "keep" or NULL: return a copy */
    if (!mode || strcmp(mode, "keep") == 0) {
        uint8_t *copy = ph_alloc(len + 1);
        if (!copy) return PH_ERR;
        memcpy(copy, data, len);
        copy[len] = 0;
        *out_data = copy;
        *out_len = len;
        return PH_OK;
    }

    bool to_crlf = (strcmp(mode, "crlf") == 0);

    /* worst case for LF->CRLF: every byte is \n -> doubles */
    size_t cap = to_crlf ? len * 2 + 1 : len + 1;
    uint8_t *buf = ph_alloc(cap);
    if (!buf) return PH_ERR;

    size_t wi = 0;
    for (size_t ri = 0; ri < len; ri++) {
        if (data[ri] == '\r' && ri + 1 < len && data[ri + 1] == '\n') {
            /* CRLF pair */
            if (to_crlf) {
                buf[wi++] = '\r';
                buf[wi++] = '\n';
            } else {
                buf[wi++] = '\n';
            }
            ri++;  /* skip the \n */
        } else if (data[ri] == '\r') {
            /* bare CR */
            if (to_crlf) {
                buf[wi++] = '\r';
                buf[wi++] = '\n';
            } else {
                buf[wi++] = '\n';
            }
        } else if (data[ri] == '\n') {
            /* bare LF */
            if (to_crlf) {
                buf[wi++] = '\r';
                buf[wi++] = '\n';
            } else {
                buf[wi++] = '\n';
            }
        } else {
            buf[wi++] = data[ri];
        }
    }

    buf[wi] = 0;
    *out_data = buf;
    *out_len = wi;
    return PH_OK;
}

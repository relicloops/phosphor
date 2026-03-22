#include "phosphor/error.h"
#include "phosphor/alloc.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static char *dup_string(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = ph_alloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

ph_error_t *ph_error_create(ph_err_category_t cat, int subcode,
                            const char *msg) {
    ph_error_t *err = ph_calloc(1, sizeof(ph_error_t));
    if (!err) return NULL;

    err->category = cat;
    err->subcode  = subcode;
    err->message  = dup_string(msg);
    err->context  = NULL;
    err->cause    = NULL;

    if (msg && !err->message) {
        ph_free(err);
        return NULL;
    }
    return err;
}

ph_error_t *ph_error_createf(ph_err_category_t cat, int subcode,
                             const char *fmt, ...) {
    if (!fmt) return ph_error_create(cat, subcode, NULL);

    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) return ph_error_create(cat, subcode, fmt);

    char *buf = ph_alloc((size_t)needed + 1);
    if (!buf) return ph_error_create(cat, subcode, fmt);

    va_start(ap, fmt);
    vsnprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    ph_error_t *err = ph_calloc(1, sizeof(ph_error_t));
    if (!err) { ph_free(buf); return NULL; }

    err->category = cat;
    err->subcode  = subcode;
    err->message  = buf;
    err->context  = NULL;
    err->cause    = NULL;
    return err;
}

void ph_error_set_context(ph_error_t *err, const char *ctx) {
    if (!err) return;
    ph_free(err->context);
    err->context = dup_string(ctx);
}

void ph_error_chain(ph_error_t *err, ph_error_t *cause) {
    if (!err) return;
    err->cause = cause;
}

void ph_error_destroy(ph_error_t *err) {
    while (err) {
        ph_error_t *next = err->cause;
        ph_free(err->message);
        ph_free(err->context);
        ph_free(err);
        err = next;
    }
}

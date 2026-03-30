#include "phosphor/term.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---- OSC 8 hyperlinks ---- */

void ph_term_link(FILE *stream, const char *url, const char *text,
                  const char *color_seq) {
    if (!stream || !text) return;

    bool use_osc = ph_color_enabled(stream) && url && url[0];

    if (use_osc)
        fprintf(stream, "\033]8;;%s\033\\", url);

    if (color_seq)
        fprintf(stream, "%s", ph_color_for(stream, color_seq));

    fprintf(stream, "%s", text);

    if (color_seq)
        fprintf(stream, "%s", ph_color_for(stream, PH_RESET));

    if (use_osc)
        fprintf(stream, "\033]8;;\033\\");
}

void ph_term_linkf(FILE *stream, const char *text, const char *color_seq,
                   const char *url_fmt, ...) {
    char url[1024];
    va_list ap;
    va_start(ap, url_fmt);
    vsnprintf(url, sizeof(url), url_fmt, ap);
    va_end(ap);

    ph_term_link(stream, url, text, color_seq);
}

/* ---- labeled key-value output ---- */

static void print_label(FILE *stream, int indent, int label_width,
                        const char *label) {
    fprintf(stream, "%*s%s%-*s",
            indent, "",
            ph_color_for(stream, PH_DIM),
            label_width, label);
    fprintf(stream, "%s", ph_color_for(stream, PH_RESET));
}

void ph_term_kv(FILE *stream, int indent, int label_width,
                const char *label, const char *value,
                const char *color_seq) {
    print_label(stream, indent, label_width, label);
    fprintf(stream, "%s%s%s\n",
            ph_color_for(stream, color_seq),
            value ? value : "",
            ph_color_for(stream, PH_RESET));
}

void ph_term_kvf(FILE *stream, int indent, int label_width,
                 const char *label, const char *color_seq,
                 const char *fmt, ...) {
    print_label(stream, indent, label_width, label);
    fprintf(stream, "%s", ph_color_for(stream, color_seq));

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stream, fmt, ap);
    va_end(ap);

    fprintf(stream, "%s\n", ph_color_for(stream, PH_RESET));
}

void ph_term_kv_link(FILE *stream, int indent, int label_width,
                     const char *label, const char *url, const char *text,
                     const char *color_seq) {
    print_label(stream, indent, label_width, label);
    ph_term_link(stream, url, text, color_seq);
    fprintf(stream, "\n");
}

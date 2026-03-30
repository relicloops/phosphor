#ifndef PHOSPHOR_TERM_H
#define PHOSPHOR_TERM_H

#include "phosphor/color.h"

#include <stdio.h>

/*
 * Terminal feature utilities.
 *
 * OSC 8 clickable hyperlinks, labeled key-value output,
 * and terminal capability detection.
 */

/* ---- OSC 8 hyperlinks ---- */

/*
 * ph_term_link -- print a clickable hyperlink using OSC 8.
 *
 * if the stream supports color (TTY), emits:
 *   \033]8;;URL\033\\TEXT\033]8;;\033\\
 * otherwise emits TEXT only.
 *
 * color_seq is an optional ANSI color applied to the text (e.g. PH_FG_CYAN).
 * pass NULL for no color.
 */
void ph_term_link(FILE *stream, const char *url, const char *text,
                  const char *color_seq);

/*
 * ph_term_linkf -- print a clickable hyperlink with printf-formatted URL.
 *
 * builds the URL from fmt + args, then delegates to ph_term_link.
 */
void ph_term_linkf(FILE *stream, const char *text, const char *color_seq,
                   const char *url_fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* ---- labeled key-value output ---- */

/*
 * ph_term_kv -- print a labeled value:
 *
 *   "  label  value\n"
 *
 * label is printed dim, value in the given color_seq.
 * label is left-padded to indent and right-padded to width.
 */
void ph_term_kv(FILE *stream, int indent, int label_width,
                const char *label, const char *value,
                const char *color_seq);

/*
 * ph_term_kvf -- same as ph_term_kv but value is printf-formatted.
 */
void ph_term_kvf(FILE *stream, int indent, int label_width,
                 const char *label, const char *color_seq,
                 const char *fmt, ...)
    __attribute__((format(printf, 6, 7)));

/*
 * ph_term_kv_link -- print a labeled clickable value:
 *
 *   "  label  URL (clickable)\n"
 */
void ph_term_kv_link(FILE *stream, int indent, int label_width,
                     const char *label, const char *url, const char *text,
                     const char *color_seq);

#endif /* PHOSPHOR_TERM_H */

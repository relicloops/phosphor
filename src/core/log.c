#include "phosphor/log.h"
#include "phosphor/color.h"

#include <stdio.h>
#include <stdarg.h>

static ph_log_level_t g_level = PH_LOG_INFO;

static const char *level_labels[] = {
    "error",
    "warn",
    "info",
    "debug",
    "trace"
};

/* per-level color sequences */
static const char *level_colors[] = {
    PH_BOLD PH_FG_RED,           /* error */
    PH_BOLD PH_FG_YELLOW,        /* warn  */
    PH_BOLD PH_FG_GREEN,         /* info  */
    PH_FG_CYAN,                  /* debug */
    PH_DIM,                      /* trace */
};

void ph_log_set_level(ph_log_level_t level) {
    g_level = level;
}

ph_log_level_t ph_log_get_level(void) {
    return g_level;
}

void ph_log(ph_log_level_t level, const char *fmt, ...) {
    if (level > g_level) return;

    /* info -> stdout, everything else -> stderr */
    FILE *out = (level == PH_LOG_INFO) ? stdout : stderr;

    const char *c = ph_color_for(out, level_colors[level]);
    const char *r = ph_color_for(out, PH_RESET);

    fprintf(out, "[%s%s%s] ", c, level_labels[level], r);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);

    fputc('\n', out);
}

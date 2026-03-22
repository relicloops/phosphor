#ifndef PHOSPHOR_COLOR_H
#define PHOSPHOR_COLOR_H

#include "phosphor/types.h"

#include <stdio.h>

/* ---- ANSI SGR escape codes ---- */

/* reset */
#define PH_RESET       "\033[0m"

/* attributes */
#define PH_BOLD        "\033[1m"
#define PH_DIM         "\033[2m"
#define PH_ITALIC      "\033[3m"
#define PH_UNDERLINE   "\033[4m"

/* foreground colors */
#define PH_FG_BLACK    "\033[30m"
#define PH_FG_RED      "\033[31m"
#define PH_FG_GREEN    "\033[32m"
#define PH_FG_YELLOW   "\033[33m"
#define PH_FG_BLUE     "\033[34m"
#define PH_FG_MAGENTA  "\033[35m"
#define PH_FG_CYAN     "\033[36m"
#define PH_FG_WHITE    "\033[37m"

/* bright foreground colors */
#define PH_FG_BRIGHT_BLACK    "\033[90m"
#define PH_FG_BRIGHT_RED      "\033[91m"
#define PH_FG_BRIGHT_GREEN    "\033[92m"
#define PH_FG_BRIGHT_YELLOW   "\033[93m"
#define PH_FG_BRIGHT_BLUE     "\033[94m"
#define PH_FG_BRIGHT_MAGENTA  "\033[95m"
#define PH_FG_BRIGHT_CYAN     "\033[96m"
#define PH_FG_BRIGHT_WHITE    "\033[97m"

/* ---- runtime color control ---- */

typedef enum {
    PH_COLOR_AUTO,    /* detect from terminal */
    PH_COLOR_ALWAYS,
    PH_COLOR_NEVER,
} ph_color_mode_t;

/*
 * ph_color_init -- call once at startup. detects TTY capability for
 * stdout and stderr. respects NO_COLOR and FORCE_COLOR env vars.
 */
void ph_color_init(ph_color_mode_t mode);

/*
 * ph_color_enabled -- returns true if color is enabled for the given stream.
 * pass stdout or stderr.
 */
bool ph_color_enabled(FILE *stream);

/*
 * ph_color_for -- returns the escape sequence if color is enabled for
 * the stream, otherwise returns "".
 */
const char *ph_color_for(FILE *stream, const char *seq);

#endif /* PHOSPHOR_COLOR_H */

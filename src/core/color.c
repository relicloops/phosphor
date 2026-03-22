#include "phosphor/color.h"

#include <stdlib.h>
#include <unistd.h>

static bool g_stdout_color = false;
static bool g_stderr_color = false;
static bool g_initialized = false;

void ph_color_init(ph_color_mode_t mode) {
    g_initialized = true;

    /* NO_COLOR (https://no-color.org/) -- any non-empty value disables */
    const char *no_color = getenv("NO_COLOR");
    if (no_color && no_color[0] != '\0') {
        g_stdout_color = false;
        g_stderr_color = false;
        return;
    }

    /* FORCE_COLOR -- any non-empty value forces color on */
    const char *force_color = getenv("FORCE_COLOR");
    if (force_color && force_color[0] != '\0') {
        g_stdout_color = true;
        g_stderr_color = true;
        return;
    }

    switch (mode) {
    case PH_COLOR_ALWAYS:
        g_stdout_color = true;
        g_stderr_color = true;
        return;
    case PH_COLOR_NEVER:
        g_stdout_color = false;
        g_stderr_color = false;
        return;
    case PH_COLOR_AUTO:
        break;
    }

    /* auto-detect: check if file descriptors are TTYs */
    g_stdout_color = isatty(STDOUT_FILENO) != 0;
    g_stderr_color = isatty(STDERR_FILENO) != 0;
}

bool ph_color_enabled(FILE *stream) {
    if (!g_initialized)
        ph_color_init(PH_COLOR_AUTO);

    if (stream == stdout) return g_stdout_color;
    if (stream == stderr) return g_stderr_color;
    return false;
}

const char *ph_color_for(FILE *stream, const char *seq) {
    return ph_color_enabled(stream) ? seq : "";
}

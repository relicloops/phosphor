#include "phosphor/cli.h"
#include "phosphor/color.h"

#include <stdio.h>

int ph_cli_version(void) {
    const char *g = ph_color_for(stdout, PH_BOLD PH_FG_GREEN);
    const char *v = ph_color_for(stdout, PH_FG_CYAN);
    const char *r = ph_color_for(stdout, PH_RESET);

    printf("%sphosphor%s %s%s%s\n", g, r, v, PHOSPHOR_VERSION, r);
    return 0;
}

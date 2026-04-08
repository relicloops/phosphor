#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/error.h"
#include "phosphor/log.h"

#include <stdio.h>

int ph_cmd_filament(const ph_cli_config_t *config,
                    const ph_parsed_args_t *args) {
    (void)config;

    const char *path = ph_args_get_flag(args, "path");
    if (!path) {
        ph_log_error("filament: --path is required");
        return PH_ERR_CONFIG;
    }

    /* placeholder -- filament editor is not yet implemented.
     *
     * audit fix (2026-04-08T11-07-17Z): return a non-zero exit
     * so scripts and CI can distinguish the placeholder from a
     * real success. The [experimental] marker in the command
     * spec still documents intent at help-listing time, and
     * PH_ERR_GENERAL is the closest category in error.h for an
     * unimplemented command (the others carry stronger semantic
     * meaning that does not fit "not yet implemented"). */
    fprintf(stdout, "filament: %s\n", path);
    fprintf(stdout, "filament: not yet implemented\n");

    return PH_ERR_GENERAL;
}

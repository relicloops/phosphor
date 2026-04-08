#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/error.h"
#include "phosphor/log.h"

#include <stdio.h>

int ph_cmd_filament(const ph_cli_config_t *config,
                    const ph_parsed_args_t *args) {
    (void)config;
    (void)args;

    /* placeholder -- filament editor is not yet implemented.
     *
     * audit fix (2026-04-08): filament is [experimental] and reserved.
     * The handler prints a placeholder message and returns
     * PH_ERR_GENERAL so scripts and CI can distinguish the placeholder
     * from a real success. The command spec no longer declares --path
     * because the editor does not exist yet; once it lands the flag
     * can be added back as a required PH_TYPE_PATH input. */
    fprintf(stdout, "filament: [experimental] not yet implemented\n");
    return PH_ERR_GENERAL;
}

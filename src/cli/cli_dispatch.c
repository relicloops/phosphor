#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/log.h"

int ph_cli_dispatch(const ph_cli_config_t *config,
                    const ph_parsed_args_t *args) {
    if (!config || !args) return PH_ERR_INTERNAL;

    switch (args->command_id) {

    case PHOSPHOR_CMD_VERSION:
        return ph_cli_version();

    case PHOSPHOR_CMD_HELP:
        return ph_cli_help(config, args->positional);

    case PHOSPHOR_CMD_CREATE:
        return ph_cmd_create(config, args);

    case PHOSPHOR_CMD_BUILD:
        return ph_cmd_build(config, args);

    case PHOSPHOR_CMD_CLEAN:
        return ph_cmd_clean(config, args);

    case PHOSPHOR_CMD_RM:
        return ph_cmd_rm(config, args);

    case PHOSPHOR_CMD_CERTS:
        return ph_cmd_certs(config, args);

    case PHOSPHOR_CMD_DOCTOR:
        return ph_cmd_doctor(config, args);

#ifdef PHOSPHOR_HAS_EMBEDDED
    case PHOSPHOR_CMD_GLOW:
        return ph_cmd_glow(config, args);
#endif

    case PHOSPHOR_CMD_SERVE:
        return ph_cmd_serve(config, args);

    default:
        return PH_ERR_INTERNAL;
    }
}

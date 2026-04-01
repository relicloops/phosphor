#ifndef PHOSPHOR_COMMANDS_H
#define PHOSPHOR_COMMANDS_H

#include "phosphor/args.h"

/* ---- phosphor command IDs ---- */

enum {
    PHOSPHOR_CMD_CREATE  = 1,
    PHOSPHOR_CMD_BUILD   = 2,
    PHOSPHOR_CMD_CLEAN   = 3,
    PHOSPHOR_CMD_DOCTOR  = 4,
    PHOSPHOR_CMD_VERSION = 5,
    PHOSPHOR_CMD_HELP    = 6,
    PHOSPHOR_CMD_RM      = 7,
    PHOSPHOR_CMD_CERTS   = 8,
    PHOSPHOR_CMD_GLOW    = 9,
    PHOSPHOR_CMD_SERVE     = 10,
    PHOSPHOR_CMD_FILAMENT  = 11,
};

/* ---- phosphor CLI configuration ---- */

extern const ph_cli_config_t phosphor_cli_config;

/* ---- command entry points ---- */

int ph_cmd_create(const ph_cli_config_t *config,
                  const ph_parsed_args_t *args);

int ph_cmd_build(const ph_cli_config_t *config,
                 const ph_parsed_args_t *args);

int ph_cmd_clean(const ph_cli_config_t *config,
                 const ph_parsed_args_t *args);

int ph_cmd_rm(const ph_cli_config_t *config,
              const ph_parsed_args_t *args);

int ph_cmd_certs(const ph_cli_config_t *config,
                 const ph_parsed_args_t *args);

int ph_cmd_doctor(const ph_cli_config_t *config,
                  const ph_parsed_args_t *args);

#ifdef PHOSPHOR_HAS_EMBEDDED
int ph_cmd_glow(const ph_cli_config_t *config,
                const ph_parsed_args_t *args);
#endif

int ph_cmd_serve(const ph_cli_config_t *config,
                 const ph_parsed_args_t *args);

int ph_cmd_filament(const ph_cli_config_t *config,
                    const ph_parsed_args_t *args);

#endif /* PHOSPHOR_COMMANDS_H */

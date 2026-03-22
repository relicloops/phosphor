#ifndef PHOSPHOR_CLI_H
#define PHOSPHOR_CLI_H

#include "phosphor/args.h"

int ph_cli_dispatch(const ph_cli_config_t *config,
                    const ph_parsed_args_t *args);
int ph_cli_help(const ph_cli_config_t *config, const char *topic);
int ph_cli_version(void);

#endif /* PHOSPHOR_CLI_H */

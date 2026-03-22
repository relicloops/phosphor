#include "phosphor/args.h"

#include <string.h>

/* ---- type name lookup ---- */

const char *ph_arg_type_name(ph_arg_type_t type) {
    switch (type) {
        case PH_TYPE_STRING: return "string";
        case PH_TYPE_INT:    return "int";
        case PH_TYPE_BOOL:   return "bool";
        case PH_TYPE_ENUM:   return "enum";
        case PH_TYPE_PATH:   return "path";
        case PH_TYPE_URL:    return "url";
        case PH_TYPE_KVP:    return "kvp";
    }
    return "(unknown)";
}

/* ---- config-driven command name lookup ---- */

const char *ph_cmd_def_name(const ph_cli_config_t *config, int command_id) {
    if (!config) return "(none)";
    for (size_t i = 0; i < config->command_count; i++) {
        if (config->commands[i].id == command_id) {
            return config->commands[i].name;
        }
    }
    return "(unknown)";
}

/* ---- config-driven spec registry ---- */

const ph_argspec_t *ph_cmd_def_specs(const ph_cli_config_t *config,
                                      int command_id, size_t *count) {
    if (!config) {
        if (count) *count = 0;
        return NULL;
    }
    for (size_t i = 0; i < config->command_count; i++) {
        if (config->commands[i].id == command_id) {
            if (count) *count = config->commands[i].spec_count;
            return config->commands[i].specs;
        }
    }
    if (count) *count = 0;
    return NULL;
}

const ph_argspec_t *ph_cmd_def_spec_lookup(const ph_cli_config_t *config,
                                            int command_id,
                                            const char *flag_name) {
    if (!flag_name) return NULL;

    size_t n = 0;
    const ph_argspec_t *specs = ph_cmd_def_specs(config, command_id, &n);
    if (!specs) return NULL;

    for (size_t i = 0; i < n; i++) {
        if (strcmp(specs[i].name, flag_name) == 0) {
            return &specs[i];
        }
    }
    return NULL;
}

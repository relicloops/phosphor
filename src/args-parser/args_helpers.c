#include "phosphor/args.h"

#include <string.h>

const char *ph_args_get_flag(const ph_parsed_args_t *args, const char *name) {
    if (!args || !name)
        return NULL;
    for (size_t i = 0; i < args->flag_count; i++) {
        if (strcmp(args->flags[i].name, name) == 0)
            return args->flags[i].value;
    }
    return NULL;
}

bool ph_args_has_flag(const ph_parsed_args_t *args, const char *name) {
    if (!args || !name)
        return false;
    for (size_t i = 0; i < args->flag_count; i++) {
        if (strcmp(args->flags[i].name, name) == 0)
            return true;
    }
    return false;
}

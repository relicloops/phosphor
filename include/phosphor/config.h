#ifndef PHOSPHOR_CONFIG_H
#define PHOSPHOR_CONFIG_H

#include "phosphor/types.h"
#include "phosphor/error.h"

/*
 * ph_config_t -- project-local configuration from .phosphor.toml or
 *                phosphor.toml found via walk-up discovery.
 *
 * ownership: all string fields heap-allocated, freed by ph_config_destroy.
 */
typedef struct {
    char   *file_path;     /* absolute path to the config file found */
    char  **keys;
    char  **values;
    size_t  count;
    size_t  cap;
} ph_config_t;

/*
 * ph_config_discover -- walk from start_dir upward to '/' looking for
 * .phosphor.toml or phosphor.toml. missing config is not an error
 * (out->file_path will be NULL). malformed config -> exit 3.
 */
ph_result_t ph_config_discover(const char *start_dir, ph_config_t *out,
                                ph_error_t **err);

/*
 * ph_config_get -- look up a key. returns NULL if not found.
 */
const char *ph_config_get(const ph_config_t *cfg, const char *key);

/*
 * ph_config_set -- insert or update a key-value pair.
 * both key and value are copied. overwrites if key exists.
 */
ph_result_t ph_config_set(ph_config_t *cfg, const char *key, const char *value);

void ph_config_destroy(ph_config_t *cfg);

#endif /* PHOSPHOR_CONFIG_H */

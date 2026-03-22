#ifndef PHOSPHOR_ACME_JSON_H
#define PHOSPHOR_ACME_JSON_H

#include <stddef.h>

/*
 * minimal JSON extraction helpers for ACME protocol responses.
 * internal header -- not part of the public API.
 */

/*
 * json_extract_string -- extract a string value for a given key.
 * returns a heap-allocated copy (free with ph_free), or NULL if not found.
 */
char *json_extract_string(const char *json, const char *key);

/*
 * json_extract_string_array -- extract an array of strings for a given key.
 * returns a heap-allocated array of heap-allocated strings.
 * caller frees each element with ph_free, then the array itself.
 */
char **json_extract_string_array(const char *json, const char *key,
                                   size_t *out_count);

#endif /* PHOSPHOR_ACME_JSON_H */

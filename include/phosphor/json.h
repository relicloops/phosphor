#ifndef PH_JSON_H
#define PH_JSON_H

#ifdef PHOSPHOR_HAS_CJSON

#include "phosphor/types.h"
#include <stddef.h>

typedef struct cJSON ph_json_t;

/* parse */
ph_json_t  *ph_json_parse(const char *json_str);
ph_json_t  *ph_json_parse_file(const char *path);

/* read */
char       *ph_json_get_string(const ph_json_t *root, const char *key);
char      **ph_json_get_string_array(const ph_json_t *root, const char *key,
                                      size_t *out_count);
ph_json_t  *ph_json_get_object(const ph_json_t *root, const char *key);
int         ph_json_get_int(const ph_json_t *root, const char *key,
                             int fallback);

/* write */
ph_json_t  *ph_json_create_object(void);
ph_json_t  *ph_json_add_object(ph_json_t *parent, const char *key);
void        ph_json_add_string(ph_json_t *parent, const char *key,
                                const char *val);
char       *ph_json_print(const ph_json_t *root);

/* lifecycle */
void        ph_json_destroy(ph_json_t *root);
void        ph_json_free_string(char *str);

#endif /* PHOSPHOR_HAS_CJSON */
#endif /* PH_JSON_H */

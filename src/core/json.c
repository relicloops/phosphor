#ifdef PHOSPHOR_HAS_CJSON

#include "phosphor/json.h"
#include "phosphor/alloc.h"

#include <cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- parse ---- */

ph_json_t *ph_json_parse(const char *json_str) {
    if (!json_str) return NULL;
    return (ph_json_t *)cJSON_Parse(json_str);
}

ph_json_t *ph_json_parse_file(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) { fclose(f); return NULL; }

    char *buf = ph_alloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';

    ph_json_t *root = (ph_json_t *)cJSON_Parse(buf);
    ph_free(buf);
    return root;
}

/* ---- read ---- */

char *ph_json_get_string(const ph_json_t *root, const char *key) {
    if (!root || !key) return NULL;

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(
        (const cJSON *)root, key);
    if (!item || !cJSON_IsString(item) || !item->valuestring)
        return NULL;

    size_t len = strlen(item->valuestring);
    char *copy = ph_alloc(len + 1);
    if (copy) {
        memcpy(copy, item->valuestring, len);
        copy[len] = '\0';
    }
    return copy;
}

char **ph_json_get_string_array(const ph_json_t *root, const char *key,
                                 size_t *out_count) {
    if (out_count) *out_count = 0;
    if (!root || !key || !out_count) return NULL;

    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(
        (const cJSON *)root, key);
    if (!arr || !cJSON_IsArray(arr)) return NULL;

    int n = cJSON_GetArraySize(arr);
    if (n <= 0) return NULL;

    char **result = ph_calloc((size_t)n, sizeof(char *));
    if (!result) return NULL;

    size_t idx = 0;
    const cJSON *elem = NULL;
    cJSON_ArrayForEach(elem, arr) {
        if (cJSON_IsString(elem) && elem->valuestring) {
            size_t len = strlen(elem->valuestring);
            result[idx] = ph_alloc(len + 1);
            if (result[idx]) {
                memcpy(result[idx], elem->valuestring, len);
                result[idx][len] = '\0';
            }
            idx++;
        }
    }

    *out_count = idx;
    return result;
}

ph_json_t *ph_json_get_object(const ph_json_t *root, const char *key) {
    if (!root || !key) return NULL;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(
        (const cJSON *)root, key);
    if (!item || !cJSON_IsObject(item)) return NULL;
    return (ph_json_t *)item;
}

int ph_json_get_int(const ph_json_t *root, const char *key, int fallback) {
    if (!root || !key) return fallback;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(
        (const cJSON *)root, key);
    if (!item || !cJSON_IsNumber(item)) return fallback;
    return item->valueint;
}

/* ---- write ---- */

ph_json_t *ph_json_create_object(void) {
    return (ph_json_t *)cJSON_CreateObject();
}

ph_json_t *ph_json_add_object(ph_json_t *parent, const char *key) {
    if (!parent || !key) return NULL;
    cJSON *obj = cJSON_AddObjectToObject((cJSON *)parent, key);
    return (ph_json_t *)obj;
}

void ph_json_add_string(ph_json_t *parent, const char *key,
                          const char *val) {
    if (!parent || !key || !val) return;
    cJSON_AddStringToObject((cJSON *)parent, key, val);
}

char *ph_json_print(const ph_json_t *root) {
    if (!root) return NULL;
    return cJSON_Print((const cJSON *)root);
}

/* ---- lifecycle ---- */

void ph_json_destroy(ph_json_t *root) {
    if (root) cJSON_Delete((cJSON *)root);
}

void ph_json_free_string(char *str) {
    if (str) cJSON_free(str);
}

#endif /* PHOSPHOR_HAS_CJSON */

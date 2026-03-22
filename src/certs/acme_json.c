#include "acme_json.h"
#include "phosphor/alloc.h"

#include <stdio.h>
#include <string.h>

char *json_extract_string(const char *json, const char *key) {
    if (!json || !key) return NULL;

    size_t klen = strlen(key);
    size_t buf_len = klen + 4;
    char *pattern = ph_alloc(buf_len);
    if (!pattern) return NULL;
    snprintf(pattern, buf_len, "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    ph_free(pattern);
    if (!pos) return NULL;

    pos += klen + 2;
    while (*pos == ' ' || *pos == ':' || *pos == '\t') pos++;

    if (*pos != '"') return NULL;
    pos++; /* skip opening quote */

    const char *end = strchr(pos, '"');
    if (!end) return NULL;

    size_t vlen = (size_t)(end - pos);
    char *val = ph_alloc(vlen + 1);
    if (val) {
        memcpy(val, pos, vlen);
        val[vlen] = '\0';
    }
    return val;
}

char **json_extract_string_array(const char *json, const char *key,
                                   size_t *out_count) {
    *out_count = 0;
    if (!json || !key) return NULL;

    size_t klen = strlen(key);
    size_t pat_len = klen + 4;
    char *pattern = ph_alloc(pat_len);
    if (!pattern) return NULL;
    snprintf(pattern, pat_len, "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    ph_free(pattern);
    if (!pos) return NULL;

    pos += klen + 2;
    while (*pos == ' ' || *pos == ':' || *pos == '\t') pos++;
    if (*pos != '[') return NULL;
    pos++; /* skip [ */

    /* count strings */
    size_t count = 0;
    const char *scan = pos;
    while (*scan && *scan != ']') {
        if (*scan == '"') count++;
        scan++;
    }
    count /= 2; /* each string has opening + closing quote */
    if (count == 0) return NULL;

    char **arr = ph_calloc(count, sizeof(char *));
    if (!arr) return NULL;

    size_t idx = 0;
    const char *p = pos;
    while (*p && *p != ']' && idx < count) {
        if (*p == '"') {
            p++; /* skip opening quote */
            const char *end = strchr(p, '"');
            if (!end) break;
            size_t vlen = (size_t)(end - p);
            arr[idx] = ph_alloc(vlen + 1);
            if (arr[idx]) {
                memcpy(arr[idx], p, vlen);
                arr[idx][vlen] = '\0';
            }
            idx++;
            p = end + 1;
        } else {
            p++;
        }
    }

    *out_count = idx;
    return arr;
}

#include "phosphor/render.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <string.h>

ph_result_t ph_render_template(const uint8_t *data, size_t len,
                                const ph_resolved_var_t *vars,
                                size_t var_count,
                                uint8_t **out_data, size_t *out_len,
                                ph_error_t **err) {
    if (!data || !out_data || !out_len) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_render_template: NULL argument");
        return PH_ERR;
    }

    /* allocate output buffer (start at 2x input for growth room) */
    size_t cap = len * 2 + 256;
    uint8_t *buf = ph_alloc(cap);
    if (!buf) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0, "allocation failed");
        return PH_ERR;
    }

    size_t wi = 0;
    size_t ri = 0;

    while (ri < len) {
        /* check for escaped marker: \<< */
        if (ri + 2 < len && data[ri] == '\\' &&
            data[ri + 1] == '<' && data[ri + 2] == '<') {
            /* emit literal << */
            if (wi + 2 >= cap) {
                cap *= 2;
                buf = ph_realloc(buf, cap);
                if (!buf) goto oom;
            }
            buf[wi++] = '<';
            buf[wi++] = '<';
            ri += 3;
            continue;
        }

        /* check for placeholder: <<name>> */
        if (ri + 3 < len && data[ri] == '<' && data[ri + 1] == '<') {
            /* find closing >> */
            size_t end = ri + 2;
            while (end + 1 < len) {
                if (data[end] == '>' && data[end + 1] == '>') break;
                end++;
            }

            if (end + 1 < len && data[end] == '>' && data[end + 1] == '>') {
                /* extract variable name */
                size_t name_len = end - (ri + 2);
                char name[256];
                if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
                memcpy(name, data + ri + 2, name_len);
                name[name_len] = '\0';

                /* look up variable */
                const char *value = ph_resolved_var_get(vars, var_count, name);
                if (value) {
                    size_t vlen = strlen(value);
                    while (wi + vlen >= cap) {
                        cap *= 2;
                        buf = ph_realloc(buf, cap);
                        if (!buf) goto oom;
                    }
                    memcpy(buf + wi, value, vlen);
                    wi += vlen;
                } else {
                    /* unresolved: keep placeholder as-is */
                    size_t plen = end + 2 - ri;
                    while (wi + plen >= cap) {
                        cap *= 2;
                        buf = ph_realloc(buf, cap);
                        if (!buf) goto oom;
                    }
                    memcpy(buf + wi, data + ri, plen);
                    wi += plen;
                    ph_log_warn("unresolved variable: %s", name);
                }
                ri = end + 2;
                continue;
            }
        }

        /* regular byte */
        if (wi >= cap) {
            cap *= 2;
            buf = ph_realloc(buf, cap);
            if (!buf) goto oom;
        }
        buf[wi++] = data[ri++];
    }

    *out_data = buf;
    *out_len = wi;
    return PH_OK;

oom:
    if (err)
        *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                 "render: allocation failed");
    return PH_ERR;
}

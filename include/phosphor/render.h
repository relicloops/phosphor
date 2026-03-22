#ifndef PHOSPHOR_RENDER_H
#define PHOSPHOR_RENDER_H

#include "phosphor/types.h"
#include "phosphor/error.h"
#include "phosphor/template.h"

/*
 * ph_render_template -- substitute <<var_name>> placeholders in data.
 * escape: \<< produces literal <<.
 * out_data and out_len receive the rendered result (caller frees with ph_free).
 */
ph_result_t ph_render_template(const uint8_t *data, size_t len,
                                const ph_resolved_var_t *vars,
                                size_t var_count,
                                uint8_t **out_data, size_t *out_len,
                                ph_error_t **err);

/*
 * ph_transform_is_binary -- returns true if data appears to be binary.
 * heuristic: NUL byte in first 8192 bytes, or known binary extension.
 */
bool ph_transform_is_binary(const uint8_t *data, size_t len,
                             const char *extension);

/*
 * ph_transform_newline -- normalize newlines in data.
 * mode: "lf", "crlf", or "keep" (no-op).
 * out_data and out_len receive result (caller frees with ph_free).
 */
ph_result_t ph_transform_newline(const uint8_t *data, size_t len,
                                  const char *mode,
                                  uint8_t **out_data, size_t *out_len);

#endif /* PHOSPHOR_RENDER_H */

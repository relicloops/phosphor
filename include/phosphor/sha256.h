#ifndef PHOSPHOR_SHA256_H
#define PHOSPHOR_SHA256_H

#include "phosphor/types.h"
#include "phosphor/error.h"

/*
 * SHA256 hash utilities for checksum verification.
 *
 * uses a vendored public-domain implementation (no external dependencies).
 * reads files in 8KB chunks for bounded memory usage.
 */

/* hex digest length: 64 hex chars + NUL */
#define PH_SHA256_HEX_LEN 65

/*
 * ph_sha256_file -- compute SHA256 hash of a file.
 *
 * writes a 64-character lowercase hex string to out_hex (must be >= 65 bytes).
 *
 * on error: PH_ERR with err->category = PH_ERR_FS.
 */
ph_result_t ph_sha256_file(const char *path, char *out_hex, ph_error_t **err);

/*
 * ph_sha256_verify -- verify file matches expected SHA256 hash.
 *
 * expected_hex must be a 64-character lowercase hex string.
 *
 * on mismatch: PH_ERR with err->category = PH_ERR_VALIDATE.
 * on I/O error: PH_ERR with err->category = PH_ERR_FS.
 */
ph_result_t ph_sha256_verify(const char *path, const char *expected_hex,
                              ph_error_t **err);

#endif /* PHOSPHOR_SHA256_H */

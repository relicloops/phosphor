#ifndef PHOSPHOR_ARCHIVE_H
#define PHOSPHOR_ARCHIVE_H

#include "phosphor/types.h"
#include "phosphor/error.h"

/*
 * optional archive template extraction.
 *
 * format detection and cleanup are always available. the actual extraction
 * requires libarchive (compile with -Dlibarchive=true).
 */

/* ---- archive format detection ---- */

typedef enum {
    PH_ARCHIVE_NONE    = 0,
    PH_ARCHIVE_TAR_GZ  = 1,
    PH_ARCHIVE_TAR_ZST = 2,
    PH_ARCHIVE_ZIP     = 3
} ph_archive_format_t;

/* ---- always available (no libarchive dependency) ---- */

/*
 * ph_archive_detect -- detect archive format from file extension.
 *
 * recognized extensions:
 *   .tar.gz, .tgz     -> PH_ARCHIVE_TAR_GZ
 *   .tar.zst           -> PH_ARCHIVE_TAR_ZST
 *   .zip               -> PH_ARCHIVE_ZIP
 *
 * returns PH_ARCHIVE_NONE for unrecognized or NULL input.
 */
ph_archive_format_t ph_archive_detect(const char *path);

/*
 * ph_archive_cleanup_extract -- remove a temporary extraction directory.
 * delegates to ph_fs_rmtree(). safe to call with NULL (no-op).
 */
ph_result_t ph_archive_cleanup_extract(const char *extract_path,
                                        ph_error_t **err);

/* ---- libarchive-dependent (compile with -Dlibarchive=true) ---- */

#ifdef PHOSPHOR_HAS_LIBARCHIVE

/*
 * ph_archive_extract -- extract an archive to a temporary directory.
 *
 * creates a temporary directory under /tmp:
 *   /tmp/.phosphor-extract-<pid>-<timestamp>/
 *
 * if checksum is non-NULL, it must be in the format "sha256:<64-hex-chars>".
 * the file is verified before extraction begins. mismatch -> PH_ERR_VALIDATE.
 *
 * security:
 *   - rejects entries with path traversal ("..") -> PH_ERR_VALIDATE
 *   - rejects entries with absolute paths -> PH_ERR_VALIDATE
 *   - checks ph_signal_interrupted() between entries
 *
 * on success:
 *   *out_path is heap-allocated absolute path to the extraction directory.
 *   caller owns *out_path and must ph_free() it after use.
 *   caller must also call ph_archive_cleanup_extract(*out_path) when done.
 *
 * on error:
 *   PH_ERR_VALIDATE (6) for checksum mismatch or path traversal.
 *   PH_ERR_FS (4) for temp dir creation or write failures.
 *   PH_ERR_CONFIG (3) for corrupt/unsupported archives.
 *   PH_ERR_SIGNAL (8) for signal interruption.
 */
ph_result_t ph_archive_extract(const char *archive_path,
                                const char *checksum,
                                char **out_path,
                                ph_error_t **err);

#endif /* PHOSPHOR_HAS_LIBARCHIVE */

#endif /* PHOSPHOR_ARCHIVE_H */

#ifndef PHOSPHOR_FS_H
#define PHOSPHOR_FS_H

#include "phosphor/types.h"
#include "phosphor/error.h"

#include <sys/types.h>

/*
 * higher-level filesystem operations built on top of platform.h primitives.
 */

/* ---- byte-safe read/write wrappers ---- */

/*
 * ph_io_read_file -- read file into byte slice with error reporting.
 * caller owns *out_data (free with ph_free).
 */
ph_result_t ph_io_read_file(const char *path, uint8_t **out_data,
                             size_t *out_len, ph_error_t **err);

/*
 * ph_io_write_file -- write data to file with error reporting.
 */
ph_result_t ph_io_write_file(const char *path, const uint8_t *data,
                              size_t len, ph_error_t **err);

/* ---- atomic write ---- */

/*
 * ph_fs_atomic_write -- write via temp file + fsync + rename.
 * EXDEV fallback: copy + remove source, logs at WARN.
 */
ph_result_t ph_fs_atomic_write(const char *path, const uint8_t *data,
                                size_t len, ph_error_t **err);

/* ---- recursive copy ---- */

/*
 * ph_fs_filter_fn -- callback for copytree filtering.
 * return true to include the file, false to skip it.
 */
typedef bool (*ph_fs_filter_fn)(const char *rel_path, bool is_dir,
                                 void *ctx);

/*
 * ph_fs_copytree -- recursive directory copy with filter callback.
 * checks ph_signal_interrupted() between files.
 * respects PH_MAX_DIR_DEPTH from path.h.
 */
ph_result_t ph_fs_copytree(const char *src, const char *dst,
                            ph_fs_filter_fn filter, void *filter_ctx,
                            ph_error_t **err);

/* ---- recursive removal ---- */

/*
 * ph_fs_rmtree -- recursively remove a directory tree.
 * checks ph_signal_interrupted() between entries.
 */
ph_result_t ph_fs_rmtree(const char *path, ph_error_t **err);

/* ---- metadata filter ---- */

/*
 * ph_metadata_is_denied -- returns true if the basename matches the
 * platform metadata deny list (.DS_Store, ._*, Thumbs.db, etc.).
 */
bool ph_metadata_is_denied(const char *basename);

#endif /* PHOSPHOR_FS_H */

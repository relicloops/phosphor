#ifndef PHOSPHOR_PATH_H
#define PHOSPHOR_PATH_H

#include "phosphor/types.h"

/*
 * path normalization and safety utilities.
 *
 * all returned strings are heap-allocated (ph_alloc) and must be freed
 * by the caller with ph_free().
 */

/* max nesting depth for recursive directory operations */
#define PH_MAX_DIR_DEPTH 32

/*
 * ph_path_normalize -- collapse "." and redundant separators.
 * does NOT resolve ".." (use ph_path_has_traversal for safety checks).
 * returns heap-allocated normalized path, or NULL on error.
 */
char *ph_path_normalize(const char *path);

/*
 * ph_path_has_traversal -- returns true if path contains ".." components.
 */
bool ph_path_has_traversal(const char *path);

/*
 * ph_path_is_absolute -- returns true if path starts with '/'.
 */
bool ph_path_is_absolute(const char *path);

/*
 * ph_path_join -- join two path segments with '/'.
 * returns heap-allocated result or NULL on error.
 */
char *ph_path_join(const char *base, const char *rel);

/*
 * ph_path_dirname -- return directory portion of path.
 * returns heap-allocated result or NULL on error.
 */
char *ph_path_dirname(const char *path);

/*
 * ph_path_basename -- return filename portion of path.
 * returns heap-allocated result or NULL on error.
 */
char *ph_path_basename(const char *path);

/*
 * ph_path_extension -- return file extension including the dot.
 * returns pointer into the input string (not heap-allocated), or NULL.
 */
const char *ph_path_extension(const char *path);

#endif /* PHOSPHOR_PATH_H */

#ifndef PHOSPHOR_GIT_FETCH_H
#define PHOSPHOR_GIT_FETCH_H

#include "phosphor/types.h"
#include "phosphor/error.h"

/*
 * optional remote git template fetching.
 *
 * URL parsing and cleanup are always available. the actual clone
 * operation requires libgit2 (compile with -Dlibgit2=true).
 */

/* ---- parsed URL ---- */

/*
 * ph_git_url_t -- parsed git template URL.
 *
 * ownership:
 *   url  -- owner: self (heap-allocated, freed by ph_git_url_destroy)
 *   ref  -- owner: self (heap-allocated, may be NULL)
 *
 * examples:
 *   "https://github.com/user/repo"       -> url=..., ref=NULL
 *   "https://github.com/user/repo#v1.0"  -> url=..., ref="v1.0"
 *   "https://github.com/user/repo#main"  -> url=..., ref="main"
 */
typedef struct {
    char *url;   /* base URL without fragment */
    char *ref;   /* branch/tag/commit ref, or NULL for default branch */
} ph_git_url_t;

/* ---- always available (no libgit2 dependency) ---- */

/*
 * ph_git_is_url -- returns true if the string looks like a git URL
 * (starts with http:// or https://).
 */
bool ph_git_is_url(const char *s);

/*
 * ph_git_url_parse -- split a template URL into base URL and optional ref.
 *
 * the '#' character separates the URL from the ref:
 *   "https://github.com/user/repo#branch" -> url + ref
 *   "https://github.com/user/repo"        -> url + NULL ref
 *
 * validates:
 *   - URL starts with http:// or https://
 *   - ref (if present) is not empty
 *   - ref does not contain path traversal
 *
 * on error: PH_ERR with err->category = PH_ERR_CONFIG.
 * caller must call ph_git_url_destroy() on success.
 */
ph_result_t ph_git_url_parse(const char *input, ph_git_url_t *out,
                              ph_error_t **err);

void ph_git_url_destroy(ph_git_url_t *parsed);

/*
 * ph_git_cleanup_clone -- remove a temporary clone directory.
 * delegates to ph_fs_rmtree(). safe to call with NULL (no-op).
 */
ph_result_t ph_git_cleanup_clone(const char *clone_path, ph_error_t **err);

/* ---- libgit2-dependent (compile with -Dlibgit2=true) ---- */

#ifdef PHOSPHOR_HAS_LIBGIT2

/*
 * ph_git_fetch_template -- clone a remote git repository to a local
 * temporary directory.
 *
 * creates a temporary directory under /tmp:
 *   /tmp/.phosphor-clone-<pid>-<timestamp>/
 *
 * if parsed->ref is non-NULL, checks out that branch/tag/commit.
 *
 * on success:
 *   *out_path is heap-allocated absolute path to the clone directory.
 *   caller owns *out_path and must ph_free() it after use.
 *   caller must also call ph_git_cleanup_clone(*out_path) when done.
 *
 * on error:
 *   PH_ERR_PROCESS (5) for network/clone failures.
 *   PH_ERR_FS (4) for temp directory creation failures.
 *   PH_ERR_CONFIG (3) for ref resolution failures.
 *
 * checks ph_signal_interrupted() during clone.
 */
ph_result_t ph_git_fetch_template(const ph_git_url_t *parsed,
                                   char **out_path,
                                   ph_error_t **err);

#endif /* PHOSPHOR_HAS_LIBGIT2 */

#endif /* PHOSPHOR_GIT_FETCH_H */

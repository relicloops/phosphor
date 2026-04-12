#include "phosphor/git_fetch.h"
#include "phosphor/alloc.h"
#include "phosphor/path.h"
#include "phosphor/fs.h"
#include "phosphor/platform.h"
#include "phosphor/signal.h"
#include "phosphor/log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* mkdtemp is POSIX but not exposed under strict c17 + _POSIX_C_SOURCE=200809L
 * on all platforms; provide an explicit declaration. */
extern char *mkdtemp(char *tmpl);

#ifdef PHOSPHOR_HAS_LIBGIT2
#include <git2.h>
#endif

#define CLONE_PREFIX ".phosphor-clone-"

/* ---- always available (no libgit2 dependency) ---- */

bool ph_git_is_url(const char *s) {
    if (!s) return false;
    return strncmp(s, "https://", 8) == 0
        || strncmp(s, "http://", 7) == 0;
}

ph_result_t ph_git_url_parse(const char *input, ph_git_url_t *out,
                              ph_error_t **err) {
    if (!input || !out) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_git_url_parse: NULL argument");
        return PH_ERR;
    }

    memset(out, 0, sizeof(*out));

    /* validate scheme */
    if (strncmp(input, "https://", 8) != 0) {
        if (err) {
            if (strncmp(input, "http://", 7) == 0) {
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "insecure http:// git URLs are rejected. "
                    "use https:// instead: %s", input);
            } else {
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                    "invalid git URL scheme: %s "
                    "(expected https://)", input);
            }
        }
        return PH_ERR;
    }

    /* find fragment separator */
    const char *hash = strchr(input, '#');

    if (hash) {
        size_t url_len = (size_t)(hash - input);
        out->url = ph_alloc(url_len + 1);
        if (!out->url) return PH_ERR;
        memcpy(out->url, input, url_len);
        out->url[url_len] = '\0';

        const char *ref_start = hash + 1;
        size_t ref_len = strlen(ref_start);

        if (ref_len == 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                    "empty ref after '#' in URL: %s", input);
            ph_free(out->url);
            out->url = NULL;
            return PH_ERR;
        }

        if (ph_path_has_traversal(ref_start)) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                    "path traversal in ref: %s", ref_start);
            ph_free(out->url);
            out->url = NULL;
            return PH_ERR;
        }

        out->ref = ph_alloc(ref_len + 1);
        if (!out->ref) {
            ph_free(out->url);
            out->url = NULL;
            return PH_ERR;
        }
        memcpy(out->ref, ref_start, ref_len + 1);
    } else {
        size_t len = strlen(input);
        out->url = ph_alloc(len + 1);
        if (!out->url) return PH_ERR;
        memcpy(out->url, input, len + 1);
        out->ref = NULL;
    }

    return PH_OK;
}

void ph_git_url_destroy(ph_git_url_t *parsed) {
    if (!parsed) return;
    ph_free(parsed->url);
    ph_free(parsed->ref);
    parsed->url = NULL;
    parsed->ref = NULL;
}

ph_result_t ph_git_cleanup_clone(const char *clone_path, ph_error_t **err) {
    if (!clone_path) return PH_OK;
    ph_log_debug("git: cleaning up clone directory: %s", clone_path);
    return ph_fs_rmtree(clone_path, err);
}

/* ---- libgit2-dependent clone implementation ---- */

#ifdef PHOSPHOR_HAS_LIBGIT2

static bool g_git2_initialized = false;

static ph_result_t ensure_git2_init(ph_error_t **err) {
    if (g_git2_initialized) return PH_OK;

    int rc = git_libgit2_init();
    if (rc < 0) {
        if (err) {
            const git_error *e = git_error_last();
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "failed to initialize libgit2: %s",
                e ? e->message : "unknown error");
        }
        return PH_ERR;
    }
    g_git2_initialized = true;
    return PH_OK;
}

static int fetch_progress_cb(const git_indexer_progress *stats, void *payload) {
    (void)payload;
    if (ph_signal_interrupted())
        return -1;
    if (stats->total_objects > 0) {
        ph_log_debug("git: fetching objects %u/%u",
                     stats->received_objects, stats->total_objects);
    }
    return 0;
}

/*
 * try_checkout_ref -- resolve ref as tag or commit and check it out.
 * called when the initial clone with checkout_branch fails.
 */
static ph_result_t try_checkout_ref(git_repository *repo, const char *ref,
                                     ph_error_t **err) {
    git_object *target = NULL;
    int rc = git_revparse_single(&target, repo, ref);
    if (rc < 0) {
        if (err) {
            const git_error *e = git_error_last();
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "cannot resolve ref '%s': %s",
                ref, e ? e->message : "unknown error");
        }
        return PH_ERR;
    }

    /* suppress -Wmissing-field-initializers from libgit2 macros */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    git_checkout_options co_opts = GIT_CHECKOUT_OPTIONS_INIT;
#pragma GCC diagnostic pop
    co_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    rc = git_checkout_tree(repo, target, &co_opts);
    git_object_free(target);

    if (rc < 0) {
        if (err) {
            const git_error *e = git_error_last();
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "git checkout failed for ref '%s': %s",
                ref, e ? e->message : "unknown error");
        }
        return PH_ERR;
    }

    /* detach HEAD to the resolved commit */
    git_object *resolved = NULL;
    rc = git_revparse_single(&resolved, repo, ref);
    if (rc == 0) {
        git_repository_set_head_detached(repo, git_object_id(resolved));
        git_object_free(resolved);
    }

    return PH_OK;
}

ph_result_t ph_git_fetch_template(const ph_git_url_t *parsed,
                                   char **out_path,
                                   ph_error_t **err) {
    if (!parsed || !parsed->url || !out_path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_git_fetch_template: NULL argument");
        return PH_ERR;
    }

    *out_path = NULL;

    if (ensure_git2_init(err) != PH_OK)
        return PH_ERR;

    /* build temporary clone directory via mkdtemp.
     * audit fix: the previous pid+time template was predictable and a local
     * attacker could pre-create the path with a symlink. mkdtemp atomically
     * creates a unique 0700 directory. libgit2's git_clone accepts an empty
     * pre-existing directory so this drop-in works as before. */
    char clone_dir[] = "/tmp/" CLONE_PREFIX "XXXXXX";
    if (mkdtemp(clone_dir) == NULL) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create clone directory via mkdtemp");
        return PH_ERR;
    }

    if (ph_signal_interrupted()) {
        ph_git_cleanup_clone(clone_dir, NULL);
        return PH_ERR;
    }

    /* configure clone options */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
#pragma GCC diagnostic pop
    clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    clone_opts.fetch_opts.callbacks.transfer_progress = fetch_progress_cb;

    if (parsed->ref)
        clone_opts.checkout_branch = parsed->ref;

    ph_log_info("git: cloning %s%s%s",
                parsed->url,
                parsed->ref ? " (ref: " : "",
                parsed->ref ? parsed->ref : "");

    git_repository *repo = NULL;
    int git_rc = git_clone(&repo, parsed->url, clone_dir, &clone_opts);

    if (git_rc < 0 && parsed->ref) {
        const git_error *e = git_error_last();
        ph_log_debug("git: branch '%s' not found (%s), trying as tag/commit",
                     parsed->ref, e ? e->message : "");

        /* clean and retry without checkout_branch */
        ph_git_cleanup_clone(clone_dir, NULL);
        if (ph_fs_mkdir_p(clone_dir, 0755) != PH_OK) {
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                    "cannot recreate clone directory: %s", clone_dir);
            return PH_ERR;
        }

        clone_opts.checkout_branch = NULL;
        git_rc = git_clone(&repo, parsed->url, clone_dir, &clone_opts);

        if (git_rc < 0) {
            e = git_error_last();
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "git clone failed: %s",
                    e ? e->message : "unknown error");
            ph_git_cleanup_clone(clone_dir, NULL);
            return PH_ERR;
        }

        /* resolve the ref as tag or commit */
        if (try_checkout_ref(repo, parsed->ref, err) != PH_OK) {
            git_repository_free(repo);
            ph_git_cleanup_clone(clone_dir, NULL);
            return PH_ERR;
        }
    } else if (git_rc < 0) {
        const git_error *e = git_error_last();
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "git clone failed: %s",
                e ? e->message : "unknown error");
        ph_git_cleanup_clone(clone_dir, NULL);
        return PH_ERR;
    }

    git_repository_free(repo);

    /* transfer ownership of clone path to caller */
    size_t dir_len = strlen(clone_dir);
    *out_path = ph_alloc(dir_len + 1);
    if (!*out_path) {
        ph_git_cleanup_clone(clone_dir, NULL);
        return PH_ERR;
    }
    memcpy(*out_path, clone_dir, dir_len + 1);

    ph_log_info("git: clone complete: %s", clone_dir);
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBGIT2 */

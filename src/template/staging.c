#include "phosphor/template.h"
#include "phosphor/fs.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

/* mkdtemp is POSIX but not exposed under strict c17 + _POSIX_C_SOURCE=200809L
 * on all platforms; provide an explicit declaration. */
extern char *mkdtemp(char *tmpl);

#define STAGING_PREFIX ".phosphor-staging-"

ph_result_t ph_staging_create(const char *dest_path, bool force,
                               ph_staging_t *out, ph_error_t **err) {
    if (!dest_path || !out) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_staging_create: NULL argument");
        return PH_ERR;
    }

    memset(out, 0, sizeof(*out));
    out->force = force;

    /* parent directory of destination */
    char *parent = ph_path_dirname(dest_path);
    if (!parent) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot determine parent of: %s", dest_path);
        return PH_ERR;
    }

    /* ensure parent exists */
    if (ph_fs_mkdir_p(parent, 0755) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create parent directory: %s", parent);
        ph_free(parent);
        return PH_ERR;
    }

    /* build staging dir template and create via mkdtemp.
     * audit fix: the previous pid+timestamp template was predictable and a
     * local attacker could pre-create the path with a symlink, hijacking the
     * staging dir. mkdtemp atomically creates a unique 0700 directory that
     * cannot be pre-empted. Since the parent varies per-call, the template
     * must be heap-allocated rather than stack. */
    const char *sfx = STAGING_PREFIX "XXXXXX";
    size_t parent_len = strlen(parent);
    size_t sfx_len = strlen(sfx);
    char *staging_path = ph_alloc(parent_len + 1 + sfx_len + 1);
    if (!staging_path) {
        ph_free(parent);
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "failed to build staging path");
        return PH_ERR;
    }
    memcpy(staging_path, parent, parent_len);
    staging_path[parent_len] = '/';
    memcpy(staging_path + parent_len + 1, sfx, sfx_len + 1);
    ph_free(parent);

    if (mkdtemp(staging_path) == NULL) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create staging directory via mkdtemp: %s",
                staging_path);
        ph_free(staging_path);
        return PH_ERR;
    }

    out->path = staging_path;
    out->dest_path = ph_alloc(strlen(dest_path) + 1);
    if (out->dest_path)
        memcpy(out->dest_path, dest_path, strlen(dest_path) + 1);
    out->active = true;

    ph_log_debug("staging created: %s", staging_path);
    return PH_OK;
}

ph_result_t ph_staging_commit(ph_staging_t *staging, ph_error_t **err) {
    if (!staging || !staging->active || !staging->path || !staging->dest_path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_staging_commit: invalid staging state");
        return PH_ERR;
    }

    /* audit fix (2026-04-07T20-37-55Z): bypass ph_fs_rename so we can
     * inspect errno from rename(2) directly. The previous implementation
     * fell back to copytree on ANY rename failure, which silently turned
     * --force into a merge instead of a replace and masked unrelated
     * rename errors. */
    if (rename(staging->path, staging->dest_path) == 0) {
        staging->active = false;
        ph_log_info("staging committed: %s -> %s",
                    staging->path, staging->dest_path);
        return PH_OK;
    }
    int saved_errno = errno;

    /* dest already exists. with --force, replace it; otherwise hard error. */
    if (saved_errno == EEXIST || saved_errno == ENOTEMPTY ||
        saved_errno == ENOTDIR) {
        if (!staging->force) {
            if (err)
                *err = ph_error_createf(PH_ERR_FS, saved_errno,
                    "staging commit: destination already exists: %s "
                    "(rerun with --force to replace)", staging->dest_path);
            return PH_ERR;
        }

        ph_log_info("staging commit: removing existing destination "
                     "before replace: %s", staging->dest_path);

        ph_error_t *rm_err = NULL;
        if (ph_fs_rmtree(staging->dest_path, &rm_err) != PH_OK) {
            if (err) {
                *err = ph_error_createf(PH_ERR_FS, 0,
                    "staging commit: cannot remove existing destination "
                    "'%s': %s", staging->dest_path,
                    rm_err && rm_err->message ? rm_err->message : "unknown");
            }
            ph_error_destroy(rm_err);
            return PH_ERR;
        }
        ph_error_destroy(rm_err);

        if (rename(staging->path, staging->dest_path) == 0) {
            staging->active = false;
            ph_log_info("staging committed (replaced): %s -> %s",
                        staging->path, staging->dest_path);
            return PH_OK;
        }
        saved_errno = errno;
        /* fall through: a non-EXDEV failure after replace is a real error */
    }

    /* EXDEV: legitimate cross-device case, fall back to copytree. */
    if (saved_errno == EXDEV) {
        ph_log_warn("staging rename hit EXDEV (cross-device), "
                     "falling back to copytree");

        if (ph_fs_copytree(staging->path, staging->dest_path,
                            NULL, NULL, err) != PH_OK)
            return PH_ERR;

        /* remove staging dir */
        ph_error_t *rm_err = NULL;
        if (ph_fs_rmtree(staging->path, &rm_err) != PH_OK) {
            ph_log_warn("failed to remove staging after copy: %s",
                        rm_err && rm_err->message ? rm_err->message : "unknown");
            ph_error_destroy(rm_err);
            /* not fatal -- files are committed */
        }

        staging->active = false;
        ph_log_info("staging committed (copy fallback): %s -> %s",
                    staging->path, staging->dest_path);
        return PH_OK;
    }

    /* any other errno is a real error and must NOT be silently downgraded
     * into a merging copy. */
    if (err)
        *err = ph_error_createf(PH_ERR_FS, saved_errno,
            "staging commit: rename '%s' -> '%s' failed: %s",
            staging->path, staging->dest_path, strerror(saved_errno));
    return PH_ERR;
}

ph_result_t ph_staging_cleanup(ph_staging_t *staging, ph_error_t **err) {
    if (!staging || !staging->active || !staging->path)
        return PH_OK;  /* nothing to clean */

    ph_log_debug("staging cleanup: %s", staging->path);

    if (ph_fs_rmtree(staging->path, err) != PH_OK)
        return PH_ERR;

    staging->active = false;
    return PH_OK;
}

void ph_staging_destroy(ph_staging_t *staging) {
    if (!staging) return;
    ph_free(staging->path);
    ph_free(staging->dest_path);
    memset(staging, 0, sizeof(*staging));
}

ph_result_t ph_staging_find_stale(const char *parent_dir,
                                   char ***out_paths, size_t *out_count,
                                   ph_error_t **err) {
    if (!parent_dir || !out_paths || !out_count) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_staging_find_stale: NULL argument");
        return PH_ERR;
    }

    *out_paths = NULL;
    *out_count = 0;

    DIR *d = opendir(parent_dir);
    if (!d) {
        if (errno == ENOENT) return PH_OK;  /* dir doesn't exist, no stale */
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot open directory: %s", parent_dir);
        return PH_ERR;
    }

    size_t cap = 4;
    char **paths = ph_calloc(cap, sizeof(char *));
    if (!paths) { closedir(d); return PH_ERR; }

    size_t count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, STAGING_PREFIX,
                    strlen(STAGING_PREFIX)) != 0)
            continue;

        char *full = ph_path_join(parent_dir, ent->d_name);
        if (!full) continue;

        /* verify it is a directory */
        ph_fs_stat_t st;
        if (ph_fs_stat(full, &st) == PH_OK && st.is_dir) {
            if (count >= cap) {
                cap *= 2;
                char **tmp = ph_realloc(paths, cap * sizeof(char *));
                if (!tmp) { ph_free(full); break; }
                paths = tmp;
            }
            paths[count++] = full;
        } else {
            ph_free(full);
        }
    }
    closedir(d);

    *out_paths = paths;
    *out_count = count;
    return PH_OK;
}

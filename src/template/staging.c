#include "phosphor/template.h"
#include "phosphor/fs.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define STAGING_PREFIX ".phosphor-staging-"

ph_result_t ph_staging_create(const char *dest_path, ph_staging_t *out,
                               ph_error_t **err) {
    if (!dest_path || !out) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_staging_create: NULL argument");
        return PH_ERR;
    }

    memset(out, 0, sizeof(*out));

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

    /* build staging dir name: .phosphor-staging-<pid>-<timestamp> */
    char name[128];
    snprintf(name, sizeof(name), STAGING_PREFIX "%d-%lld",
             (int)getpid(), (long long)time(NULL));

    char *staging_path = ph_path_join(parent, name);
    ph_free(parent);

    if (!staging_path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "failed to build staging path");
        return PH_ERR;
    }

    /* create the staging directory */
    if (ph_fs_mkdir_p(staging_path, 0755) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create staging directory: %s", staging_path);
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

    /* try rename first (atomic on same filesystem) */
    if (ph_fs_rename(staging->path, staging->dest_path) == PH_OK) {
        staging->active = false;
        ph_log_info("staging committed: %s -> %s",
                    staging->path, staging->dest_path);
        return PH_OK;
    }

    /* EXDEV fallback: copytree + remove staging */
    ph_log_warn("staging rename failed (cross-device?), falling back to copy");

    if (ph_fs_copytree(staging->path, staging->dest_path,
                        NULL, NULL, err) != PH_OK)
        return PH_ERR;

    /* remove staging dir */
    ph_error_t *rm_err = NULL;
    if (ph_fs_rmtree(staging->path, &rm_err) != PH_OK) {
        ph_log_warn("failed to remove staging after copy: %s",
                    rm_err ? rm_err->message : "unknown");
        ph_error_destroy(rm_err);
        /* not a fatal error -- files are committed */
    }

    staging->active = false;
    ph_log_info("staging committed (copy fallback): %s -> %s",
                staging->path, staging->dest_path);
    return PH_OK;
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

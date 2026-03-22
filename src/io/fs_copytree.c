#include "phosphor/fs.h"
#include "phosphor/platform.h"
#include "phosphor/path.h"
#include "phosphor/signal.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static ph_result_t copytree_recurse(const char *src, const char *dst,
                                     ph_fs_filter_fn filter, void *ctx,
                                     const char *rel_base, int depth,
                                     ph_error_t **err) {
    if (depth > PH_MAX_DIR_DEPTH) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "directory depth exceeds %d: %s",
                                     PH_MAX_DIR_DEPTH, src);
        return PH_ERR;
    }

    DIR *d = opendir(src);
    if (!d) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot open directory: %s (%s)",
                                     src, strerror(errno));
        return PH_ERR;
    }

    /* ensure destination directory exists */
    if (ph_fs_mkdir_p(dst, 0755) != PH_OK) {
        closedir(d);
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot create directory: %s", dst);
        return PH_ERR;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (ph_signal_interrupted()) {
            closedir(d);
            if (err)
                *err = ph_error_createf(PH_ERR_SIGNAL, 0, "interrupted");
            return PH_ERR;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char *src_child = ph_path_join(src, entry->d_name);
        char *dst_child = ph_path_join(dst, entry->d_name);

        /* build relative path for filter */
        char *rel_child;
        if (rel_base && rel_base[0] != '\0') {
            rel_child = ph_path_join(rel_base, entry->d_name);
        } else {
            size_t nlen = strlen(entry->d_name);
            rel_child = ph_alloc(nlen + 1);
            if (rel_child) memcpy(rel_child, entry->d_name, nlen + 1);
        }

        if (!src_child || !dst_child || !rel_child) {
            ph_free(src_child);
            ph_free(dst_child);
            ph_free(rel_child);
            closedir(d);
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0, "allocation failed");
            return PH_ERR;
        }

        ph_fs_stat_t st;
        if (ph_fs_stat(src_child, &st) != PH_OK || !st.exists) {
            ph_free(src_child);
            ph_free(dst_child);
            ph_free(rel_child);
            continue;
        }

        /* apply filter */
        if (filter && !filter(rel_child, st.is_dir, ctx)) {
            ph_log_trace("copytree: skipped %s (filtered)", rel_child);
            ph_free(src_child);
            ph_free(dst_child);
            ph_free(rel_child);
            continue;
        }

        if (st.is_dir) {
            ph_result_t rc = copytree_recurse(src_child, dst_child,
                                               filter, ctx,
                                               rel_child, depth + 1, err);
            if (rc != PH_OK) {
                ph_free(src_child);
                ph_free(dst_child);
                ph_free(rel_child);
                closedir(d);
                return PH_ERR;
            }
        } else if (st.is_file) {
            uint8_t *data = NULL;
            size_t data_len = 0;

            if (ph_fs_read_file(src_child, &data, &data_len) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                                             "cannot read: %s", src_child);
                ph_free(src_child);
                ph_free(dst_child);
                ph_free(rel_child);
                closedir(d);
                return PH_ERR;
            }

            if (ph_fs_write_file(dst_child, data, data_len) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                                             "cannot write: %s", dst_child);
                ph_free(data);
                ph_free(src_child);
                ph_free(dst_child);
                ph_free(rel_child);
                closedir(d);
                return PH_ERR;
            }

            /* preserve permissions */
            ph_fs_chmod(dst_child, st.mode & 07777);
            ph_free(data);
        }

        ph_free(src_child);
        ph_free(dst_child);
        ph_free(rel_child);
    }

    closedir(d);
    return PH_OK;
}

ph_result_t ph_fs_copytree(const char *src, const char *dst,
                            ph_fs_filter_fn filter, void *filter_ctx,
                            ph_error_t **err) {
    if (!src || !dst) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_fs_copytree: NULL argument");
        return PH_ERR;
    }

    return copytree_recurse(src, dst, filter, filter_ctx, "", 0, err);
}

ph_result_t ph_fs_rmtree(const char *path, ph_error_t **err) {
    if (!path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_fs_rmtree: NULL path");
        return PH_ERR;
    }

    ph_fs_stat_t st;
    if (ph_fs_stat(path, &st) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot stat: %s", path);
        return PH_ERR;
    }
    if (!st.exists) return PH_OK;

    if (!st.is_dir) {
        if (unlink(path) != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                                         "cannot remove file: %s (%s)",
                                         path, strerror(errno));
            return PH_ERR;
        }
        return PH_OK;
    }

    DIR *d = opendir(path);
    if (!d) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot open directory: %s (%s)",
                                     path, strerror(errno));
        return PH_ERR;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (ph_signal_interrupted()) {
            closedir(d);
            if (err)
                *err = ph_error_createf(PH_ERR_SIGNAL, 0, "interrupted");
            return PH_ERR;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char *child = ph_path_join(path, entry->d_name);
        if (!child) {
            closedir(d);
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0, "allocation failed");
            return PH_ERR;
        }

        ph_result_t rc = ph_fs_rmtree(child, err);
        ph_free(child);
        if (rc != PH_OK) {
            closedir(d);
            return PH_ERR;
        }
    }

    closedir(d);

    if (rmdir(path) != 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "cannot remove directory: %s (%s)",
                                     path, strerror(errno));
        return PH_ERR;
    }

    return PH_OK;
}

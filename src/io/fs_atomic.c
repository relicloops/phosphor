#include "phosphor/fs.h"
#include "phosphor/platform.h"
#include "phosphor/path.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

ph_result_t ph_fs_atomic_write(const char *path, const uint8_t *data,
                                size_t len, ph_error_t **err) {
    if (!path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_fs_atomic_write: NULL path");
        return PH_ERR;
    }

    /* build temp path: <dir>/.<basename>.XXXXXX */
    char *dir = ph_path_dirname(path);
    char *base = ph_path_basename(path);
    if (!dir || !base) {
        ph_free(dir);
        ph_free(base);
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "path decomposition failed: %s", path);
        return PH_ERR;
    }

    size_t tmplen = strlen(dir) + 2 + strlen(base) + 7 + 1;
    char *tmp = ph_alloc(tmplen);
    if (!tmp) {
        ph_free(dir);
        ph_free(base);
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0, "allocation failed");
        return PH_ERR;
    }

    snprintf(tmp, tmplen, "%s/.%s.XXXXXX", dir, base);
    ph_free(base);

    int fd = mkstemp(tmp);
    if (fd < 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "mkstemp failed for %s: %s",
                                     tmp, strerror(errno));
        ph_free(tmp);
        ph_free(dir);
        return PH_ERR;
    }

    /* write data */
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                                         "write to temp file failed: %s",
                                         strerror(errno));
            close(fd);
            unlink(tmp);
            ph_free(tmp);
            ph_free(dir);
            return PH_ERR;
        }
        total += (size_t)n;
    }

    /* fsync the file */
    if (fsync(fd) != 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                                     "fsync temp file failed: %s",
                                     strerror(errno));
        close(fd);
        unlink(tmp);
        ph_free(tmp);
        ph_free(dir);
        return PH_ERR;
    }
    close(fd);

    /* atomic rename */
    if (rename(tmp, path) != 0) {
        if (errno == EXDEV) {
            ph_log_warn("EXDEV on rename %s -> %s; falling back to copy",
                        tmp, path);
            /* fallback: copy + remove */
            if (ph_fs_write_file(path, data, len) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                                             "EXDEV fallback write failed: %s",
                                             path);
                unlink(tmp);
                ph_free(tmp);
                ph_free(dir);
                return PH_ERR;
            }
            unlink(tmp);
        } else {
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                                         "rename %s -> %s failed: %s",
                                         tmp, path, strerror(errno));
            unlink(tmp);
            ph_free(tmp);
            ph_free(dir);
            return PH_ERR;
        }
    }

    ph_free(tmp);
    ph_free(dir);
    return PH_OK;
}

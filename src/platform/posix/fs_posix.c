#include "phosphor/platform.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>

ph_result_t ph_fs_stat(const char *path, ph_fs_stat_t *out) {
    if (!path || !out) return PH_ERR;

    struct stat sb;
    if (lstat(path, &sb) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            *out = (ph_fs_stat_t){ .exists = false };
            return PH_OK;
        }
        ph_log_error("stat %s: %s", path, strerror(errno));
        return PH_ERR;
    }

    out->size    = sb.st_size;
    out->mode    = sb.st_mode;
    out->is_dir  = S_ISDIR(sb.st_mode);
    out->is_file = S_ISREG(sb.st_mode);
    out->is_link = S_ISLNK(sb.st_mode);
    out->exists  = true;
    return PH_OK;
}

ph_result_t ph_fs_read_file(const char *path, uint8_t **out_data,
                            size_t *out_len) {
    if (!path || !out_data || !out_len) return PH_ERR;

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        ph_log_error("open %s: %s", path, strerror(errno));
        return PH_ERR;
    }

    struct stat sb;
    if (fstat(fd, &sb) != 0) {
        ph_log_error("fstat %s: %s", path, strerror(errno));
        close(fd);
        return PH_ERR;
    }

    size_t size = (size_t)sb.st_size;
    uint8_t *buf = ph_alloc(size + 1);
    if (!buf) {
        ph_log_error("read %s: allocation failed (%zu bytes)", path, size);
        close(fd);
        return PH_ERR;
    }

    size_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, buf + total, size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            ph_log_error("read %s: %s", path, strerror(errno));
            ph_free(buf);
            close(fd);
            return PH_ERR;
        }
        if (n == 0) break;
        total += (size_t)n;
    }

    close(fd);
    buf[total] = 0;
    *out_data = buf;
    *out_len  = total;
    return PH_OK;
}

ph_result_t ph_fs_write_file(const char *path, const uint8_t *data,
                             size_t len) {
    if (!path) return PH_ERR;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        ph_log_error("open %s: %s", path, strerror(errno));
        return PH_ERR;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            ph_log_error("write %s: %s", path, strerror(errno));
            close(fd);
            return PH_ERR;
        }
        total += (size_t)n;
    }

    close(fd);
    return PH_OK;
}

ph_result_t ph_fs_rename(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return PH_ERR;

    if (rename(old_path, new_path) != 0) {
        ph_log_error("rename %s -> %s: %s", old_path, new_path,
                     strerror(errno));
        return PH_ERR;
    }
    return PH_OK;
}

ph_result_t ph_fs_fsync_path(const char *path) {
    if (!path) return PH_ERR;

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        ph_log_error("fsync open %s: %s", path, strerror(errno));
        return PH_ERR;
    }

    int rc = fsync(fd);
    close(fd);
    if (rc != 0) {
        ph_log_error("fsync %s: %s", path, strerror(errno));
        return PH_ERR;
    }
    return PH_OK;
}

ph_result_t ph_fs_chmod(const char *path, mode_t mode) {
    if (!path) return PH_ERR;

    if (chmod(path, mode) != 0) {
        ph_log_error("chmod %s: %s", path, strerror(errno));
        return PH_ERR;
    }
    return PH_OK;
}

ph_result_t ph_fs_mkdir_p(const char *path, mode_t mode) {
    if (!path) return PH_ERR;

    size_t len = strlen(path);
    char *buf = ph_alloc(len + 1);
    if (!buf) return PH_ERR;
    memcpy(buf, path, len + 1);

    for (size_t i = 1; i <= len; i++) {
        if (buf[i] == '/' || buf[i] == '\0') {
            char saved = buf[i];
            buf[i] = '\0';

            if (mkdir(buf, mode) != 0 && errno != EEXIST) {
                ph_log_error("mkdir %s: %s", buf, strerror(errno));
                ph_free(buf);
                return PH_ERR;
            }
            buf[i] = saved;
        }
    }

    ph_free(buf);
    return PH_OK;
}

bool ph_fs_fnmatch(const char *pattern, const char *string) {
    if (!pattern || !string) return false;
    return fnmatch(pattern, string, FNM_PATHNAME | FNM_PERIOD) == 0;
}

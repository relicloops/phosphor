#include "phosphor/archive.h"
#include "phosphor/alloc.h"
#include "phosphor/path.h"
#include "phosphor/fs.h"
#include "phosphor/sha256.h"
#include "phosphor/platform.h"
#include "phosphor/signal.h"
#include "phosphor/log.h"

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#ifdef PHOSPHOR_HAS_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

#define EXTRACT_PREFIX ".phosphor-extract-"

/* ---- always available (no libarchive dependency) ---- */

/*
 * case-insensitive suffix match for extension detection.
 */
static bool ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t sfxlen = strlen(suffix);
    if (sfxlen > slen) return false;
    return strcasecmp(s + slen - sfxlen, suffix) == 0;
}

ph_archive_format_t ph_archive_detect(const char *path) {
    if (!path || *path == '\0') return PH_ARCHIVE_NONE;

    if (ends_with(path, ".tar.gz") || ends_with(path, ".tgz"))
        return PH_ARCHIVE_TAR_GZ;
    if (ends_with(path, ".tar.zst"))
        return PH_ARCHIVE_TAR_ZST;
    if (ends_with(path, ".zip"))
        return PH_ARCHIVE_ZIP;

    return PH_ARCHIVE_NONE;
}

ph_result_t ph_archive_cleanup_extract(const char *extract_path,
                                        ph_error_t **err) {
    if (!extract_path) return PH_OK;
    ph_log_debug("archive: cleaning up extraction directory: %s", extract_path);
    return ph_fs_rmtree(extract_path, err);
}

/* ---- libarchive-dependent extraction ---- */

#ifdef PHOSPHOR_HAS_LIBARCHIVE

/*
 * parse_checksum -- extract algorithm and hex from "sha256:<hex>" format.
 * returns pointer to the hex portion, or NULL on format error.
 */
static const char *parse_checksum(const char *checksum, ph_error_t **err) {
    if (strncmp(checksum, "sha256:", 7) != 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                "unsupported checksum format: %s (expected sha256:<hex>)",
                checksum);
        return NULL;
    }
    return checksum + 7;
}

/*
 * validate_entry_path -- reject path traversal and absolute paths.
 */
static ph_result_t validate_entry_path(const char *pathname,
                                        ph_error_t **err) {
    if (ph_path_is_absolute(pathname)) {
        if (err)
            *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                "archive entry has absolute path (zip/tar slip): %s",
                pathname);
        return PH_ERR;
    }
    if (ph_path_has_traversal(pathname)) {
        if (err)
            *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                "archive entry has path traversal (zip/tar slip): %s",
                pathname);
        return PH_ERR;
    }
    return PH_OK;
}

/*
 * extract_entry_data -- write archive entry data to disk.
 */
static ph_result_t extract_entry_data(struct archive *ar, const char *dest,
                                       ph_error_t **err) {
    FILE *fp = fopen(dest, "wb");
    if (!fp) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create file: %s", dest);
        return PH_ERR;
    }

    const void *buf;
    size_t size;
    la_int64_t offset;
    int rc;

    while ((rc = archive_read_data_block(ar, &buf, &size, &offset)) ==
           ARCHIVE_OK) {
        if (fwrite(buf, 1, size, fp) != size) {
            fclose(fp);
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                    "write error extracting: %s", dest);
            return PH_ERR;
        }
    }

    fclose(fp);

    if (rc != ARCHIVE_EOF) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "archive read error for entry: %s (%s)",
                dest, archive_error_string(ar));
        return PH_ERR;
    }

    return PH_OK;
}

ph_result_t ph_archive_extract(const char *archive_path,
                                const char *checksum,
                                char **out_path,
                                ph_error_t **err) {
    if (!archive_path || !out_path) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_archive_extract: NULL argument");
        return PH_ERR;
    }

    *out_path = NULL;

    /* step 1: optional checksum verification */
    if (checksum) {
        const char *hex = parse_checksum(checksum, err);
        if (!hex) return PH_ERR;

        ph_log_info("archive: verifying checksum for %s", archive_path);
        if (ph_sha256_verify(archive_path, hex, err) != PH_OK)
            return PH_ERR;
        ph_log_info("archive: checksum verified");
    }

    /* step 2: create temporary extraction directory */
    char extract_dir[256];
    snprintf(extract_dir, sizeof(extract_dir),
             "/tmp/" EXTRACT_PREFIX "%d-%lld",
             (int)getpid(), (long long)time(NULL));

    if (ph_fs_mkdir_p(extract_dir, 0755) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create extraction directory: %s", extract_dir);
        return PH_ERR;
    }

    if (ph_signal_interrupted()) {
        ph_archive_cleanup_extract(extract_dir, NULL);
        if (err)
            *err = ph_error_createf(PH_ERR_SIGNAL, 0,
                "interrupted before extraction");
        return PH_ERR;
    }

    /* step 3: open archive */
    struct archive *ar = archive_read_new();
    if (!ar) {
        ph_archive_cleanup_extract(extract_dir, NULL);
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "failed to allocate archive reader");
        return PH_ERR;
    }

    archive_read_support_format_tar(ar);
    archive_read_support_format_zip(ar);
    archive_read_support_filter_gzip(ar);
    archive_read_support_filter_zstd(ar);
    archive_read_support_filter_none(ar);

    int rc = archive_read_open_filename(ar, archive_path, 10240);
    if (rc != ARCHIVE_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "cannot open archive: %s (%s)",
                archive_path, archive_error_string(ar));
        archive_read_free(ar);
        ph_archive_cleanup_extract(extract_dir, NULL);
        return PH_ERR;
    }

    ph_log_info("archive: extracting %s to %s", archive_path, extract_dir);

    /* step 4: iterate entries */
    struct archive_entry *entry;
    while ((rc = archive_read_next_header(ar, &entry)) == ARCHIVE_OK) {
        if (ph_signal_interrupted()) {
            archive_read_free(ar);
            ph_archive_cleanup_extract(extract_dir, NULL);
            if (err)
                *err = ph_error_createf(PH_ERR_SIGNAL, 0,
                    "interrupted during extraction");
            return PH_ERR;
        }

        const char *pathname = archive_entry_pathname(entry);

        /* security: reject traversal and absolute paths */
        if (validate_entry_path(pathname, err) != PH_OK) {
            archive_read_free(ar);
            ph_archive_cleanup_extract(extract_dir, NULL);
            return PH_ERR;
        }

        /* build full destination path */
        char *dest = ph_path_join(extract_dir, pathname);
        if (!dest) {
            archive_read_free(ar);
            ph_archive_cleanup_extract(extract_dir, NULL);
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                    "path join failed for entry: %s", pathname);
            return PH_ERR;
        }

        int entry_type = archive_entry_filetype(entry);

        if (entry_type == AE_IFDIR) {
            /* directory entry: create it */
            if (ph_fs_mkdir_p(dest, 0755) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                        "cannot create directory: %s", dest);
                ph_free(dest);
                archive_read_free(ar);
                ph_archive_cleanup_extract(extract_dir, NULL);
                return PH_ERR;
            }
        } else if (entry_type == AE_IFREG) {
            /* regular file: create parent dirs, then write */
            char *parent = ph_path_dirname(dest);
            if (parent) {
                ph_fs_mkdir_p(parent, 0755);
                ph_free(parent);
            }

            if (extract_entry_data(ar, dest, err) != PH_OK) {
                ph_free(dest);
                archive_read_free(ar);
                ph_archive_cleanup_extract(extract_dir, NULL);
                return PH_ERR;
            }

            /* preserve executable permission */
            mode_t mode = archive_entry_perm(entry);
            if (mode & 0111)
                chmod(dest, 0755);
        }
        /* skip symlinks, special files */

        ph_free(dest);
    }

    archive_read_free(ar);

    if (rc != ARCHIVE_EOF) {
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "archive read error: %s (%s)",
                archive_path, archive_error_string(ar));
        ph_archive_cleanup_extract(extract_dir, NULL);
        return PH_ERR;
    }

    /* step 5: transfer ownership of extract path to caller */
    size_t dir_len = strlen(extract_dir);
    *out_path = ph_alloc(dir_len + 1);
    if (!*out_path) {
        ph_archive_cleanup_extract(extract_dir, NULL);
        return PH_ERR;
    }
    memcpy(*out_path, extract_dir, dir_len + 1);

    ph_log_info("archive: extraction complete: %s", extract_dir);
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBARCHIVE */

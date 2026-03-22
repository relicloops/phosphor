#include "phosphor/template.h"
#include "phosphor/render.h"
#include "phosphor/fs.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/signal.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/regex.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ---- filter context ---- */

typedef struct {
    const ph_filters_t *filters;
#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t **exclude_re;
    size_t       exclude_re_count;
#endif
} ph_exec_filter_ctx_t;

/*
 * exec_filter_cb -- copytree filter callback implementing exclude patterns.
 * returns true to include, false to skip.
 */
static bool exec_filter_cb(const char *rel_path, bool is_dir, void *ctx) {
    (void)is_dir;
    const ph_exec_filter_ctx_t *fctx = ctx;
    if (!fctx || !fctx->filters) return true;

    const ph_filters_t *f = fctx->filters;

    /* check metadata deny list (always applied) */
    const char *basename = strrchr(rel_path, '/');
    basename = basename ? basename + 1 : rel_path;
    if (ph_metadata_is_denied(basename)) {
        ph_log_trace("filter: denied metadata file: %s", rel_path);
        return false;
    }

    /* check glob exclude patterns */
    for (size_t i = 0; i < f->exclude_count; i++) {
        if (f->exclude[i] && ph_fs_fnmatch(f->exclude[i], rel_path)) {
            ph_log_trace("filter: excluded by glob '%s': %s",
                          f->exclude[i], rel_path);
            return false;
        }
    }

#ifdef PHOSPHOR_HAS_PCRE2
    /* check regex exclude patterns */
    for (size_t i = 0; i < fctx->exclude_re_count; i++) {
        if (fctx->exclude_re[i] &&
            ph_regex_match(fctx->exclude_re[i], rel_path)) {
            ph_log_trace("filter: excluded by regex: %s", rel_path);
            return false;
        }
    }
#endif

    return true;
}

/*
 * check_deny -- check a path against deny patterns.
 * returns PH_OK if path is allowed, PH_ERR if denied (sets err).
 */
static ph_result_t check_deny(const char *rel_path,
                                const ph_filters_t *filters,
#ifdef PHOSPHOR_HAS_PCRE2
                                ph_regex_t **deny_re, size_t deny_re_count,
#endif
                                ph_error_t **err) {
    if (!filters) return PH_OK;

    /* glob deny patterns */
    for (size_t i = 0; i < filters->deny_count; i++) {
        if (filters->deny[i] && ph_fs_fnmatch(filters->deny[i], rel_path)) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "file '%s' matches deny pattern '%s'",
                    rel_path, filters->deny[i]);
            return PH_ERR;
        }
    }

#ifdef PHOSPHOR_HAS_PCRE2
    /* regex deny patterns */
    for (size_t i = 0; i < deny_re_count; i++) {
        if (deny_re[i] && ph_regex_match(deny_re[i], rel_path)) {
            if (err)
                *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                    "file '%s' matches deny_regex pattern", rel_path);
            return PH_ERR;
        }
    }
#endif

    return PH_OK;
}

/* ---- regex cleanup helpers ---- */

#ifdef PHOSPHOR_HAS_PCRE2
static void free_regex_array(ph_regex_t **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++)
        ph_regex_destroy(arr[i]);
    ph_free(arr);
}
#endif

/* ---- recursive render-tree ---- */

typedef struct {
    const ph_resolved_var_t *vars;
    size_t                   var_count;
    const ph_filters_t      *filters;
    const ph_exec_filter_ctx_t *filter_ctx;
    const char              *newline;
    ph_plan_stats_t         *stats;
} rendertree_ctx_t;

static ph_result_t rendertree_recurse(const char *src, const char *dst,
                                       const rendertree_ctx_t *rctx,
                                       const char *rel_base, int depth,
                                       ph_error_t **err) {
    if (depth > PH_MAX_DIR_DEPTH) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "directory depth exceeds %d: %s", PH_MAX_DIR_DEPTH, src);
        return PH_ERR;
    }

    DIR *d = opendir(src);
    if (!d) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot open directory: %s", src);
        return PH_ERR;
    }

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
            if (err) *err = ph_error_createf(PH_ERR_FS, 0, "allocation failed");
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
        if (rctx->filter_ctx && rctx->filter_ctx->filters) {
            if (!exec_filter_cb(rel_child, st.is_dir,
                                 (void *)rctx->filter_ctx)) {
                ph_free(src_child);
                ph_free(dst_child);
                ph_free(rel_child);
                continue;
            }
        }

        if (st.is_dir) {
            ph_result_t rc = rendertree_recurse(src_child, dst_child, rctx,
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

            const char *ext = ph_path_extension(src_child);
            bool is_binary = ph_transform_is_binary(data, data_len, ext);

            /* also check manifest binary_ext */
            if (!is_binary && ext && rctx->filters) {
                for (size_t j = 0; j < rctx->filters->binary_ext_count; j++) {
                    if (rctx->filters->binary_ext[j] &&
                        strcmp(ext, rctx->filters->binary_ext[j]) == 0) {
                        is_binary = true;
                        break;
                    }
                }
            }

            uint8_t *out = data;
            size_t out_len = data_len;
            bool out_owned = false;

            if (!is_binary) {
                uint8_t *rendered = NULL;
                size_t rendered_len = 0;
                if (ph_render_template(data, data_len,
                                        rctx->vars, rctx->var_count,
                                        &rendered, &rendered_len, err) != PH_OK) {
                    ph_free(data);
                    ph_free(src_child);
                    ph_free(dst_child);
                    ph_free(rel_child);
                    closedir(d);
                    return PH_ERR;
                }
                out = rendered;
                out_len = rendered_len;
                out_owned = true;

                if (rctx->newline) {
                    uint8_t *nl_out = NULL;
                    size_t nl_len = 0;
                    if (ph_transform_newline(out, out_len, rctx->newline,
                                              &nl_out, &nl_len) == PH_OK) {
                        ph_free(out);
                        out = nl_out;
                        out_len = nl_len;
                    }
                }
            }

            if (ph_fs_write_file(dst_child, out, out_len) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                        "cannot write: %s", dst_child);
                if (out_owned) ph_free(out);
                ph_free(data);
                ph_free(src_child);
                ph_free(dst_child);
                ph_free(rel_child);
                closedir(d);
                return PH_ERR;
            }

            ph_fs_chmod(dst_child, st.mode & 07777);
            rctx->stats->bytes_written += out_len;
            rctx->stats->files_rendered++;

            if (out_owned) ph_free(out);
            ph_free(data);
        }

        ph_free(src_child);
        ph_free(dst_child);
        ph_free(rel_child);
    }

    closedir(d);
    return PH_OK;
}

/* ---- plan execution ---- */

ph_result_t ph_plan_execute(const ph_plan_t *plan,
                             const ph_resolved_var_t *vars, size_t var_count,
                             const ph_filters_t *filters,
                             ph_plan_stats_t *stats,
                             ph_error_t **err) {
    if (!plan || !stats) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_plan_execute: NULL argument");
        return PH_ERR;
    }

    memset(stats, 0, sizeof(*stats));

    /* ---- compile filter patterns ---- */

    ph_exec_filter_ctx_t filter_ctx;
    memset(&filter_ctx, 0, sizeof(filter_ctx));
    filter_ctx.filters = filters;

#ifdef PHOSPHOR_HAS_PCRE2
    ph_regex_t **exclude_re = NULL;
    ph_regex_t **deny_re = NULL;
    size_t exclude_re_count = 0;
    size_t deny_re_count = 0;

    if (filters && filters->exclude_regex_count > 0) {
        exclude_re_count = filters->exclude_regex_count;
        exclude_re = ph_calloc(exclude_re_count, sizeof(ph_regex_t *));
        if (!exclude_re) goto cleanup_err;
        for (size_t i = 0; i < exclude_re_count; i++) {
            if (filters->exclude_regex[i]) {
                if (ph_regex_compile(filters->exclude_regex[i],
                                      &exclude_re[i], err) != PH_OK)
                    goto cleanup_err;
            }
        }
        filter_ctx.exclude_re = exclude_re;
        filter_ctx.exclude_re_count = exclude_re_count;
    }

    if (filters && filters->deny_regex_count > 0) {
        deny_re_count = filters->deny_regex_count;
        deny_re = ph_calloc(deny_re_count, sizeof(ph_regex_t *));
        if (!deny_re) goto cleanup_err;
        for (size_t i = 0; i < deny_re_count; i++) {
            if (filters->deny_regex[i]) {
                if (ph_regex_compile(filters->deny_regex[i],
                                      &deny_re[i], err) != PH_OK)
                    goto cleanup_err;
            }
        }
    }
#endif

    /* ---- execute operations ---- */

    for (size_t i = 0; i < plan->count; i++) {
        if (ph_signal_interrupted()) {
            if (err)
                *err = ph_error_createf(PH_ERR_SIGNAL, 0, "interrupted");
            goto cleanup_err;
        }

        const ph_planned_op_t *op = &plan->ops[i];

        if (op->skip) {
            stats->skipped++;
            continue;
        }

        /* deny check for COPY and RENDER operations */
        if ((op->kind == PH_OP_COPY || op->kind == PH_OP_RENDER) &&
            op->from_abs && filters) {
            const char *rel = strrchr(op->from_abs, '/');
            rel = rel ? rel + 1 : op->from_abs;

            if (check_deny(rel, filters,
#ifdef PHOSPHOR_HAS_PCRE2
                            deny_re, deny_re_count,
#endif
                            err) != PH_OK)
                goto cleanup_err;
        }

        switch (op->kind) {

        case PH_OP_MKDIR: {
            if (!op->to_abs) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "mkdir op %zu: missing 'to' path", i);
                goto cleanup_err;
            }

            mode_t mode = 0755;
            if (op->mode) mode = (mode_t)strtol(op->mode, NULL, 8);

            if (ph_fs_mkdir_p(op->to_abs, mode) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                        "cannot create directory: %s", op->to_abs);
                goto cleanup_err;
            }
            stats->dirs_created++;
            ph_log_info("  mkdir %s", op->to_abs);
            break;
        }

        case PH_OP_COPY: {
            if (!op->from_abs || !op->to_abs) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "copy op %zu: missing 'from' or 'to'", i);
                goto cleanup_err;
            }

            /* check if source is a directory -- recurse */
            ph_fs_stat_t st;
            if (ph_fs_stat(op->from_abs, &st) != PH_OK || !st.exists) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                        "source does not exist: %s", op->from_abs);
                goto cleanup_err;
            }

            if (st.is_dir) {
                if (ph_fs_copytree(op->from_abs, op->to_abs,
                                    filters ? exec_filter_cb : NULL,
                                    filters ? &filter_ctx : NULL,
                                    err) != PH_OK)
                    goto cleanup_err;
                stats->files_copied++;
            } else {
                /* ensure parent dir */
                char *dir = ph_path_dirname(op->to_abs);
                if (dir) { ph_fs_mkdir_p(dir, 0755); ph_free(dir); }

                uint8_t *data = NULL;
                size_t data_len = 0;
                if (ph_fs_read_file(op->from_abs, &data, &data_len) != PH_OK) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_FS, 0,
                            "cannot read: %s", op->from_abs);
                    goto cleanup_err;
                }

                if (op->atomic) {
                    if (ph_fs_atomic_write(op->to_abs, data, data_len, err) != PH_OK) {
                        ph_free(data);
                        goto cleanup_err;
                    }
                } else {
                    if (ph_fs_write_file(op->to_abs, data, data_len) != PH_OK) {
                        if (err)
                            *err = ph_error_createf(PH_ERR_FS, 0,
                                "cannot write: %s", op->to_abs);
                        ph_free(data);
                        goto cleanup_err;
                    }
                }

                stats->bytes_written += data_len;
                stats->files_copied++;
                ph_free(data);
            }
            ph_log_info("  copy %s -> %s", op->from_abs, op->to_abs);
            break;
        }

        case PH_OP_RENDER: {
            if (!op->from_abs || !op->to_abs) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "render op %zu: missing 'from' or 'to'", i);
                goto cleanup_err;
            }

            ph_fs_stat_t st;
            if (ph_fs_stat(op->from_abs, &st) != PH_OK || !st.exists) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                        "source does not exist: %s", op->from_abs);
                goto cleanup_err;
            }

            if (st.is_dir) {
                rendertree_ctx_t rctx = {
                    .vars       = vars,
                    .var_count  = var_count,
                    .filters    = filters,
                    .filter_ctx = filters ? &filter_ctx : NULL,
                    .newline    = op->newline,
                    .stats      = stats,
                };
                if (rendertree_recurse(op->from_abs, op->to_abs,
                                        &rctx, "", 0, err) != PH_OK)
                    goto cleanup_err;
            } else {
                /* ensure parent dir */
                char *dir = ph_path_dirname(op->to_abs);
                if (dir) { ph_fs_mkdir_p(dir, 0755); ph_free(dir); }

                uint8_t *data = NULL;
                size_t data_len = 0;
                if (ph_fs_read_file(op->from_abs, &data, &data_len) != PH_OK) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_FS, 0,
                            "cannot read: %s", op->from_abs);
                    goto cleanup_err;
                }

                /* check if binary */
                const char *ext = ph_path_extension(op->from_abs);
                bool is_binary = op->is_binary ||
                                  ph_transform_is_binary(data, data_len, ext);

                uint8_t *out = data;
                size_t out_len = data_len;
                bool out_owned = false;

                if (!is_binary) {
                    /* render placeholders */
                    uint8_t *rendered = NULL;
                    size_t rendered_len = 0;
                    if (ph_render_template(data, data_len, vars, var_count,
                                            &rendered, &rendered_len, err) != PH_OK) {
                        ph_free(data);
                        goto cleanup_err;
                    }
                    out = rendered;
                    out_len = rendered_len;
                    out_owned = true;

                    /* newline normalization */
                    if (op->newline) {
                        uint8_t *nl_out = NULL;
                        size_t nl_len = 0;
                        if (ph_transform_newline(out, out_len, op->newline,
                                                  &nl_out, &nl_len) == PH_OK) {
                            ph_free(out);
                            out = nl_out;
                            out_len = nl_len;
                        }
                    }
                }

                if (op->atomic) {
                    if (ph_fs_atomic_write(op->to_abs, out, out_len, err) != PH_OK) {
                        if (out_owned) ph_free(out);
                        ph_free(data);
                        goto cleanup_err;
                    }
                } else {
                    if (ph_fs_write_file(op->to_abs, out, out_len) != PH_OK) {
                        if (err)
                            *err = ph_error_createf(PH_ERR_FS, 0,
                                "cannot write: %s", op->to_abs);
                        if (out_owned) ph_free(out);
                        ph_free(data);
                        goto cleanup_err;
                    }
                }

                stats->bytes_written += out_len;
                stats->files_rendered++;
                if (out_owned) ph_free(out);
                ph_free(data);
            }
            ph_log_info("  render %s -> %s", op->from_abs, op->to_abs);
            break;
        }

        case PH_OP_CHMOD: {
            if (!op->from_abs || !op->mode) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "chmod op %zu: missing 'from' or 'mode'", i);
                goto cleanup_err;
            }

            mode_t mode = (mode_t)strtol(op->mode, NULL, 8);
            if (ph_fs_chmod(op->from_abs, mode) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                        "chmod failed: %s", op->from_abs);
                goto cleanup_err;
            }
            ph_log_info("  chmod %s %s", op->mode, op->from_abs);
            break;
        }

        case PH_OP_REMOVE: {
            const char *target = op->to_abs ? op->to_abs : op->from_abs;
            if (!target) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "remove op %zu: missing target path", i);
                goto cleanup_err;
            }

            if (ph_fs_rmtree(target, err) != PH_OK)
                goto cleanup_err;
            ph_log_info("  remove %s", target);
            break;
        }

        } /* switch */
    }

    /* ---- success cleanup ---- */

#ifdef PHOSPHOR_HAS_PCRE2
    free_regex_array(exclude_re, exclude_re_count);
    free_regex_array(deny_re, deny_re_count);
#endif
    return PH_OK;

cleanup_err:
#ifdef PHOSPHOR_HAS_PCRE2
    free_regex_array(exclude_re, exclude_re_count);
    free_regex_array(deny_re, deny_re_count);
#endif
    return PH_ERR;
}

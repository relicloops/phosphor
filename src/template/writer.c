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
    ph_regex_t **deny_re;
    size_t       deny_re_count;
#endif
    /* audit fix: mutable deny-hit state populated by the filter callback
     * during recursive walks. ph_fs_copytree's filter API is bool-only, so
     * the callback records the first denied path on the context and returns
     * false (skip). The caller inspects deny_hit after the walk and fails
     * the operation. This is the only way to enforce manifest deny rules on
     * descendants of a directory copy/render. */
    bool  deny_hit;
    char *deny_path;    /* strdup'd on first hit; owned by ctx */
    char *deny_pattern; /* strdup'd on first hit; owned by ctx */
} ph_exec_filter_ctx_t;

/* duplicate a C string into ph_alloc memory; NULL-safe */
static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *r = ph_alloc(n + 1);
    if (r) memcpy(r, s, n + 1);
    return r;
}

/* reset mutable deny-hit state on the context between ops */
static void filter_ctx_reset_deny(ph_exec_filter_ctx_t *fctx) {
    if (!fctx) return;
    ph_free(fctx->deny_path);
    ph_free(fctx->deny_pattern);
    fctx->deny_path = NULL;
    fctx->deny_pattern = NULL;
    fctx->deny_hit = false;
}

/*
 * exec_filter_cb -- copytree filter callback implementing exclude and deny
 * patterns. returns true to include, false to skip.
 *
 * a deny hit is recorded on the context (fctx->deny_hit + deny_path +
 * deny_pattern) and the entry is skipped. the caller MUST check deny_hit
 * after the walk and fail the operation.
 */
static bool exec_filter_cb(const char *rel_path, bool is_dir, void *ctx) {
    (void)is_dir;
    ph_exec_filter_ctx_t *fctx = ctx;
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

    /* audit fix: recursive deny enforcement. previously deny/deny_regex were
     * only checked once against the basename of op->from_abs, so nested
     * denied files inside a directory copy/render were still propagated. */
    for (size_t i = 0; i < f->deny_count; i++) {
        if (f->deny[i] && ph_fs_fnmatch(f->deny[i], rel_path)) {
            if (!fctx->deny_hit) {
                fctx->deny_path = dup_cstr(rel_path);
                fctx->deny_pattern = dup_cstr(f->deny[i]);
                fctx->deny_hit = true;
            }
            return false;
        }
    }

#ifdef PHOSPHOR_HAS_PCRE2
    for (size_t i = 0; i < fctx->deny_re_count; i++) {
        if (fctx->deny_re[i] &&
            ph_regex_match(fctx->deny_re[i], rel_path)) {
            if (!fctx->deny_hit) {
                fctx->deny_path = dup_cstr(rel_path);
                fctx->deny_pattern = dup_cstr("<deny_regex>");
                fctx->deny_hit = true;
            }
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

/*
 * single_file_apply_filters -- audit fix (2026-04-07T20-37-55Z).
 *
 * Apply the full filter model (metadata, exclude, exclude_regex, deny,
 * deny_regex) to a non-directory copy/render op so single-file ops behave
 * the same as descendants of a directory walk.
 *
 * Verdicts:
 *   PH_OK + *out_skip = false  -> proceed with op
 *   PH_OK + *out_skip = true   -> silently skip (metadata or exclude match)
 *   PH_ERR                     -> hard error (deny match), err is set
 *
 * The relative path is taken from op->from_rel (recorded by ph_plan_build
 * after <<var>> rendering). If from_rel is missing for any reason, fall
 * back to the basename of from_abs so the behavior is at least no worse
 * than the previous basename-only check.
 */
static ph_result_t single_file_apply_filters(
        const ph_planned_op_t *op,
        const ph_filters_t *filters,
#ifdef PHOSPHOR_HAS_PCRE2
        ph_regex_t **exclude_re, size_t exclude_re_count,
        ph_regex_t **deny_re, size_t deny_re_count,
#endif
        bool *out_skip,
        ph_error_t **err) {
    if (out_skip) *out_skip = false;
    if (!filters || !op || !op->from_abs) return PH_OK;

    const char *rel;
    if (op->from_rel && op->from_rel[0] != '\0') {
        rel = op->from_rel;
    } else {
        const char *slash = strrchr(op->from_abs, '/');
        rel = slash ? slash + 1 : op->from_abs;
    }

    const char *bn = strrchr(rel, '/');
    bn = bn ? bn + 1 : rel;

    /* metadata deny: silent skip */
    if (ph_metadata_is_denied(bn)) {
        ph_log_trace("filter: denied metadata file: %s", rel);
        if (out_skip) *out_skip = true;
        return PH_OK;
    }

    /* glob exclude: silent skip */
    for (size_t i = 0; i < filters->exclude_count; i++) {
        if (filters->exclude[i] && ph_fs_fnmatch(filters->exclude[i], rel)) {
            ph_log_trace("filter: excluded by glob '%s': %s",
                          filters->exclude[i], rel);
            if (out_skip) *out_skip = true;
            return PH_OK;
        }
    }

#ifdef PHOSPHOR_HAS_PCRE2
    /* regex exclude: silent skip */
    for (size_t i = 0; i < exclude_re_count; i++) {
        if (exclude_re[i] && ph_regex_match(exclude_re[i], rel)) {
            ph_log_trace("filter: excluded by regex: %s", rel);
            if (out_skip) *out_skip = true;
            return PH_OK;
        }
    }
#endif

    /* deny (glob + regex): hard error against the FULL relative path */
    return check_deny(rel, filters,
#ifdef PHOSPHOR_HAS_PCRE2
                       deny_re, deny_re_count,
#endif
                       err);
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
    ph_exec_filter_ctx_t    *filter_ctx;  /* audit fix: non-const so the
                                             filter callback can record a
                                             deny hit on the context */
    const char              *newline;
    const char              *dest_dir;    /* audit fix (finding 4):
                                             per-child containment anchor,
                                             may be NULL */
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
                                 rctx->filter_ctx)) {
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

            /* audit fix (finding 4): per-child containment re-check */
            if (rctx->dest_dir &&
                !ph_path_is_under(dst_child, rctx->dest_dir)) {
                if (err)
                    *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                        "rendertree: child escapes dest_dir: %s",
                        dst_child);
                if (out_owned) ph_free(out);
                ph_free(data);
                ph_free(src_child);
                ph_free(dst_child);
                ph_free(rel_child);
                closedir(d);
                return PH_ERR;
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
        /* audit fix: the recursive filter callback enforces deny_regex on
         * every descendant; wire the compiled regexes into the ctx. */
        filter_ctx.deny_re = deny_re;
        filter_ctx.deny_re_count = deny_re_count;
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

        /* audit fix (2026-04-07T20-37-55Z): unified filter enforcement for
         * COPY and RENDER. Previously a single basename-only deny check ran
         * here, leaving single-file ops free of metadata, exclude, and
         * full-path deny enforcement. Directory ops still rely on their
         * recursive walker, so we only apply the silent metadata/exclude
         * skip path when the source is not a directory; the deny check is
         * applied uniformly to both cases against the full relative path. */
        if ((op->kind == PH_OP_COPY || op->kind == PH_OP_RENDER) &&
            op->from_abs && filters) {
            ph_fs_stat_t pre_st;
            bool pre_is_dir = false;
            if (ph_fs_stat(op->from_abs, &pre_st) == PH_OK && pre_st.exists)
                pre_is_dir = pre_st.is_dir;

            if (!pre_is_dir) {
                bool sf_skip = false;
                if (single_file_apply_filters(op, filters,
#ifdef PHOSPHOR_HAS_PCRE2
                                               exclude_re, exclude_re_count,
                                               deny_re, deny_re_count,
#endif
                                               &sf_skip, err) != PH_OK)
                    goto cleanup_err;
                if (sf_skip) {
                    ph_log_debug("op %zu: skipped (filter)", i);
                    stats->skipped++;
                    continue;
                }
            } else {
                /* directory op: still enforce top-level deny on the full
                 * manifest-relative path so a manifest cannot bypass deny
                 * by writing `from = "denied/dir"`. nested entries are
                 * checked by exec_filter_cb during the walk. */
                const char *rel;
                if (op->from_rel && op->from_rel[0] != '\0') {
                    rel = op->from_rel;
                } else {
                    const char *slash = strrchr(op->from_abs, '/');
                    rel = slash ? slash + 1 : op->from_abs;
                }
                if (check_deny(rel, filters,
#ifdef PHOSPHOR_HAS_PCRE2
                                deny_re, deny_re_count,
#endif
                                err) != PH_OK)
                    goto cleanup_err;
            }
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
                /* audit fix (finding 1): validate the top-level
                 * destination before entering the recursive walk,
                 * mirroring the single-file branches (lines 663-669).
                 * Without this the copytree walker only checks
                 * descendants, not the root directory itself. */
                if (plan->dest_dir &&
                    !ph_path_is_under(op->to_abs, plan->dest_dir)) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "copy op %zu: target escapes dest_dir: %s",
                            i, op->to_abs);
                    goto cleanup_err;
                }
                /* audit fix: reset deny-hit state before the walk and
                 * check after. a hit inside the recursive walker is a hard
                 * error, even though the walker only saw a skip. */
                filter_ctx_reset_deny(&filter_ctx);
                if (ph_fs_copytree(op->from_abs, op->to_abs,
                                    filters ? exec_filter_cb : NULL,
                                    filters ? &filter_ctx : NULL,
                                    plan->dest_dir, err) != PH_OK)
                    goto cleanup_err;
                if (filter_ctx.deny_hit) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "copy op %zu: nested file '%s' matches deny "
                            "pattern '%s'", i,
                            filter_ctx.deny_path ? filter_ctx.deny_path : "?",
                            filter_ctx.deny_pattern ? filter_ctx.deny_pattern : "?");
                    goto cleanup_err;
                }
                stats->files_copied++;
            } else {
                /* audit fix (2026-04-08T17-06-51Z, finding 4): execute-time
                 * containment re-check mirroring chmod/remove. A symlink
                 * swapped into the staging tree between plan build and
                 * execute can redirect this single-file write outside the
                 * destination tree, because ph_fs_write_file / open() with
                 * O_CREAT follow symlinks. ph_path_is_under canonicalizes
                 * both sides so a swapped symlink is caught. */
                if (plan->dest_dir &&
                    !ph_path_is_under(op->to_abs, plan->dest_dir)) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "copy op %zu: target escapes dest_dir: %s",
                            i, op->to_abs);
                    goto cleanup_err;
                }

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

                /* audit fix (finding 6): preserve source mode.
                 * recursive paths do this in copytree; single-file
                 * writes (especially atomic via mkstemp) would
                 * otherwise inherit 0600 or 0644. */
                if (ph_fs_chmod(op->to_abs, st.mode & 07777) != PH_OK) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_FS, 0,
                            "copy op %zu: cannot chmod: %s", i, op->to_abs);
                    goto cleanup_err;
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
                /* audit fix (finding 1): top-level destination check
                 * for the render directory branch, mirroring the
                 * single-file check and the copy directory branch. */
                if (plan->dest_dir &&
                    !ph_path_is_under(op->to_abs, plan->dest_dir)) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "render op %zu: target escapes dest_dir: %s",
                            i, op->to_abs);
                    goto cleanup_err;
                }
                /* audit fix: reset deny-hit state before the walk and
                 * check after. see copy op above for rationale. */
                filter_ctx_reset_deny(&filter_ctx);
                rendertree_ctx_t rctx = {
                    .vars       = vars,
                    .var_count  = var_count,
                    .filters    = filters,
                    .filter_ctx = filters ? &filter_ctx : NULL,
                    .newline    = op->newline,
                    .dest_dir   = plan->dest_dir,
                    .stats      = stats,
                };
                if (rendertree_recurse(op->from_abs, op->to_abs,
                                        &rctx, "", 0, err) != PH_OK)
                    goto cleanup_err;
                if (filter_ctx.deny_hit) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "render op %zu: nested file '%s' matches deny "
                            "pattern '%s'", i,
                            filter_ctx.deny_path ? filter_ctx.deny_path : "?",
                            filter_ctx.deny_pattern ? filter_ctx.deny_pattern : "?");
                    goto cleanup_err;
                }
            } else {
                /* audit fix (2026-04-08T17-06-51Z, finding 4): execute-time
                 * containment re-check mirroring chmod/remove. A symlink
                 * swapped in between plan build and execute can redirect
                 * this single-file render outside the destination tree.
                 * See the matching block in the copy branch above. */
                if (plan->dest_dir &&
                    !ph_path_is_under(op->to_abs, plan->dest_dir)) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                            "render op %zu: target escapes dest_dir: %s",
                            i, op->to_abs);
                    goto cleanup_err;
                }

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

                /* audit fix (finding 6): preserve source mode */
                if (ph_fs_chmod(op->to_abs, st.mode & 07777) != PH_OK) {
                    if (err)
                        *err = ph_error_createf(PH_ERR_FS, 0,
                            "render op %zu: cannot chmod: %s", i, op->to_abs);
                    if (out_owned) ph_free(out);
                    ph_free(data);
                    goto cleanup_err;
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
            /* audit fix (2026-04-08): chmod must target the generated
             * output tree, not the template source. The planner sets
             * op->to_abs for ops that have a 'to' field, so prefer it
             * and fall back to from_abs only for backward compat with
             * manifests that set 'from' and no 'to'. Mirrors the
             * REMOVE op dispatch below. */
            const char *target = op->to_abs ? op->to_abs : op->from_abs;
            if (!target || !op->mode) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "chmod op %zu: missing target or 'mode'", i);
                goto cleanup_err;
            }

            /* containment: the resolved target must stay under dest_dir
             * so a manifest cannot chmod arbitrary files via a 'from'
             * value that escapes the template tree. */
            if (plan->dest_dir &&
                !ph_path_is_under(target, plan->dest_dir)) {
                if (err)
                    *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                        "chmod op %zu: target escapes dest_dir: %s",
                        i, target);
                goto cleanup_err;
            }

            mode_t mode = (mode_t)strtol(op->mode, NULL, 8);
            if (ph_fs_chmod(target, mode) != PH_OK) {
                if (err)
                    *err = ph_error_createf(PH_ERR_FS, 0,
                        "chmod failed: %s", target);
                goto cleanup_err;
            }
            ph_log_info("  chmod %s %s", op->mode, target);
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

            /* audit fix: belt-and-suspenders containment re-check.
             * must stay under dest_dir so we never rmtree outside the
             * destination tree, even if a symlink was swapped between
             * plan build and execute. */
            if (plan->dest_dir &&
                !ph_path_is_under(target, plan->dest_dir)) {
                if (err)
                    *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                        "remove op %zu: target escapes dest_dir: %s",
                        i, target);
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
    filter_ctx_reset_deny(&filter_ctx);
    return PH_OK;

cleanup_err:
#ifdef PHOSPHOR_HAS_PCRE2
    free_regex_array(exclude_re, exclude_re_count);
    free_regex_array(deny_re, deny_re_count);
#endif
    filter_ctx_reset_deny(&filter_ctx);
    return PH_ERR;
}

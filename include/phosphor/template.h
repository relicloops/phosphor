#ifndef PHOSPHOR_TEMPLATE_H
#define PHOSPHOR_TEMPLATE_H

#include "phosphor/types.h"
#include "phosphor/error.h"
#include "phosphor/manifest.h"
#include "phosphor/config.h"
#include "phosphor/args.h"

/* ---- resolved variable ---- */

/*
 * ph_resolved_var_t -- variable with its final value after merge.
 *
 * ownership: name and value heap-allocated, freed by ph_resolved_vars_destroy.
 */
typedef struct {
    char          *name;
    char          *value;
    ph_var_type_t  type;
} ph_resolved_var_t;

/* ---- variable merge context ---- */

typedef struct {
    const ph_manifest_t    *manifest;
    const ph_parsed_args_t *args;
    const ph_cli_config_t  *cli_config;
    const ph_config_t      *config;
} ph_var_merge_ctx_t;

/*
 * ph_var_merge -- resolve all variables via 4-level precedence:
 *   CLI flags > env vars > project config > manifest defaults.
 * validates constraints (regex, enum, int bounds, URL, path safety).
 * on error: exit code 6 (validation failure).
 */
ph_result_t ph_var_merge(const ph_var_merge_ctx_t *ctx,
                          ph_resolved_var_t **out_vars,
                          size_t *out_count,
                          ph_error_t **err);

void ph_resolved_vars_destroy(ph_resolved_var_t *vars, size_t count);

/*
 * ph_resolved_var_get -- look up a resolved variable by name.
 * returns NULL if not found.
 */
const char *ph_resolved_var_get(const ph_resolved_var_t *vars, size_t count,
                                 const char *name);

/* ---- planned operation ---- */

typedef struct {
    ph_op_kind_t  kind;
    char         *from_abs;     /* absolute source path (NULL for mkdir) */
    char         *to_abs;       /* absolute destination path */
    char         *mode;         /* octal mode string or NULL */
    bool          overwrite;
    bool          is_binary;
    bool          skip;         /* condition evaluated to false */
    bool          atomic;
    char         *newline;
} ph_planned_op_t;

/* ---- plan ---- */

typedef struct {
    ph_planned_op_t *ops;
    size_t           count;
    size_t           cap;
} ph_plan_t;

/* ---- plan statistics ---- */

typedef struct {
    size_t files_copied;
    size_t files_rendered;
    size_t dirs_created;
    size_t bytes_written;
    size_t skipped;
} ph_plan_stats_t;

/*
 * ph_plan_build -- resolve ops from manifest into absolute-path plan.
 * evaluates conditions, detects conflicts.
 */
ph_result_t ph_plan_build(const ph_manifest_t *manifest,
                           const ph_resolved_var_t *vars, size_t var_count,
                           const char *template_root,
                           const char *dest_dir,
                           ph_plan_t *out,
                           ph_error_t **err);

/*
 * ph_plan_execute -- run the plan into dest_dir. tracks stats.
 * checks ph_signal_interrupted() between operations.
 */
ph_result_t ph_plan_execute(const ph_plan_t *plan,
                             const ph_resolved_var_t *vars, size_t var_count,
                             const ph_filters_t *filters,
                             ph_plan_stats_t *stats,
                             ph_error_t **err);

void ph_plan_destroy(ph_plan_t *plan);

/* ---- staging directory ---- */

typedef struct {
    char *path;         /* staging directory absolute path */
    char *dest_path;    /* final destination absolute path */
    bool  active;       /* staging created and not committed */
} ph_staging_t;

/*
 * ph_staging_create -- create a staging directory named
 * .phosphor-staging-<pid>-<timestamp> in the parent of dest_path.
 */
ph_result_t ph_staging_create(const char *dest_path, ph_staging_t *out,
                               ph_error_t **err);

/*
 * ph_staging_commit -- rename staging to dest_path.
 * EXDEV fallback: copytree + remove staging, logs at WARN.
 */
ph_result_t ph_staging_commit(ph_staging_t *staging, ph_error_t **err);

/*
 * ph_staging_cleanup -- remove staging directory on failure/signal.
 */
ph_result_t ph_staging_cleanup(ph_staging_t *staging, ph_error_t **err);

void ph_staging_destroy(ph_staging_t *staging);

/*
 * ph_staging_find_stale -- find stale staging dirs in parent.
 * returns heap-allocated array of paths (caller frees each + array).
 */
ph_result_t ph_staging_find_stale(const char *parent_dir,
                                   char ***out_paths, size_t *out_count,
                                   ph_error_t **err);

#endif /* PHOSPHOR_TEMPLATE_H */

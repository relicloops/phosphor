#include "phosphor/alloc.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/config.h"
#include "phosphor/embedded.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/manifest.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/signal.h"
#include "phosphor/template.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* mkdtemp is POSIX but not exposed under strict c17 + _POSIX_C_SOURCE=200809L
 * on all platforms; provide an explicit declaration */
extern char *mkdtemp(char *tmpl);

/* ---- dry-run plan printer ---- */

static void glow_print_plan(const ph_plan_t *plan) {
  ph_log_info("dry-run: %zu operations planned", plan->count);
  for (size_t i = 0; i < plan->count; i++) {
    const ph_planned_op_t *op = &plan->ops[i];
    if (op->skip) {
      ph_log_info("  [skip] op %zu", i);
      continue;
    }
    const char *kind = "unknown";
    switch (op->kind) {
    case PH_OP_MKDIR:
      kind = "mkdir";
      break;
    case PH_OP_COPY:
      kind = "copy";
      break;
    case PH_OP_RENDER:
      kind = "render";
      break;
    case PH_OP_CHMOD:
      kind = "chmod";
      break;
    case PH_OP_REMOVE:
      kind = "remove";
      break;
    }
    if (op->from_abs && op->to_abs)
      ph_log_info("  [%s] %s -> %s", kind, op->from_abs, op->to_abs);
    else if (op->to_abs)
      ph_log_info("  [%s] %s", kind, op->to_abs);
    else if (op->from_abs)
      ph_log_info("  [%s] %s", kind, op->from_abs);
  }
}

/* ---- preflight checks ---- */

static ph_result_t glow_preflight(const ph_plan_t *plan, bool force,
                                  ph_error_t **err) {
  for (size_t i = 0; i < plan->count; i++) {
    const ph_planned_op_t *op = &plan->ops[i];
    if (op->skip)
      continue;
    const char *dest = op->to_abs;
    if (!dest)
      continue;
    ph_fs_stat_t st;
    if (ph_fs_stat(dest, &st) == PH_OK && st.exists) {
      if (!force && !op->overwrite) {
        if (err)
          *err = ph_error_createf(
              PH_ERR_FS, 0,
              "destination already exists: %s (use --force to overwrite)",
              dest);
        return PH_ERR;
      }
    }
  }
  return PH_OK;
}

/* ---- print summary ---- */

static void glow_print_summary(const ph_plan_stats_t *stats) {
  ph_log_info("glow complete:");
  if (stats->dirs_created > 0)
    ph_log_info("  directories created: %zu", stats->dirs_created);
  if (stats->files_copied > 0)
    ph_log_info("  files copied: %zu", stats->files_copied);
  if (stats->files_rendered > 0)
    ph_log_info("  files rendered: %zu", stats->files_rendered);
  if (stats->skipped > 0)
    ph_log_info("  operations skipped: %zu", stats->skipped);
  if (stats->bytes_written > 0)
    ph_log_info("  bytes written: %zu", stats->bytes_written);
}

/* ---- main glow pipeline ---- */

int ph_cmd_glow(const ph_cli_config_t *config, const ph_parsed_args_t *args) {
  (void)config;

  ph_error_t *err = NULL;
  int exit_code = 0;

  /* step 1: extract flags */
  const char *name_val = ph_args_get_flag(args, "name");
  const char *output_val = ph_args_get_flag(args, "output");
  bool dry_run = ph_args_has_flag(args, "dry-run");
  bool force = ph_args_has_flag(args, "force");
  bool verbose = ph_args_has_flag(args, "verbose");
  if (verbose)
    ph_log_set_level(PH_LOG_DEBUG);

  if (!name_val) {
    ph_log_error("--name is required");
    return PH_ERR_USAGE;
  }

  char cwd[PATH_MAX];
  if (!getcwd(cwd, sizeof(cwd)))
    return PH_ERR_INTERNAL;

  ph_log_debug("glow: --name=%s", name_val);

  /* step 2: write embedded template to temp directory */
  char tmpdir[] = "/tmp/phosphor-glow-XXXXXX";
  if (!mkdtemp(tmpdir)) {
    ph_log_error("failed to create temp directory");
    return PH_ERR_FS;
  }

  ph_log_debug("glow: writing embedded template to %s", tmpdir);
  if (ph_embedded_write_to_dir(tmpdir) != PH_OK) {
    ph_log_error("failed to write embedded template");
    ph_fs_rmtree(tmpdir, NULL);
    return PH_ERR_FS;
  }

  /* step 3: load manifest from embedded template */
  char *manifest_path = ph_manifest_find(tmpdir);
  if (!manifest_path) {
    ph_log_error("glow: no manifest in embedded template");
    ph_fs_rmtree(tmpdir, NULL);
    return PH_ERR_CONFIG;
  }

  ph_manifest_t manifest;
  memset(&manifest, 0, sizeof(manifest));
  if (ph_manifest_load(manifest_path, &manifest, &err) != PH_OK) {
    ph_log_error("manifest error: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_CONFIG;
    ph_error_destroy(err);
    ph_free(manifest_path);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }
  ph_free(manifest_path);
  ph_log_debug("glow: manifest loaded (%zu variables, %zu ops)",
               manifest.variable_count, manifest.op_count);

  if (ph_signal_interrupted()) {
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return PH_ERR_SIGNAL;
  }

  /* step 4: discover project config */
  ph_config_t project_config;
  memset(&project_config, 0, sizeof(project_config));
  if (ph_config_discover(cwd, &project_config, &err) != PH_OK) {
    ph_log_warn("config discovery: %s", err ? err->message : "unknown");
    ph_error_destroy(err);
    err = NULL;
  }

  /* step 5: merge variables */
  ph_var_merge_ctx_t merge_ctx = {
      .manifest = &manifest,
      .args = args,
      .cli_config = config,
      .config = &project_config,
  };

  ph_resolved_var_t *vars = NULL;
  size_t var_count = 0;
  if (ph_var_merge(&merge_ctx, &vars, &var_count, &err) != PH_OK) {
    ph_log_error("variable merge: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_VALIDATE;
    ph_error_destroy(err);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }

  ph_log_debug("glow: variable merge complete (%zu variables)", var_count);
  for (size_t i = 0; i < var_count; i++)
    ph_log_debug("glow:   %s = %s", vars[i].name, vars[i].value);

  if (ph_signal_interrupted()) {
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return PH_ERR_SIGNAL;
  }

  /* determine destination directory */
  char *dest_dir = NULL;
  if (output_val) {
    dest_dir = ph_path_join(cwd, output_val);
  } else {
    dest_dir = ph_path_join(cwd, name_val);
  }
  if (!dest_dir) {
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return PH_ERR_INTERNAL;
  }

  /* resolve template root: tmpdir + source_root */
  char *template_root = NULL;
  if (manifest.tmpl.source_root) {
    template_root = ph_path_join(tmpdir, manifest.tmpl.source_root);
  } else {
    size_t len = strlen(tmpdir);
    template_root = ph_alloc(len + 1);
    if (template_root)
      memcpy(template_root, tmpdir, len + 1);
  }
  if (!template_root) {
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return PH_ERR_INTERNAL;
  }

  /* step 6: build plan */
  ph_plan_t plan;
  memset(&plan, 0, sizeof(plan));
  if (ph_plan_build(&manifest, vars, var_count, template_root, dest_dir, &plan,
                    &err) != PH_OK) {
    ph_log_error("plan build: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_INTERNAL;
    ph_error_destroy(err);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }

  ph_log_debug("glow: plan built (%zu operations)", plan.count);

  /* step 7: preflight checks */
  if (glow_preflight(&plan, force, &err) != PH_OK) {
    ph_log_error("preflight: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_FS;
    ph_error_destroy(err);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }

  /* step 8: dry-run */
  if (dry_run) {
    glow_print_plan(&plan);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return 0;
  }

  /* step 9: staging -> execute -> commit */
  ph_staging_t staging;
  memset(&staging, 0, sizeof(staging));
  if (ph_staging_create(dest_dir, force, &staging, &err) != PH_OK) {
    ph_log_error("staging: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_FS;
    ph_error_destroy(err);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }

  /* re-build plan targeting staging dir */
  ph_plan_destroy(&plan);
  memset(&plan, 0, sizeof(plan));
  if (ph_plan_build(&manifest, vars, var_count, template_root, staging.path,
                    &plan, &err) != PH_OK) {
    ph_log_error("plan rebuild for staging: %s",
                 err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_INTERNAL;
    ph_error_destroy(err);
    ph_staging_cleanup(&staging, NULL);
    ph_staging_destroy(&staging);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }

  /* execute plan into staging */
  ph_log_debug("glow: executing plan into staging at %s", staging.path);
  ph_plan_stats_t stats;
  memset(&stats, 0, sizeof(stats));
  if (ph_plan_execute(&plan, vars, var_count, &manifest.filters, &stats,
                      &err) != PH_OK) {
    ph_log_error("execution: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_FS;
    ph_error_destroy(err);
    ph_staging_cleanup(&staging, NULL);
    ph_staging_destroy(&staging);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }

  if (ph_signal_interrupted()) {
    ph_staging_cleanup(&staging, NULL);
    ph_staging_destroy(&staging);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return PH_ERR_SIGNAL;
  }

  /* commit staging to destination */
  ph_log_debug("glow: committing staging to %s", dest_dir);
  if (ph_staging_commit(&staging, &err) != PH_OK) {
    ph_log_error("staging commit: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_FS;
    ph_error_destroy(err);
    ph_staging_cleanup(&staging, NULL);
    ph_staging_destroy(&staging);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_fs_rmtree(tmpdir, NULL);
    return exit_code;
  }

  /* step 10: summary */
  glow_print_summary(&stats);
  ph_log_info("project created at: %s", dest_dir);

  /* cleanup */
  ph_staging_destroy(&staging);
  ph_plan_destroy(&plan);
  ph_free(template_root);
  ph_free(dest_dir);
  ph_resolved_vars_destroy(vars, var_count);
  ph_config_destroy(&project_config);
  ph_manifest_destroy(&manifest);
  ph_fs_rmtree(tmpdir, NULL);

  return 0;
}

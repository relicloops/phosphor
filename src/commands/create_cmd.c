#include "phosphor/alloc.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/config.h"
#include "phosphor/fs.h"
#include "phosphor/archive.h"
#include "phosphor/git_fetch.h"
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

/* ---- dry-run plan printer ---- */

static void print_plan(const ph_plan_t *plan) {
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

static ph_result_t preflight_check(const ph_plan_t *plan, bool force,
                                   ph_error_t **err) {
  for (size_t i = 0; i < plan->count; i++) {
    const ph_planned_op_t *op = &plan->ops[i];
    if (op->skip)
      continue;

    /* collision check: destination already exists? */
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

static void print_summary(const ph_plan_stats_t *stats) {
  ph_log_info("create complete:");
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

/* ---- main create pipeline ---- */

int ph_cmd_create(const ph_cli_config_t *config, const ph_parsed_args_t *args) {
  (void)config;

  ph_error_t *err = NULL;
  int exit_code = 0;

  /* step 1: extract flags */
  const char *name_val = ph_args_get_flag(args, "name");
  const char *tmpl_val = ph_args_get_flag(args, "template");
  const char *output_val = ph_args_get_flag(args, "output");
  const char *checksum_val = ph_args_get_flag(args, "checksum");
  bool dry_run = ph_args_has_flag(args, "dry-run");
  bool force = ph_args_has_flag(args, "force");
  bool verbose = ph_args_has_flag(args, "verbose");
  if (verbose)
    ph_log_set_level(PH_LOG_DEBUG);

  /* reserved flags -- warn and ignore, matching build --normalize-eol style */
  if (ph_args_has_flag(args, "toml"))
    ph_log_warn("create: --toml is reserved for future use; ignored");
  if (ph_args_has_flag(args, "allow-hooks"))
    ph_log_warn("create: --allow-hooks is reserved for future use; ignored");
  if (ph_args_has_flag(args, "yes"))
    ph_log_warn("create: --yes is reserved for future use; ignored");
  if (ph_args_has_flag(args, "normalize-eol"))
    ph_log_warn("create: --normalize-eol is reserved for future use; ignored");
  if (ph_args_has_flag(args, "allow-hidden"))
    ph_log_warn("create: --allow-hidden is reserved for future use; ignored");

  if (!name_val) {
    ph_log_error("--name is required");
    return PH_ERR_USAGE;
  }

  if (!tmpl_val) {
    ph_log_error("--template is required");
    return PH_ERR_USAGE;
  }

  /* get cwd early -- needed for path resolution */
  char cwd[PATH_MAX];
  if (!getcwd(cwd, sizeof(cwd))) {
    return PH_ERR_INTERNAL;
  }

  ph_log_debug("create: --name=%s --template=%s", name_val, tmpl_val);

  /* step 2: resolve template source */
  char *template_abs = NULL;
  char *clone_dir = NULL;   /* non-NULL when template was cloned from git */
  char *extract_dir = NULL; /* non-NULL when template was extracted from archive */

  if (ph_git_is_url(tmpl_val)) {
#ifdef PHOSPHOR_HAS_LIBGIT2
    ph_git_url_t git_url;
    if (ph_git_url_parse(tmpl_val, &git_url, &err) != PH_OK) {
      ph_log_error("template URL: %s", err ? err->message : "unknown");
      exit_code = err ? (int)err->category : PH_ERR_CONFIG;
      ph_error_destroy(err);
      return exit_code;
    }

    if (ph_git_fetch_template(&git_url, &template_abs, &err) != PH_OK) {
      ph_log_error("git fetch: %s", err ? err->message : "unknown");
      exit_code = err ? (int)err->category : PH_ERR_PROCESS;
      ph_error_destroy(err);
      ph_git_url_destroy(&git_url);
      return exit_code;
    }
    ph_git_url_destroy(&git_url);

    /* keep a copy for cleanup at end */
    size_t clen = strlen(template_abs);
    clone_dir = ph_alloc(clen + 1);
    if (clone_dir)
      memcpy(clone_dir, template_abs, clen + 1);
#else
    ph_log_error("remote git templates are not available in this build");
    return PH_ERR_USAGE;
#endif /* PHOSPHOR_HAS_LIBGIT2 */
  } else if (ph_archive_detect(tmpl_val) != PH_ARCHIVE_NONE) {
#ifdef PHOSPHOR_HAS_LIBARCHIVE
    /* resolve archive path to absolute */
    char *archive_abs = NULL;
    if (ph_path_is_absolute(tmpl_val)) {
      archive_abs = ph_path_normalize(tmpl_val);
    } else {
      char *joined = ph_path_join(cwd, tmpl_val);
      if (joined) {
        archive_abs = ph_path_normalize(joined);
        ph_free(joined);
      }
    }

    if (!archive_abs) {
      ph_log_error("cannot resolve archive path: %s", tmpl_val);
      return PH_ERR_FS;
    }

    /* verify archive file exists */
    ph_fs_stat_t ar_st;
    if (ph_fs_stat(archive_abs, &ar_st) != PH_OK || !ar_st.is_file) {
      ph_log_error("archive not found or not a file: %s", archive_abs);
      ph_free(archive_abs);
      return PH_ERR_FS;
    }

    if (ph_archive_extract(archive_abs, checksum_val, &template_abs,
                            &err) != PH_OK) {
      ph_log_error("archive extract: %s", err ? err->message : "unknown");
      exit_code = err ? (int)err->category : PH_ERR_CONFIG;
      ph_error_destroy(err);
      ph_free(archive_abs);
      return exit_code;
    }
    ph_free(archive_abs);

    /* keep a copy for cleanup at end */
    size_t elen = strlen(template_abs);
    extract_dir = ph_alloc(elen + 1);
    if (extract_dir)
      memcpy(extract_dir, template_abs, elen + 1);
#else
    ph_log_error("archive templates are not available in this build");
    return PH_ERR_USAGE;
#endif /* PHOSPHOR_HAS_LIBARCHIVE */
  } else {
    /* local template -- resolve to absolute path */
    if (ph_path_is_absolute(tmpl_val)) {
      template_abs = ph_path_normalize(tmpl_val);
    } else {
      char *joined = ph_path_join(cwd, tmpl_val);
      if (joined) {
        template_abs = ph_path_normalize(joined);
        ph_free(joined);
      }
    }
  }

  if (!template_abs) {
    ph_log_error("template not found: %s", tmpl_val);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_FS;
  }

  /* verify template directory exists */
  ph_fs_stat_t tmpl_st;
  if (ph_fs_stat(template_abs, &tmpl_st) != PH_OK || !tmpl_st.is_dir) {
    ph_log_error("template is not a directory: %s", template_abs);
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_FS;
  }

  if (ph_signal_interrupted()) {
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_SIGNAL;
  }

  /* step 3: load manifest */
  ph_log_debug("create: template resolved to %s", template_abs);
  char *manifest_path = ph_manifest_find(template_abs);
  if (!manifest_path) {
    ph_log_error("create: no manifest found in %s "
                 "(tried template.phosphor.toml, manifest.toml)",
                 template_abs);
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_CONFIG;
  }

  ph_manifest_t manifest;
  memset(&manifest, 0, sizeof(manifest));
  if (ph_manifest_load(manifest_path, &manifest, &err) != PH_OK) {
    ph_log_error("manifest error: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_CONFIG;
    ph_error_destroy(err);
    ph_free(manifest_path);
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return exit_code;
  }
  ph_free(manifest_path);
  ph_log_debug("create: manifest loaded (%zu variables, %zu ops)",
               manifest.variable_count, manifest.op_count);

  if (ph_signal_interrupted()) {
    ph_manifest_destroy(&manifest);
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_SIGNAL;
  }

  /* step 4: discover project config */
  ph_config_t project_config;
  memset(&project_config, 0, sizeof(project_config));
  if (ph_config_discover(cwd, &project_config, &err) != PH_OK) {
    ph_log_warn("config discovery: %s", err ? err->message : "unknown");
    ph_error_destroy(err);
    err = NULL;
    /* not fatal -- continue without config */
  }

  if (ph_signal_interrupted()) {
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_SIGNAL;
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
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return exit_code;
  }

  ph_log_debug("create: variable merge complete (%zu variables)", var_count);
  for (size_t i = 0; i < var_count; i++)
    ph_log_debug("create:   %s = %s", vars[i].name, vars[i].value);

  if (ph_signal_interrupted()) {
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
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
    ph_free(template_abs);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_INTERNAL;
  }

  /* resolve template root: template_abs + source_root */
  char *template_root = NULL;
  if (manifest.tmpl.source_root) {
    template_root = ph_path_join(template_abs, manifest.tmpl.source_root);
  } else {
    size_t len = strlen(template_abs);
    template_root = ph_alloc(len + 1);
    if (template_root)
      memcpy(template_root, template_abs, len + 1);
  }

  /* template_abs no longer needed after deriving template_root */
  ph_free(template_abs);
  template_abs = NULL;

  if (!template_root) {
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
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
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return exit_code;
  }

  ph_log_debug("create: plan built (%zu operations)", plan.count);

  if (ph_signal_interrupted()) {
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_SIGNAL;
  }

  /* step 7: preflight checks */
  if (preflight_check(&plan, force, &err) != PH_OK) {
    ph_log_error("preflight: %s", err ? err->message : "unknown");
    exit_code = err ? (int)err->category : PH_ERR_FS;
    ph_error_destroy(err);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return exit_code;
  }

  /* step 8: dry-run */
  if (dry_run) {
    print_plan(&plan);
    ph_plan_destroy(&plan);
    ph_free(template_root);
    ph_free(dest_dir);
    ph_resolved_vars_destroy(vars, var_count);
    ph_config_destroy(&project_config);
    ph_manifest_destroy(&manifest);
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
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
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
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
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return exit_code;
  }

  /* execute plan into staging */
  ph_log_debug("create: executing plan into staging at %s", staging.path);
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
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
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
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return PH_ERR_SIGNAL;
  }

  /* commit staging to destination */
  ph_log_debug("create: committing staging to %s", dest_dir);
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
    ph_git_cleanup_clone(clone_dir, NULL);
    ph_free(clone_dir);
    ph_archive_cleanup_extract(extract_dir, NULL);
    ph_free(extract_dir);
    return exit_code;
  }

  /* step 10: summary report */
  print_summary(&stats);
  ph_log_info("project created at: %s", dest_dir);

  /* cleanup */
  ph_staging_destroy(&staging);
  ph_plan_destroy(&plan);
  ph_free(template_root);
  ph_free(dest_dir);
  ph_resolved_vars_destroy(vars, var_count);
  ph_config_destroy(&project_config);
  ph_manifest_destroy(&manifest);
  ph_git_cleanup_clone(clone_dir, NULL);
  ph_free(clone_dir);
  ph_archive_cleanup_extract(extract_dir, NULL);
  ph_free(extract_dir);

  return 0;
}

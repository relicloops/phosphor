#include "phosphor/alloc.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/error.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/proc.h"
#include "phosphor/signal.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- well-known build output directories ---- */

static const char *const build_dirs[] = {
    "build",
    "public",
};

static const char *const wipe_dirs[] = {
    "build",
    "public",
    "node_modules",
};

/* ---- stale staging directory scanner ---- */

static const char stale_prefix[] = ".phosphor-staging-";

static int scan_stale_dirs(const char *project_root, bool dry_run,
                           bool verbose, size_t *out_removed) {
    DIR *d = opendir(project_root);
    if (!d) {
        ph_log_error("clean: cannot open project root: %s (%s)",
                      project_root, strerror(errno));
        return PH_ERR_FS;
    }

    size_t removed = 0;
    struct dirent *ent;

    while ((ent = readdir(d)) != NULL) {
        if (ph_signal_interrupted()) {
            closedir(d);
            return PH_ERR_SIGNAL;
        }

        if (strncmp(ent->d_name, stale_prefix, sizeof(stale_prefix) - 1) != 0)
            continue;

        char *full = ph_path_join(project_root, ent->d_name);
        if (!full) continue;

        ph_fs_stat_t st;
        if (ph_fs_stat(full, &st) != PH_OK || !st.exists || !st.is_dir) {
            ph_free(full);
            continue;
        }

        if (dry_run) {
            ph_log_info("clean: would remove stale: %s", full);
        } else {
            if (verbose)
                ph_log_info("clean: removing stale: %s", full);

            ph_error_t *err = NULL;
            if (ph_fs_rmtree(full, &err) != PH_OK) {
                ph_log_error("clean: failed to remove %s: %s",
                              full, err ? err->message : "unknown error");
                ph_error_destroy(err);
                ph_free(full);
                closedir(d);
                return PH_ERR_FS;
            }
            removed++;
        }

        ph_free(full);
    }

    closedir(d);
    if (out_removed) *out_removed = removed;
    return 0;
}

/* ---- legacy: clean via shell scripts ---- */

/* audit fix (finding 12): forward --dry-run and --wipe to the script
 * so the legacy path honors the same flags the native path does. */
static int clean_via_scripts(const char *project_root_abs,
                             bool dry_run, bool wipe) {
    /* validate that script exists */
    char *script_path = ph_path_join(project_root_abs,
                                     "scripts/_default/clean.sh");
    if (!script_path) return PH_ERR_INTERNAL;

    ph_fs_stat_t st;
    if (ph_fs_stat(script_path, &st) != PH_OK || !st.is_file) {
        ph_log_error("clean: legacy: missing clean script: %s", script_path);
        ph_free(script_path);
        return PH_ERR_VALIDATE;
    }
    ph_free(script_path);

    ph_argv_builder_t ab;
    if (ph_argv_init(&ab, 8) != PH_OK) return PH_ERR_INTERNAL;

    ph_argv_push(&ab, "sh");
    ph_argv_push(&ab, "scripts/_default/clean.sh");
    if (dry_run) ph_argv_push(&ab, "--dry-run");
    if (wipe)    ph_argv_push(&ab, "--wipe");

    char **argv = ph_argv_finalize(&ab);
    if (!argv) return PH_ERR_INTERNAL;

    const char *extras[] = {
        "SNI", "TLD", "SITE_", "DEFAULT_", "NODE_",
        "NPM_", "ROOT_", "SCRIPTS_", NULL
    };

    ph_env_t env;
    if (ph_env_build(extras, &env) != PH_OK) {
        ph_argv_free(argv);
        return PH_ERR_INTERNAL;
    }

    if (ph_signal_interrupted()) {
        ph_env_destroy(&env);
        ph_argv_free(argv);
        return PH_ERR_SIGNAL;
    }

    ph_log_info("clean: legacy: running scripts/_default/clean.sh in %s",
                 project_root_abs);

    ph_proc_opts_t opts = {
        .argv        = argv,
        .cwd         = project_root_abs,
        .env         = &env,
        .timeout_sec = 0,
    };

    int child_exit = 0;
    ph_result_t rc = ph_proc_exec(&opts, &child_exit);

    ph_env_destroy(&env);
    ph_argv_free(argv);

    if (rc != PH_OK) {
        ph_log_error("clean: legacy: failed to spawn clean process");
        return PH_ERR_PROCESS;
    }

    if (child_exit != 0)
        ph_log_error("clean: legacy: script failed (exit %d)", child_exit);
    else
        ph_log_info("clean: legacy: completed");

    return child_exit;
}

/* audit fix (findings 2 and 12): legacy path is compile-time gated */

static bool use_legacy_scripts(void) {
#ifdef PHOSPHOR_SCRIPT_FALLBACK
    return true;
#else
    return false;
#endif
}

/* ---- main clean pipeline ---- */

int ph_cmd_clean(const ph_cli_config_t *config,
                 const ph_parsed_args_t *args) {
    (void)config;

    /* step 1: extract flags */
    const char *project_val = ph_args_get_flag(args, "project");
    bool stale              = ph_args_has_flag(args, "stale");
    bool wipe               = ph_args_has_flag(args, "wipe");
    bool dry_run            = ph_args_has_flag(args, "dry-run");
    bool verbose            = ph_args_has_flag(args, "verbose");

    if (verbose)
        ph_log_set_level(PH_LOG_DEBUG);

    /* step 2: resolve project root to absolute path */
    char *project_root_abs = NULL;

    if (project_val) {
        if (ph_path_is_absolute(project_val)) {
            project_root_abs = ph_path_normalize(project_val);
        } else {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) {
                ph_log_error("clean: failed to get current directory");
                return PH_ERR_INTERNAL;
            }
            char *joined = ph_path_join(cwd, project_val);
            if (joined) {
                project_root_abs = ph_path_normalize(joined);
                ph_free(joined);
            }
        }
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            ph_log_error("clean: failed to get current directory");
            return PH_ERR_INTERNAL;
        }
        project_root_abs = ph_path_normalize(cwd);
    }

    if (!project_root_abs) {
        ph_log_error("clean: failed to resolve project root");
        return PH_ERR_INTERNAL;
    }

    /* audit fix: reject --legacy-scripts when not compiled in */
#ifndef PHOSPHOR_SCRIPT_FALLBACK
    if (ph_args_has_flag(args, "legacy-scripts")) {
        ph_log_error("clean: --legacy-scripts requires compilation with "
                     "-Dscript_fallback=true");
        ph_free(project_root_abs);
        return PH_ERR_USAGE;
    }
#endif

    /* step 3: verify project root exists */
    ph_fs_stat_t root_st;
    if (ph_fs_stat(project_root_abs, &root_st) != PH_OK ||
        !root_st.exists || !root_st.is_dir) {
        ph_log_error("clean: project root does not exist: %s",
                      project_root_abs);
        ph_free(project_root_abs);
        return PH_ERR_VALIDATE;
    }

    /* legacy scripts dispatch (not for --stale; stale is always native) */
    if (!stale && use_legacy_scripts()) {
        ph_log_warn("clean: using deprecated shell-script fallback; "
                     "recompile without -Dscript_fallback=true to disable");

        int rc = clean_via_scripts(project_root_abs, dry_run, wipe);
        ph_free(project_root_abs);
        return rc;
    }

    /* step 4: handle --stale mode */
    if (stale) {
        size_t stale_removed = 0;
        int rc = scan_stale_dirs(project_root_abs, dry_run, verbose,
                                 &stale_removed);
        if (rc != 0) {
            ph_free(project_root_abs);
            return rc;
        }

        if (!dry_run) {
            if (stale_removed > 0)
                ph_log_info("clean: removed %zu stale staging dir(s)",
                             stale_removed);
            else
                ph_log_info("clean: no stale staging directories found");
        }

        ph_free(project_root_abs);
        return 0;
    }

    /* step 5: standard clean -- remove well-known build output dirs */
    if (ph_signal_interrupted()) {
        ph_free(project_root_abs);
        return PH_ERR_SIGNAL;
    }

    size_t removed_count = 0;

    const char *const *dirs = wipe ? wipe_dirs : build_dirs;
    size_t dir_count = wipe
        ? sizeof(wipe_dirs) / sizeof(wipe_dirs[0])
        : sizeof(build_dirs) / sizeof(build_dirs[0]);

    for (size_t i = 0; i < dir_count; i++) {
        if (ph_signal_interrupted()) {
            ph_free(project_root_abs);
            return PH_ERR_SIGNAL;
        }

        char *target = ph_path_join(project_root_abs, dirs[i]);
        if (!target) {
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }

        ph_fs_stat_t st;
        if (ph_fs_stat(target, &st) != PH_OK || !st.exists || !st.is_dir) {
            if (verbose)
                ph_log_debug("clean: skipping (not found): %s", target);
            ph_free(target);
            continue;
        }

        if (dry_run) {
            ph_log_info("clean: would remove: %s", target);
        } else {
            if (verbose)
                ph_log_info("clean: removing: %s", target);

            ph_error_t *err = NULL;
            if (ph_fs_rmtree(target, &err) != PH_OK) {
                ph_log_error("clean: failed to remove %s: %s",
                              target, err ? err->message : "unknown error");
                ph_error_destroy(err);
                ph_free(target);
                ph_free(project_root_abs);
                return PH_ERR_FS;
            }
            removed_count++;
        }

        ph_free(target);
    }

    /* step 6: report */
    if (!dry_run) {
        if (removed_count > 0)
            ph_log_info("clean: removed %zu directory(ies)", removed_count);
        else
            ph_log_info("clean: nothing to remove");
    }

    ph_free(project_root_abs);
    return 0;
}

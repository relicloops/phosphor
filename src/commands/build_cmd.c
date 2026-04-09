#include "phosphor/alloc.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/config.h"
#include "phosphor/error.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/manifest.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/proc.h"
#include "phosphor/certs.h"
#include "phosphor/signal.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- build report ---- */

typedef struct {
    const char *project_root;
    const char *deploy_at;
    int         child_exit;
    size_t      metadata_removed;
    size_t      metadata_warnings;
} build_report_t;

static void report_plain(const build_report_t *r) {
    if (r->child_exit == 0) {
        ph_log_info("build: completed successfully");
    } else {
        ph_log_error("build: failed with exit code %d", r->child_exit);
    }
    ph_log_info("  project: %s", r->project_root);
    if (r->deploy_at)
        ph_log_info("  deploy-at: %s", r->deploy_at);
    if (r->metadata_removed > 0)
        ph_log_info("  metadata removed: %zu", r->metadata_removed);
    if (r->metadata_warnings > 0)
        ph_log_warn("  metadata warnings: %zu", r->metadata_warnings);
}

static void report_toml(const build_report_t *r) {
    printf("[build]\n");
    printf("status = \"%s\"\n", r->child_exit == 0 ? "success" : "failure");
    printf("exit_code = %d\n", r->child_exit);
    printf("project = \"%s\"\n", r->project_root);
    if (r->deploy_at)
        printf("deploy_at = \"%s\"\n", r->deploy_at);
    printf("metadata_removed = %zu\n", r->metadata_removed);
    printf("warnings = %zu\n", r->metadata_warnings);
}

/* ---- post-build metadata cleanup ---- */

typedef struct {
    size_t removed;
    size_t warnings;
} cleanup_result_t;

static cleanup_result_t cleanup_metadata(const char *dir_path, int depth) {
    cleanup_result_t result = {0, 0};

    if (depth > PH_MAX_DIR_DEPTH) {
        ph_log_warn("build: metadata cleanup exceeded max depth at %s",
                     dir_path);
        result.warnings++;
        return result;
    }

    DIR *d = opendir(dir_path);
    if (!d) {
        ph_log_debug("build: cannot open directory for cleanup: %s (%s)",
                      dir_path, strerror(errno));
        return result;
    }

    struct dirent *ent;

    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char *full = ph_path_join(dir_path, ent->d_name);
        if (!full) continue;

        ph_fs_stat_t st;
        if (ph_fs_stat(full, &st) != PH_OK || !st.exists) {
            ph_free(full);
            continue;
        }

        if (st.is_dir) {
            cleanup_result_t sub = cleanup_metadata(full, depth + 1);
            result.removed  += sub.removed;
            result.warnings += sub.warnings;
        } else if (st.is_file && ph_metadata_is_denied(ent->d_name)) {
            if (unlink(full) == 0) {
                ph_log_debug("build: removed metadata file: %s", full);
                result.removed++;
            } else {
                ph_log_warn("build: failed to remove %s: %s",
                             full, strerror(errno));
                result.warnings++;
            }
        }

        ph_free(full);
    }

    closedir(d);
    return result;
}

/* ---- deploy-at path escape guard ---- */

/* audit fix: canonical containment check via ph_path_is_under. */
static bool deploy_at_escapes_root(const char *deploy_at_abs,
                                   const char *project_root_abs) {
    return !ph_path_is_under(deploy_at_abs, project_root_abs);
}

/* ---- copytree filter: skip platform metadata ---- */

static bool metadata_skip_filter(const char *rel_path, bool is_dir,
                                 void *ctx) {
    (void)ctx;
    (void)is_dir;
    const char *base = strrchr(rel_path, '/');
    base = base ? base + 1 : rel_path;
    return !ph_metadata_is_denied(base);
}

/* ---- env helper ---- */

static const char *env_or(const char *key, const char *fallback) {
    const char *val = getenv(key);
    return (val && val[0] != '\0') ? val : fallback;
}

/* ---- legacy: build via shell scripts ---- */

static int build_via_scripts(const char *project_root_abs,
                             const char *deploy_at_abs,
                             bool clean_first,
                             const char *tld_val,
                             bool verbose,
                             bool toml_output,
                             bool strict) {
    (void)verbose;
    /* validate that scripts exist */
    char *script_path = ph_path_join(project_root_abs,
                                     "scripts/_default/all.sh");
    if (!script_path) return PH_ERR_INTERNAL;

    ph_fs_stat_t st;
    if (ph_fs_stat(script_path, &st) != PH_OK || !st.is_file) {
        ph_log_error("build: legacy: missing build script: %s", script_path);
        ph_free(script_path);
        return PH_ERR_VALIDATE;
    }
    ph_free(script_path);

    /* build argv */
    ph_argv_builder_t ab;
    if (ph_argv_init(&ab, 8) != PH_OK) return PH_ERR_INTERNAL;

    ph_argv_push(&ab, "sh");
    ph_argv_push(&ab, "scripts/_default/all.sh");

    if (clean_first)
        ph_argv_push(&ab, "--clean");

    if (deploy_at_abs) {
        ph_argv_push(&ab, "--public");
        ph_argv_push(&ab, deploy_at_abs);
    }

    if (tld_val) {
        ph_argv_push(&ab, "--tld");
        ph_argv_push(&ab, tld_val);
    }

    char **argv = ph_argv_finalize(&ab);
    if (!argv) return PH_ERR_INTERNAL;

    /* build sanitized environment */
    const char *extras[] = {
        "SNI", "TLD", "SITE_", "DEFAULT_", "NODE_",
        "NPM_", "ESBUILD_", "ROOT_", "SCRIPTS_", NULL
    };

    ph_env_t env;
    if (ph_env_build(extras, &env) != PH_OK) {
        ph_argv_free(argv);
        return PH_ERR_INTERNAL;
    }

    if (tld_val)
        ph_env_set(&env, "TLD", tld_val);

    if (ph_signal_interrupted()) {
        ph_env_destroy(&env);
        ph_argv_free(argv);
        return PH_ERR_SIGNAL;
    }

    if (!toml_output)
        ph_log_info("build: legacy: running scripts/_default/all.sh in %s",
                     project_root_abs);

    ph_proc_opts_t opts = {
        .argv        = argv,
        .cwd         = project_root_abs,
        .env         = &env,
        .timeout_sec = 0,
    };

    int child_exit = 0;
    ph_result_t rc = ph_proc_exec(&opts, &child_exit);

    if (ph_signal_interrupted()) {
        ph_env_destroy(&env);
        ph_argv_free(argv);
        return PH_ERR_SIGNAL;
    }

    if (rc != PH_OK) {
        ph_log_error("build: legacy: failed to spawn build process");
        ph_env_destroy(&env);
        ph_argv_free(argv);
        return PH_ERR_PROCESS;
    }

    /* post-build metadata cleanup */
    cleanup_result_t cr = {0, 0};
    if (child_exit == 0 && deploy_at_abs)
        cr = cleanup_metadata(deploy_at_abs, 0);

    build_report_t report = {
        .project_root      = project_root_abs,
        .deploy_at         = deploy_at_abs,
        .child_exit        = child_exit,
        .metadata_removed  = cr.removed,
        .metadata_warnings = cr.warnings,
    };

    if (toml_output) report_toml(&report);
    else             report_plain(&report);

    int exit_code = child_exit;
    if (strict && child_exit == 0 && cr.warnings > 0) {
        ph_log_error("build: --strict: %zu warning(s) treated as errors",
                      cr.warnings);
        exit_code = PH_ERR_VALIDATE;
    }

    ph_env_destroy(&env);
    ph_argv_free(argv);

    return exit_code;
}

/* ---- JS string escape for esbuild --define values ---- */

/*
 * escape_define_value -- return a heap-allocated JS-safe copy of `in`.
 * escapes: " -> \", \ -> \\, \n -> \n, \r -> \r, \t -> \t, and
 * control chars 0x00-0x1F -> \u00XX.  caller must ph_free().
 */
static char *escape_define_value(const char *in) {
    if (!in) {
        char *e = ph_alloc(1);
        if (e) e[0] = '\0';
        return e;
    }

    /* worst case: every char becomes \u00XX (6 chars) */
    size_t ilen = strlen(in);
    size_t cap = ilen * 6 + 1;
    char *out = ph_alloc(cap);
    if (!out) return NULL;

    size_t o = 0;
    for (size_t i = 0; i < ilen; i++) {
        unsigned char c = (unsigned char)in[i];
        switch (c) {
        case '"':  out[o++] = '\\'; out[o++] = '"';  break;
        case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
        case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
        case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
        case '\t': out[o++] = '\\'; out[o++] = 't';  break;
        default:
            if (c < 0x20) {
                o += (size_t)snprintf(out + o, cap - o, "\\u%04x", c);
            } else {
                out[o++] = (char)c;
            }
        }
    }
    out[o] = '\0';
    return out;
}

/* ---- check if legacy script mode is active ---- */

/* audit fix (findings 2 and 12): the legacy shell-script path is
 * compile-time gated via -Dscript_fallback=true. Previously the
 * runtime --legacy-scripts flag bypassed the gate and also skipped
 * the native deploy containment guard. Now:
 *   - use_legacy_scripts only checks the compile-time flag,
 *   - the runtime flag is rejected at parse time when the fallback
 *     is not compiled in (see PH_REJECT_LEGACY_SCRIPTS below). */
static bool use_legacy_scripts(void) {
#ifdef PHOSPHOR_SCRIPT_FALLBACK
    return true;
#else
    return false;
#endif
}

/* ---- main build pipeline ---- */

int ph_cmd_build(const ph_cli_config_t *config,
                 const ph_parsed_args_t *args) {
    (void)config;

    /* step 1: extract flags */
    const char *project_val   = ph_args_get_flag(args, "project");
    const char *deploy_at_val = ph_args_get_flag(args, "deploy-at");
    bool clean_first          = ph_args_has_flag(args, "clean-first");
    const char *tld_val       = ph_args_get_flag(args, "tld");
    bool verbose              = ph_args_has_flag(args, "verbose");
    bool strict               = ph_args_has_flag(args, "strict");
    bool toml_output          = ph_args_has_flag(args, "toml");

    /* normalize-eol stays reserved */
    if (ph_args_has_flag(args, "normalize-eol"))
        ph_log_warn("build: --normalize-eol is reserved for future use; "
                     "ignored");

    if (verbose)
        ph_log_set_level(PH_LOG_DEBUG);

    /* effective TLD: flag > env > default */
    const char *tld_eff = tld_val;
    if (!tld_eff) tld_eff = getenv("TLD");
    if (!tld_eff || !tld_eff[0]) tld_eff = ".host";

    /* step 2: resolve project root to absolute path */
    char *project_root_abs = NULL;

    if (project_val) {
        if (ph_path_is_absolute(project_val)) {
            project_root_abs = ph_path_normalize(project_val);
        } else {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) {
                ph_log_error("build: failed to get current directory");
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
            ph_log_error("build: failed to get current directory");
            return PH_ERR_INTERNAL;
        }
        project_root_abs = ph_path_normalize(cwd);
    }

    if (!project_root_abs) {
        ph_log_error("build: failed to resolve project root");
        return PH_ERR_INTERNAL;
    }

    /* audit fix: reject --legacy-scripts when not compiled in */
#ifndef PHOSPHOR_SCRIPT_FALLBACK
    if (ph_args_has_flag(args, "legacy-scripts")) {
        ph_log_error("build: --legacy-scripts requires compilation with "
                     "-Dscript_fallback=true");
        ph_free(project_root_abs);
        return PH_ERR_USAGE;
    }
#endif

    /* legacy scripts dispatch */
    if (use_legacy_scripts()) {
        ph_log_warn("build: using deprecated shell-script fallback; "
                     "recompile without -Dscript_fallback=true to disable");

        /* resolve deploy-at for legacy path */
        char *legacy_deploy = NULL;
        if (deploy_at_val) {
            if (ph_path_is_absolute(deploy_at_val))
                legacy_deploy = ph_path_normalize(deploy_at_val);
            else {
                char *j = ph_path_join(project_root_abs, deploy_at_val);
                if (j) {
                    legacy_deploy = ph_path_normalize(j);
                    ph_free(j);
                }
            }
        }

        /* audit fix: containment-check the legacy deploy path the
         * same way the native path does at deploy_at_escapes_root. */
        if (legacy_deploy &&
            deploy_at_escapes_root(legacy_deploy, project_root_abs)) {
            ph_log_error("build: legacy --deploy-at escapes project root: "
                         "%s", legacy_deploy);
            ph_free(legacy_deploy);
            ph_free(project_root_abs);
            return PH_ERR_VALIDATE;
        }

        int rc = build_via_scripts(project_root_abs, legacy_deploy,
                                   clean_first, tld_val, verbose,
                                   toml_output, strict);
        ph_free(legacy_deploy);
        ph_free(project_root_abs);
        return rc;
    }

    /* step 2b: try loading the template manifest for build config */
    bool has_manifest = false;
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    {
        char *manifest_path = ph_manifest_find(project_root_abs);
        if (manifest_path) {
            ph_error_t *merr = NULL;
            if (ph_manifest_load(manifest_path, &manifest, &merr)
                == PH_OK) {
                has_manifest = true;
                ph_log_debug("build: loaded manifest from %s",
                              manifest_path);
            } else {
                ph_log_warn("build: cannot parse manifest: %s",
                             merr ? merr->message : "unknown");
                ph_error_destroy(merr);
            }
            ph_free(manifest_path);
        }
    }

    /* step 3: validate project layout -- src/ must exist */
    char *src_dir = ph_path_join(project_root_abs, "src");
    if (!src_dir) {
        ph_free(project_root_abs);
        return PH_ERR_INTERNAL;
    }

    ph_fs_stat_t st;
    if (ph_fs_stat(src_dir, &st) != PH_OK || !st.is_dir) {
        ph_log_error("build: missing src/ directory in %s", project_root_abs);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_VALIDATE;
    }

    /* step 4: compute build and deploy paths */
    char *build_dir = ph_path_join(project_root_abs, "build/src");
    if (!build_dir) {
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_INTERNAL;
    }

    /* step 4: compute deploy path -- priority chain:
     *   1. CLI --deploy-at flag (explicit override)
     *   2. [deploy] public_dir from manifest
     *   3. Auto-derive from first [[certs.domains]] name -> "public/{name}"
     *   4. Env fallback: public/{SNI}{TLD}
     */
    char *deploy_dir = NULL;

    if (deploy_at_val) {
        /* tier 1: --deploy-at flag */
        if (ph_path_is_absolute(deploy_at_val)) {
            deploy_dir = ph_path_normalize(deploy_at_val);
        } else {
            char *joined = ph_path_join(project_root_abs, deploy_at_val);
            if (joined) {
                deploy_dir = ph_path_normalize(joined);
                ph_free(joined);
            }
        }
        if (!deploy_dir) {
            ph_log_error("build: failed to resolve --deploy-at path");
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }

        /* validate: deploy-at must stay within project root */
        if (deploy_at_escapes_root(deploy_dir, project_root_abs)) {
            ph_log_error("build: --deploy-at escapes project root: %s",
                          deploy_dir);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return PH_ERR_VALIDATE;
        }
    }

    if (!deploy_dir && has_manifest && manifest.deploy.present &&
        manifest.deploy.public_dir) {
        /* tier 2: [deploy] public_dir from manifest */
        char *joined = ph_path_join(project_root_abs,
                                     manifest.deploy.public_dir);
        if (joined) {
            deploy_dir = ph_path_normalize(joined);
            ph_free(joined);
        }
        if (deploy_dir) {
            /* audit fix: manifest public_dir must stay within project root */
            if (deploy_at_escapes_root(deploy_dir, project_root_abs)) {
                ph_log_error("build: [deploy] public_dir escapes project "
                              "root: %s", deploy_dir);
                ph_free(deploy_dir);
                ph_free(build_dir);
                ph_free(src_dir);
                if (has_manifest) ph_manifest_destroy(&manifest);
                ph_free(project_root_abs);
                return PH_ERR_VALIDATE;
            }
            ph_log_debug("build: deploy path from [deploy] public_dir: %s",
                          deploy_dir);
        }
    }

    if (!deploy_dir && has_manifest) {
        /* tier 3: auto-derive from first [[certs.domains]] name */
        char *toml_path = ph_manifest_find(project_root_abs);
        if (toml_path) {
            ph_certs_config_t certs_cfg;
            memset(&certs_cfg, 0, sizeof(certs_cfg));
            ph_error_t *cerr = NULL;
            if (ph_certs_config_parse(toml_path, &certs_cfg, &cerr) == PH_OK
                && certs_cfg.present && certs_cfg.domain_count > 0
                && certs_cfg.domains[0].name) {

                char *pub = ph_path_join(project_root_abs, "public");
                if (pub) {
                    deploy_dir = ph_path_join(pub,
                                               certs_cfg.domains[0].name);
                    ph_free(pub);
                }
                if (deploy_dir)
                    ph_log_debug("build: deploy path from "
                                  "certs.domains[0].name: %s", deploy_dir);
            }
            ph_error_destroy(cerr);
            ph_certs_config_destroy(&certs_cfg);
            ph_free(toml_path);
        }
    }

    if (!deploy_dir) {
        /* tier 4: env fallback -- public/{SNI}{TLD} */
        const char *sni = env_or("SNI", "unknown");
        size_t sni_tld_len = strlen(sni) + strlen(tld_eff) + 1;
        char *sni_tld = ph_alloc(sni_tld_len);
        if (!sni_tld) {
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }
        snprintf(sni_tld, sni_tld_len, "%s%s", sni, tld_eff);

        char *pub = ph_path_join(project_root_abs, "public");
        if (pub) {
            deploy_dir = ph_path_join(pub, sni_tld);
            ph_free(pub);
        }
        ph_free(sni_tld);

        if (!deploy_dir) {
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }
    }

    /* audit fix (2026-04-07): unified deploy_dir containment gate.
     * Tiers 1 and 2 already had per-tier guards, but tier 3
     * (certs.domains[0].name-derived) and tier 4 (env-derived) wrote
     * unchecked values straight into ph_fs_rmtree below. Funnel every
     * resolved deploy path through the same canonical containment check so
     * a hostile manifest, a malformed cert domain, or a poisoned $SNI/$TLD
     * cannot point deployment outside the project root and have us
     * recursively delete it. Defense-in-depth: redundant for tiers 1+2,
     * but cheap and uniform. */
    if (deploy_at_escapes_root(deploy_dir, project_root_abs)) {
        ph_log_error("build: resolved deploy directory escapes project "
                      "root: %s", deploy_dir);
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        if (has_manifest) ph_manifest_destroy(&manifest);
        ph_free(project_root_abs);
        return PH_ERR_VALIDATE;
    }

    /* step 5: --clean-first removes build and deploy dirs */
    if (clean_first) {
        ph_error_t *err = NULL;
        ph_fs_rmtree(build_dir, &err);
        ph_error_destroy(err);
        err = NULL;
        ph_fs_rmtree(deploy_dir, &err);
        ph_error_destroy(err);
    }

    /* step 6: reset build dir (always start fresh) */
    {
        ph_error_t *err = NULL;
        ph_fs_rmtree(build_dir, &err);
        ph_error_destroy(err);
    }
    if (ph_fs_mkdir_p(build_dir, 0755) != PH_OK) {
        ph_log_error("build: cannot create build directory: %s", build_dir);
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_FS;
    }

    if (ph_fs_mkdir_p(deploy_dir, 0755) != PH_OK) {
        ph_log_error("build: cannot create deploy directory: %s", deploy_dir);
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_FS;
    }

    /* step 7: build sanitized environment */
    const char *extras[] = {
        "SNI", "TLD", "SITE_", "DEFAULT_", "NODE_",
        "NPM_", "ESBUILD_", "ROOT_", "SCRIPTS_", NULL
    };

    ph_env_t env;
    if (ph_env_build(extras, &env) != PH_OK) {
        ph_log_error("build: failed to construct environment");
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_INTERNAL;
    }

    if (tld_val)
        ph_env_set(&env, "TLD", tld_val);

    /* step 8: ensure esbuild is available (npm install if needed) */
    if (ph_signal_interrupted()) {
        ph_env_destroy(&env);
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_SIGNAL;
    }

    char *esbuild_bin = ph_path_join(project_root_abs,
                                     "node_modules/.bin/esbuild");
    if (!esbuild_bin) {
        ph_env_destroy(&env);
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_INTERNAL;
    }

    ph_fs_stat_t esbuild_st;
    if (ph_fs_stat(esbuild_bin, &esbuild_st) != PH_OK ||
        !esbuild_st.exists) {
        if (!toml_output)
            ph_log_info("build: esbuild not found, running npm install");

        ph_argv_builder_t npm_ab;
        if (ph_argv_init(&npm_ab, 4) != PH_OK) {
            ph_free(esbuild_bin);
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }
        ph_argv_push(&npm_ab, "npm");
        ph_argv_push(&npm_ab, "install");
        char **npm_argv = ph_argv_finalize(&npm_ab);
        if (!npm_argv) {
            ph_free(esbuild_bin);
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }

        ph_proc_opts_t npm_opts = {
            .argv        = npm_argv,
            .cwd         = project_root_abs,
            .env         = &env,
            .timeout_sec = 0,
        };

        int npm_exit = 0;
        ph_result_t npm_rc = ph_proc_exec(&npm_opts, &npm_exit);
        ph_argv_free(npm_argv);

        if (npm_rc != PH_OK || npm_exit != 0) {
            ph_log_error("build: npm install failed (exit %d)", npm_exit);
            ph_free(esbuild_bin);
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_PROCESS;
        }
    }
    ph_free(esbuild_bin);

    /* step 9: run esbuild */
    if (ph_signal_interrupted()) {
        ph_env_destroy(&env);
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_SIGNAL;
    }

    if (!toml_output)
        ph_log_info("build: running esbuild in %s", project_root_abs);

    {
        char *esbuild_path = ph_path_join(project_root_abs,
                                          "node_modules/.bin/esbuild");
        if (!esbuild_path) {
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }

        ph_argv_builder_t ab;
        if (ph_argv_init(&ab, 32) != PH_OK) {
            ph_free(esbuild_path);
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }

        ph_argv_push(&ab, esbuild_path);

        const char *entry_point = "src/app.tsx";
        if (has_manifest && manifest.build.present && manifest.build.entry)
            entry_point = manifest.build.entry;
        ph_argv_push(&ab, entry_point);
        ph_argv_push(&ab, "--bundle");
        ph_argv_push(&ab, "--minify");
        ph_argv_push(&ab, "--format=esm");
        ph_argv_push(&ab, "--platform=browser");
        ph_argv_push(&ab, "--target=es2020");
        ph_argv_push(&ab, "--jsx=transform");
        ph_argv_push(&ab, "--jsx-factory=h");
        ph_argv_push(&ab, "--jsx-fragment=Fragment");
        ph_argv_push(&ab, "--splitting");
        ph_argv_push(&ab, "--entry-names=[name]");

        /* audit fix (finding 3): escape define values as JS string
         * literals before passing to esbuild. Unescaped ", \, newlines,
         * or control characters produce malformed --define arguments. */
        /* inject build-time defines */
        if (has_manifest && manifest.build.present &&
            manifest.build.define_count > 0) {
            for (size_t i = 0; i < manifest.build.define_count; i++) {
                ph_build_define_t *d = &manifest.build.defines[i];
                const char *val = d->default_val ? d->default_val : "";
                if (d->env) {
                    const char *env_val = getenv(d->env);
                    if (env_val && env_val[0] != '\0') val = env_val;
                }

                /* auto-populate *_PUBLIC_DIR__ from resolved deploy path */
                if ((!val || val[0] == '\0') && deploy_dir) {
                    size_t nlen = strlen(d->name);
                    if (nlen >= 14 &&
                        strcmp(d->name + nlen - 14, "_PUBLIC_DIR__") == 0) {
                        /* use project-relative deploy path */
                        size_t rlen = strlen(project_root_abs);
                        if (strncmp(deploy_dir, project_root_abs, rlen) == 0
                            && deploy_dir[rlen] == '/') {
                            val = deploy_dir + rlen + 1;
                        } else {
                            val = deploy_dir;
                        }
                        ph_log_debug("build: auto-populated %s = \"%s\" "
                                      "from deploy path", d->name, val);
                    }
                }

                char *esc = escape_define_value(val);
                if (esc) {
                    ph_argv_pushf(&ab, "--define:%s=\"%s\"", d->name, esc);
                    ph_free(esc);
                }
            }
        } else {
            /* legacy hardcoded defines for backward compat */
            const char *legacy_pairs[][2] = {
                {"__TLD__",            NULL},
                {"__SITE_OWNER__",     NULL},
                {"__SITE_OWNER_SLUG__",NULL},
                {"__SITE_GITHUB__",    NULL},
                {"__SITE_INSTAGRAM__", NULL},
                {"__SITE_X__",         NULL},
            };
            legacy_pairs[0][1] = tld_eff;
            legacy_pairs[1][1] = env_or("SITE_OWNER", "Unknown Owner");
            legacy_pairs[2][1] = env_or("SITE_OWNER_SLUG", "unknown-owner");
            legacy_pairs[3][1] = env_or("SITE_GITHUB", "https://github.com");
            legacy_pairs[4][1] = env_or("SITE_INSTAGRAM",
                                         "https://www.instagram.com");
            legacy_pairs[5][1] = env_or("SITE_X", "https://x.com");
            for (size_t i = 0; i < 6; i++) {
                char *esc = escape_define_value(legacy_pairs[i][1]);
                if (esc) {
                    ph_argv_pushf(&ab, "--define:%s=\"%s\"",
                                   legacy_pairs[i][0], esc);
                    ph_free(esc);
                }
            }
        }

        ph_argv_pushf(&ab, "--outdir=%s", build_dir);

        char **argv = ph_argv_finalize(&ab);
        ph_free(esbuild_path);
        if (!argv) {
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }

        ph_proc_opts_t opts = {
            .argv        = argv,
            .cwd         = project_root_abs,
            .env         = &env,
            .timeout_sec = 0,
        };

        int child_exit = 0;
        ph_result_t rc = ph_proc_exec(&opts, &child_exit);
        ph_argv_free(argv);

        if (ph_signal_interrupted()) {
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_SIGNAL;
        }

        if (rc != PH_OK) {
            ph_log_error("build: failed to spawn esbuild");
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_PROCESS;
        }

        if (child_exit != 0) {
            /* esbuild failed -- report and bail */
            build_report_t report = {
                .project_root = project_root_abs,
                .deploy_at    = deploy_dir,
                .child_exit   = child_exit,
            };
            if (toml_output) report_toml(&report);
            else             report_plain(&report);

            if (has_manifest)
                ph_manifest_destroy(&manifest);
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return child_exit;
        }
    }

    /* step 10: copy static assets into build dir */
    {
        char *static_dir = ph_path_join(src_dir, "static");
        if (static_dir) {
            ph_fs_stat_t static_st;
            if (ph_fs_stat(static_dir, &static_st) == PH_OK &&
                static_st.is_dir) {
                if (verbose)
                    ph_log_debug("build: copying static assets from %s",
                                  static_dir);

                ph_error_t *err = NULL;
                if (ph_fs_copytree(static_dir, build_dir,
                                   metadata_skip_filter, NULL, &err) != PH_OK) {
                    ph_log_error("build: failed to copy static assets: %s",
                                  err ? err->message : "unknown");
                    ph_error_destroy(err);
                    ph_env_destroy(&env);
                    ph_free(static_dir);
                    ph_free(deploy_dir);
                    ph_free(build_dir);
                    ph_free(src_dir);
                    ph_free(project_root_abs);
                    return PH_ERR_FS;
                }
            } else if (verbose) {
                ph_log_debug("build: no static/ directory, skipping");
            }
            ph_free(static_dir);
        }
    }

    /* step 11: deploy -- sync build output to deploy dir */
    if (ph_signal_interrupted()) {
        ph_env_destroy(&env);
        ph_free(deploy_dir);
        ph_free(build_dir);
        ph_free(src_dir);
        ph_free(project_root_abs);
        return PH_ERR_SIGNAL;
    }

    if (verbose)
        ph_log_debug("build: deploying %s -> %s", build_dir, deploy_dir);

    {
        /* clear deploy dir for deterministic sync (like rsync --delete) */
        ph_error_t *err = NULL;
        ph_fs_rmtree(deploy_dir, &err);
        ph_error_destroy(err);
        err = NULL;

        if (ph_fs_copytree(build_dir, deploy_dir,
                           metadata_skip_filter, NULL, &err) != PH_OK) {
            ph_log_error("build: failed to deploy: %s",
                          err ? err->message : "unknown");
            ph_error_destroy(err);
            ph_env_destroy(&env);
            ph_free(deploy_dir);
            ph_free(build_dir);
            ph_free(src_dir);
            ph_free(project_root_abs);
            return PH_ERR_FS;
        }
    }

    /* step 12: post-deploy metadata cleanup (safety net) */
    cleanup_result_t cr = cleanup_metadata(deploy_dir, 0);

    /* step 13: build report */
    build_report_t report = {
        .project_root      = project_root_abs,
        .deploy_at         = deploy_dir,
        .child_exit        = 0,
        .metadata_removed  = cr.removed,
        .metadata_warnings = cr.warnings,
    };

    if (toml_output)
        report_toml(&report);
    else
        report_plain(&report);

    /* step 14: strict mode -- warnings become errors */
    int exit_code = 0;
    if (strict && cr.warnings > 0) {
        ph_log_error("build: --strict: %zu warning(s) treated as errors",
                      cr.warnings);
        exit_code = PH_ERR_VALIDATE;
    }

    /* step 15: cleanup */
    if (has_manifest)
        ph_manifest_destroy(&manifest);
    ph_env_destroy(&env);
    ph_free(deploy_dir);
    ph_free(build_dir);
    ph_free(src_dir);
    ph_free(project_root_abs);

    return exit_code;
}

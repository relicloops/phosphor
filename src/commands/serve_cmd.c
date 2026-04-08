#include "phosphor/alloc.h"
#include "phosphor/certs.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/error.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/manifest.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/serve.h"
#include "phosphor/term.h"
#ifdef PHOSPHOR_HAS_NCURSES
#include "phosphor/dashboard.h"
#endif

#include <arpa/inet.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int flag_int(const ph_parsed_args_t *args, const char *name) {
    const char *v = ph_args_get_flag(args, name);
    return v ? atoi(v) : 0;
}

/* validate IPv4 or IPv6 address string */
static bool is_valid_ip(const char *addr) {
    if (!addr) return true; /* NULL = use default, always valid */
    struct in_addr  v4;
    struct in6_addr v6;
    if (inet_pton(AF_INET, addr, &v4) == 1) return true;
    if (inet_pton(AF_INET6, addr, &v6) == 1) return true;
    return false;
}

static void warn_privileged_port(const char *label, int port) {
    if (port > 0 && port < 1024)
        ph_log_info("serve: [warn] %s port %d is privileged (< 1024) "
                     "-- may require root", label, port);
}

/*
 * anchor_serve_path -- audit fix (2026-04-07T20-37-55Z).
 *
 * Anchor a relative serve config path under project_root_abs so the value
 * is stable regardless of phosphor's caller-cwd.
 *
 * Returns:
 *   NULL  -- raw is NULL, already absolute, or (when bare_ok) is a bare
 *            command name with no '/' (PATH lookup). The caller should
 *            pass `raw` through unchanged.
 *   heap  -- a newly allocated joined path. The caller MUST track it and
 *            free it after the cfg is no longer in use.
 *
 * `bare_ok` is for bin_path-style fields where a name without '/' must be
 * left alone so the OS PATH lookup keeps working.
 */
static char *anchor_serve_path(const char *raw,
                                const char *project_root_abs,
                                bool bare_ok) {
    if (!raw) return NULL;
    if (ph_path_is_absolute(raw)) return NULL;
    if (bare_ok && strchr(raw, '/') == NULL) return NULL;
    return ph_path_join(project_root_abs, raw);
}

/*
 * serve_validate_under_root -- audit fix (2026-04-08T14-06-47Z).
 *
 * Reject resolved serve paths that escape project_root_abs. Called
 * after anchor_serve_path has run so either:
 *   - the raw value was absolute and cfg.<field> points at the raw
 *     (no anchoring happened), or
 *   - the raw value was relative and cfg.<field> points at a derived
 *     path joined under project_root_abs.
 *
 * In both cases the final path must canonicalize to a location under
 * project_root_abs. bin_path fields are excluded from this check at
 * the call site -- bare names resolve via PATH downstream.
 *
 * Returns 0 on success (or if path is NULL, which means "not set");
 * returns PH_ERR_VALIDATE on failure and logs the offending field.
 * Callers are responsible for the cfg cleanup cascade.
 */
static int serve_validate_under_root(const char *label,
                                      const char *path,
                                      const char *project_root_abs) {
    if (!path) return 0;
    if (ph_path_has_traversal(path)) {
        ph_log_error("serve: %s contains path traversal: %s", label, path);
        return PH_ERR_VALIDATE;
    }
    if (!ph_path_is_under(path, project_root_abs)) {
        ph_log_error("serve: %s escapes project root: %s", label, path);
        return PH_ERR_VALIDATE;
    }
    return 0;
}

/* Heap-allocated derived path pointers for the serve config. Each field is
 * either NULL (no allocation -- the cfg uses the raw flag/manifest value)
 * or owned by this struct and freed via serve_derived_paths_free.
 *
 * Centralizing these here removes the per-cleanup-site ph_free chains that
 * grow with every new anchored field. */
struct serve_derived_paths {
    char *www_root;
    char *redir_acme_webroot;
    char *ns_bin;
    char *ns_working_dir;
    char *ns_log_directory;
    char *ns_upload_dir;
    char *ns_augments_dir;
    char *ns_grafts_dir;
    char *redir_bin;
    char *redir_working_dir;
};

static void serve_derived_paths_free(struct serve_derived_paths *p) {
    if (!p) return;
    ph_free(p->www_root);
    ph_free(p->redir_acme_webroot);
    ph_free(p->ns_bin);
    ph_free(p->ns_working_dir);
    ph_free(p->ns_log_directory);
    ph_free(p->ns_upload_dir);
    ph_free(p->ns_augments_dir);
    ph_free(p->ns_grafts_dir);
    ph_free(p->redir_bin);
    ph_free(p->redir_working_dir);
    memset(p, 0, sizeof(*p));
}

int ph_cmd_serve(const ph_cli_config_t *config,
                 const ph_parsed_args_t *args) {
    (void)config;

    /* step 1: extract phosphor-only flags */
    const char *project_val    = ph_args_get_flag(args, "project");
    bool        verbose        = ph_args_has_flag(args, "verbose");
    const char *ns_bin         = ph_args_get_flag(args, "neonsignal-bin");
    const char *redir_bin      = ph_args_get_flag(args, "redirect-bin");
    bool        no_redirect    = ph_args_has_flag(args, "no-redirect");
    bool        watch_flag     = ph_args_has_flag(args, "watch");
    const char *watch_cmd_flag = ph_args_get_flag(args, "watch-cmd");
    bool        no_dashboard   = ph_args_has_flag(args, "no-dashboard");

    if (verbose)
        ph_log_set_level(PH_LOG_DEBUG);

    /* step 2: resolve project root */
    char *project_root_abs = NULL;
    if (project_val) {
        if (ph_path_is_absolute(project_val)) {
            project_root_abs = ph_path_normalize(project_val);
        } else {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) {
                ph_log_error("serve: failed to get current directory");
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
            ph_log_error("serve: failed to get current directory");
            return PH_ERR_INTERNAL;
        }
        project_root_abs = ph_path_normalize(cwd);
    }
    if (!project_root_abs) {
        ph_log_error("serve: failed to resolve project root");
        return PH_ERR_INTERNAL;
    }
    ph_log_debug("serve: project root: %s", project_root_abs);

    /* step 3: load manifest for [deploy] and [certs] */
    bool has_manifest = false;
    ph_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));

    bool has_certs = false;
    ph_certs_config_t certs_cfg;
    memset(&certs_cfg, 0, sizeof(certs_cfg));

    {
        char *toml_path = ph_path_join(project_root_abs,
                                        "template.phosphor.toml");
        if (toml_path) {
            ph_fs_stat_t st;
            if (ph_fs_stat(toml_path, &st) == PH_OK && st.is_file) {
                ph_error_t *merr = NULL;
                if (ph_manifest_load(toml_path, &manifest, &merr) == PH_OK) {
                    has_manifest = true;
                    ph_log_debug("serve: loaded manifest from %s", toml_path);
                } else {
                    ph_log_warn("serve: cannot parse manifest: %s",
                                 merr ? merr->message : "unknown");
                    ph_error_destroy(merr);
                }

                /* parse certs separately */
                ph_error_t *cerr = NULL;
                if (ph_certs_config_parse(toml_path, &certs_cfg, &cerr)
                    == PH_OK && certs_cfg.present) {
                    has_certs = true;
                    ph_log_debug("serve: loaded [certs] from manifest");
                }
                ph_error_destroy(cerr);
            }
            ph_free(toml_path);
        }
    }

    /* step 4: check manifest guards
     *
     * skip if we can't determine www-root and certs-root from any source:
     *   - CLI flags (--www-root, --certs-root) override everything
     *   - [serve] section provides explicit config
     *   - [deploy] + [certs] sections provide derived values
     * if none of these sources exist, there's nothing to serve.
     */
    const char *www_root_flag   = ph_args_get_flag(args, "www-root");
    const char *certs_root_flag = ph_args_get_flag(args, "certs-root");

    bool has_explicit_paths = (www_root_flag && certs_root_flag);
    bool has_serve_section  = (has_manifest && manifest.serve.present);

    if (!has_explicit_paths && !has_serve_section) {
        if (has_manifest && !manifest.deploy.present) {
            ph_log_info("serve: skipping -- no [deploy] or [serve] in manifest");
            if (has_certs) ph_certs_config_destroy(&certs_cfg);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return 0;
        }
        if (!has_certs) {
            ph_log_info("serve: skipping -- no [certs] or [serve] in manifest");
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return 0;
        }
    }

    /* step 5: build serve config
     *
     * resolution chain (highest wins):
     *   tier 1: CLI flag
     *   tier 2: [serve] manifest section
     *   tier 3: [deploy]/[certs] derived values
     *   tier 4: NULL/0 (library defaults)
     */
    const ph_serve_manifest_config_t *ms =
        (has_manifest && manifest.serve.present) ? &manifest.serve : NULL;

    ph_serve_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.skip_redirect = no_redirect || (ms && ms->no_redirect);
    cfg.verbose = verbose;

    /* audit fix (2026-04-07T20-37-55Z): centralize all heap-allocated
     * derived path pointers in one struct. previously each anchored field
     * grew its own ph_free at every cleanup site, which was unsustainable
     * once we needed to anchor 8+ fields. */
    struct serve_derived_paths derived;
    memset(&derived, 0, sizeof(derived));

    /* neonsignal config -- tier 1 (flag) > tier 2 (manifest [serve]).
     * audit fix: anchor relative bin_path under project_root_abs so the
     * preflight access() check (run before any chdir) finds the right file
     * when phosphor is invoked from outside the project. bare names with
     * no '/' stay unanchored so PATH lookup keeps working. */
    {
        const char *raw = ns_bin ? ns_bin : (ms ? ms->ns_bin : NULL);
        derived.ns_bin = anchor_serve_path(raw, project_root_abs, true);
        cfg.ns.bin_path = derived.ns_bin ? derived.ns_bin : raw;
    }

    {
        int t1 = flag_int(args, "threads");
        cfg.ns.threads = t1 > 0 ? t1
                       : (ms ? ms->ns_threads : 0);
    }

    {
        const char *f = ph_args_get_flag(args, "host");
        cfg.ns.host = f ? f : (ms ? ms->ns_host : NULL);
    }

    {
        int t1 = flag_int(args, "port");
        cfg.ns.port = t1 > 0 ? t1
                    : (ms ? ms->ns_port : 0);
    }

    /* www-root: flag > [serve] > dirname(deploy.public_dir).
     * audit fix: when the value is manifest-relative we must anchor it under
     * project_root_abs, otherwise `phosphor serve --project /elsewhere` from
     * an unrelated cwd resolves www_root against the wrong directory. */
    if (www_root_flag) {
        if (ph_path_is_absolute(www_root_flag)) {
            cfg.ns.www_root = www_root_flag;
        } else {
            derived.www_root = ph_path_join(project_root_abs, www_root_flag);
            cfg.ns.www_root = derived.www_root;
        }
    } else if (ms && ms->ns_www_root) {
        if (ph_path_is_absolute(ms->ns_www_root)) {
            cfg.ns.www_root = ms->ns_www_root;
        } else {
            derived.www_root = ph_path_join(project_root_abs, ms->ns_www_root);
            cfg.ns.www_root = derived.www_root;
        }
        ph_log_debug("serve: www-root from [serve]: %s", cfg.ns.www_root);
    } else if (has_manifest && manifest.deploy.present &&
               manifest.deploy.public_dir) {
        char *parent = ph_path_dirname(manifest.deploy.public_dir);
        if (parent && !ph_path_is_absolute(parent)) {
            derived.www_root = ph_path_join(project_root_abs, parent);
            ph_free(parent);
        } else {
            derived.www_root = parent;
        }
        cfg.ns.www_root = derived.www_root;
        ph_log_debug("serve: www-root from [deploy]: %s", cfg.ns.www_root);
    }

    /* certs-root: flag > [serve] > certs.output_dir */
    if (certs_root_flag) {
        cfg.ns.certs_root = certs_root_flag;
    } else if (ms && ms->ns_certs_root) {
        cfg.ns.certs_root = ms->ns_certs_root;
        ph_log_debug("serve: certs-root from [serve]: %s",
                      cfg.ns.certs_root);
    } else if (has_certs && certs_cfg.output_dir) {
        cfg.ns.certs_root = certs_cfg.output_dir;
        ph_log_debug("serve: certs-root from [certs]: %s",
                      cfg.ns.certs_root);
    }

    /* audit fix (2026-04-07T20-37-55Z): anchor working/upload/augments/
     * grafts directories under project_root_abs when relative. all of
     * these end up either in chdir() of a child process or in mkdir()
     * called by phosphor itself before any chdir, so caller-cwd dependent
     * resolution is wrong. */
    {
        const char *f = ph_args_get_flag(args, "working-dir");
        const char *raw = f ? f : (ms ? ms->ns_working_dir : NULL);
        derived.ns_working_dir = anchor_serve_path(raw, project_root_abs, false);
        cfg.ns.working_dir = derived.ns_working_dir
                           ? derived.ns_working_dir : raw;
    }
    {
        const char *f = ph_args_get_flag(args, "upload-dir");
        const char *raw = f ? f : (ms ? ms->ns_upload_dir : NULL);
        derived.ns_upload_dir = anchor_serve_path(raw, project_root_abs, false);
        cfg.ns.upload_dir = derived.ns_upload_dir
                          ? derived.ns_upload_dir : raw;
    }
    {
        const char *f = ph_args_get_flag(args, "augments-dir");
        const char *raw = f ? f : (ms ? ms->ns_augments_dir : NULL);
        derived.ns_augments_dir = anchor_serve_path(raw, project_root_abs, false);
        cfg.ns.augments_dir = derived.ns_augments_dir
                            ? derived.ns_augments_dir : raw;
    }
    {
        const char *f = ph_args_get_flag(args, "grafts-dir");
        const char *raw = f ? f : (ms ? ms->ns_grafts_dir : NULL);
        derived.ns_grafts_dir = anchor_serve_path(raw, project_root_abs, false);
        cfg.ns.grafts_dir = derived.ns_grafts_dir
                          ? derived.ns_grafts_dir : raw;
    }

    /* neonsignal logging flags -- tier 1 (flag) > tier 2 (manifest) */
    cfg.ns.enable_debug = ph_args_has_flag(args, "enable-debug")
                        || (ms && ms->ns_enable_debug);
    cfg.ns.enable_log = ph_args_has_flag(args, "enable-log")
                      || (ms && ms->ns_enable_log);
    cfg.ns.enable_log_color = ph_args_has_flag(args, "enable-log-color")
                            || (ms && ms->ns_enable_log_color);
    cfg.ns.enable_file_log = ph_args_has_flag(args, "enable-file-log")
                           || (ms && ms->ns_enable_file_log);
    /* audit fix: log_directory is mkdir'd directly by phosphor BEFORE any
     * child chdir, so a relative value resolved against the caller cwd
     * creates the log dir in the wrong place. */
    {
        const char *f = ph_args_get_flag(args, "log-directory");
        const char *raw = f ? f : (ms ? ms->ns_log_directory : NULL);
        derived.ns_log_directory = anchor_serve_path(raw, project_root_abs, false);
        cfg.ns.log_directory = derived.ns_log_directory
                             ? derived.ns_log_directory : raw;
    }
    cfg.ns.disable_proxies_check = ph_args_has_flag(args, "disable-proxies-check")
                                 || (ms && ms->ns_disable_proxies_check);

    /* watch config -- tier 1 (flag) > tier 2 (manifest [serve]) */
    cfg.ns.watch = watch_flag || (ms && ms->ns_watch);
    cfg.ns.watch_cmd = watch_cmd_flag ? watch_cmd_flag
                     : (ms ? ms->ns_watch_cmd : NULL);
    cfg.ns.deploy_dir = (has_manifest && manifest.deploy.present
                         && manifest.deploy.public_dir)
                        ? manifest.deploy.public_dir : NULL;

    /* redirect config -- tier 1 (flag) > tier 2 (manifest [serve]).
     * audit fix: anchor relative redir.bin_path under project_root_abs.
     * see ns.bin_path above for the bare-name carve-out rationale. */
    {
        const char *raw = redir_bin ? redir_bin
                                    : (ms ? ms->redir_bin : NULL);
        derived.redir_bin = anchor_serve_path(raw, project_root_abs, true);
        cfg.redir.bin_path = derived.redir_bin ? derived.redir_bin : raw;
    }

    {
        int t1 = flag_int(args, "redirect-instances");
        cfg.redir.instances = t1 > 0 ? t1
                            : (ms ? ms->redir_instances : 0);
    }
    {
        const char *f = ph_args_get_flag(args, "redirect-host");
        cfg.redir.host = f ? f : (ms ? ms->redir_host : NULL);
    }
    {
        int t1 = flag_int(args, "redirect-port");
        cfg.redir.port = t1 > 0 ? t1
                       : (ms ? ms->redir_port : 0);
    }
    {
        int t1 = flag_int(args, "redirect-target-port");
        cfg.redir.target_port = t1 > 0 ? t1
                              : (ms ? ms->redir_target_port : 0);
    }
    /* audit fix: redirect.acme_webroot is a path that may come from the
     * manifest as a relative value. Anchor it under project_root_abs so the
     * child resolves the same directory regardless of phosphor's cwd. */
    {
        const char *f = ph_args_get_flag(args, "redirect-acme-webroot");
        const char *raw = f ? f
                            : (ms ? ms->redir_acme_webroot : NULL);
        derived.redir_acme_webroot =
            anchor_serve_path(raw, project_root_abs, false);
        cfg.redir.acme_webroot = derived.redir_acme_webroot
                               ? derived.redir_acme_webroot : raw;
    }
    {
        const char *f = ph_args_get_flag(args, "redirect-working-dir");
        const char *raw = f ? f : (ms ? ms->redir_working_dir : NULL);
        derived.redir_working_dir =
            anchor_serve_path(raw, project_root_abs, false);
        cfg.redir.working_dir = derived.redir_working_dir
                              ? derived.redir_working_dir : raw;
    }

    /* if redirect target port not set, default to neonsignal port */
    if (cfg.redir.target_port == 0 && cfg.ns.port > 0)
        cfg.redir.target_port = cfg.ns.port;

    /* step 5b: containment enforcement. Every serve path that feeds a
     * child's cwd/serve-root/write-root must stay under project_root_abs
     * so a buggy or malicious manifest cannot redirect writes, chdirs,
     * or served content outside the project tree. bin_path fields are
     * intentionally excluded -- bare names are PATH-resolved downstream
     * and absolute paths to binaries are allowed by design. */
    {
        int vrc = 0;
        vrc |= serve_validate_under_root("www-root",
                cfg.ns.www_root, project_root_abs);
        vrc |= serve_validate_under_root("certs-root",
                cfg.ns.certs_root, project_root_abs);
        vrc |= serve_validate_under_root("working-dir",
                cfg.ns.working_dir, project_root_abs);
        vrc |= serve_validate_under_root("upload-dir",
                cfg.ns.upload_dir, project_root_abs);
        vrc |= serve_validate_under_root("augments-dir",
                cfg.ns.augments_dir, project_root_abs);
        vrc |= serve_validate_under_root("grafts-dir",
                cfg.ns.grafts_dir, project_root_abs);
        vrc |= serve_validate_under_root("log-directory",
                cfg.ns.log_directory, project_root_abs);
        vrc |= serve_validate_under_root("redirect-acme-webroot",
                cfg.redir.acme_webroot, project_root_abs);
        vrc |= serve_validate_under_root("redirect-working-dir",
                cfg.redir.working_dir, project_root_abs);
        if (vrc != 0) {
            serve_derived_paths_free(&derived);
            if (has_certs) ph_certs_config_destroy(&certs_cfg);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return PH_ERR_VALIDATE;
        }
    }

    /* audit fix: default child working_dir to the resolved project root.
     * previously, when no flag or manifest setting was given, children
     * inherited phosphor's cwd, which drifts if serve was launched from a
     * subdirectory or via `cd ...; phosphor serve`. explicit flag or
     * manifest values still win. */
    if (!cfg.ns.working_dir)
        cfg.ns.working_dir = project_root_abs;
    if (!cfg.redir.working_dir)
        cfg.redir.working_dir = project_root_abs;

    /* step 6: validate hosts and warn on privileged ports */
    if (cfg.ns.host && !is_valid_ip(cfg.ns.host)) {
        ph_log_error("serve: invalid neonsignal host address: %s", cfg.ns.host);
        serve_derived_paths_free(&derived);
        if (has_certs) ph_certs_config_destroy(&certs_cfg);
        if (has_manifest) ph_manifest_destroy(&manifest);
        ph_free(project_root_abs);
        return PH_ERR_CONFIG;
    }
    if (cfg.redir.host && !is_valid_ip(cfg.redir.host)) {
        ph_log_error("serve: invalid redirect host address: %s", cfg.redir.host);
        serve_derived_paths_free(&derived);
        if (has_certs) ph_certs_config_destroy(&certs_cfg);
        if (has_manifest) ph_manifest_destroy(&manifest);
        ph_free(project_root_abs);
        return PH_ERR_CONFIG;
    }

    warn_privileged_port("neonsignal", cfg.ns.port);
    warn_privileged_port("redirect", cfg.redir.port);

    /* step 7: check binaries exist */
    {
        ph_error_t *berr = NULL;
        if (ph_serve_check_binaries(&cfg, &berr) != PH_OK) {
            ph_log_error("serve: %s", berr ? berr->message : "binary check failed");
            ph_error_destroy(berr);
            serve_derived_paths_free(&derived);
            if (has_certs) ph_certs_config_destroy(&certs_cfg);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return PH_ERR_CONFIG;
        }
    }

    /* step 7b: ensure log directories exist before spawning neonsignal.
     *
     * audit fix (2026-04-08T11-07-17Z): previously this step used raw
     * mkdir() and discarded every return value, so a nested
     * log_directory like "var/log/phosphor" silently failed when
     * "var/log" did not already exist -- neonsignal would then spawn
     * with --enable-file-log configured but no log tree. Switch to
     * recursive ph_fs_mkdir_p and fail-closed so users get a clear
     * diagnostic instead of a broken file-log configuration.
     *
     * ph_fs_mkdir_p silences EEXIST internally, so pre-existing
     * directories remain a no-op. */
    if (cfg.ns.log_directory) {
        if (ph_fs_mkdir_p(cfg.ns.log_directory, 0755) != PH_OK) {
            ph_log_error("serve: cannot create log directory '%s'",
                         cfg.ns.log_directory);
            serve_derived_paths_free(&derived);
            if (has_certs) ph_certs_config_destroy(&certs_cfg);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return PH_ERR_FS;
        }

        char subdir[512];
        snprintf(subdir, sizeof(subdir), "%s/debug", cfg.ns.log_directory);
        if (ph_fs_mkdir_p(subdir, 0755) != PH_OK) {
            ph_log_error("serve: cannot create log subdirectory '%s'",
                         subdir);
            serve_derived_paths_free(&derived);
            if (has_certs) ph_certs_config_destroy(&certs_cfg);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return PH_ERR_FS;
        }

        snprintf(subdir, sizeof(subdir), "%s/shell", cfg.ns.log_directory);
        if (ph_fs_mkdir_p(subdir, 0755) != PH_OK) {
            ph_log_error("serve: cannot create log subdirectory '%s'",
                         subdir);
            serve_derived_paths_free(&derived);
            if (has_certs) ph_certs_config_destroy(&certs_cfg);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return PH_ERR_FS;
        }
    }

    /* step 8: determine dashboard mode */
    bool use_dashboard = false;
#ifdef PHOSPHOR_HAS_NCURSES
    use_dashboard = !no_dashboard;
#else
    (void)no_dashboard;
#endif

    /* capture child output when using the dashboard */
    cfg.capture_output = use_dashboard;

    /* step 8b: compute info values (used by banner and dashboard) */
    int ns_port_val = cfg.ns.port > 0 ? cfg.ns.port : 9443;
    const char *ns_bind_val = cfg.ns.host ? cfg.ns.host : "0.0.0.0";
    const char *domain_val = NULL;
    if (has_certs && certs_cfg.domain_count > 0 && certs_cfg.domains[0].name)
        domain_val = certs_cfg.domains[0].name;
    const char *url_host = domain_val ? domain_val : ns_bind_val;

    char url_buf[256];
    snprintf(url_buf, sizeof(url_buf), "https://%s", url_host);
    char port_buf[32];
    snprintf(port_buf, sizeof(port_buf), "%d", ns_port_val);

    int rd_port_val = cfg.redir.port > 0 ? cfg.redir.port : 9090;
    int rd_target_val = cfg.redir.target_port > 0
                      ? cfg.redir.target_port : ns_port_val;
    char http_buf[256];
    snprintf(http_buf, sizeof(http_buf), "http://%s:%d",
             ns_bind_val, rd_port_val);
    char redir_buf[64];
    snprintf(redir_buf, sizeof(redir_buf), "-> :%d", rd_target_val);

    /* step 8c: print startup banner (skip if dashboard will show it) */
    if (!use_dashboard) {
        fprintf(stdout, "\n");
        ph_term_kv_link(stdout, 2, 10, "https", url_buf, url_buf, PH_FG_CYAN);
        ph_term_kv(stdout, 2, 10, "bind", ns_bind_val, PH_FG_GREEN);
        ph_term_kvf(stdout, 2, 10, "port", PH_FG_GREEN, "%d", ns_port_val);
        if (!cfg.skip_redirect) {
            ph_term_kv_link(stdout, 2, 10, "http", http_buf, http_buf,
                            PH_FG_GREEN);
            ph_term_kvf(stdout, 2, 10, "", PH_DIM, "-> :%d", rd_target_val);
        }
        if (cfg.ns.watch)
            ph_term_kv(stdout, 2, 10, "watch", "on", PH_FG_YELLOW);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    /* step 9: start */
    ph_serve_session_t *session = NULL;
    {
        ph_error_t *serr = NULL;
        if (ph_serve_start(&cfg, &session, &serr) != PH_OK) {
            ph_log_error("serve: %s",
                          serr ? serr->message : "start failed");
            ph_error_destroy(serr);
            serve_derived_paths_free(&derived);
            if (has_certs) ph_certs_config_destroy(&certs_cfg);
            if (has_manifest) ph_manifest_destroy(&manifest);
            ph_free(project_root_abs);
            return PH_ERR_INTERNAL;
        }
    }

    /* step 10: wait (blocks until Ctrl+C or child exit) */
    int exit_code;

#ifdef PHOSPHOR_HAS_NCURSES
    if (use_dashboard) {
        /* build dashboard config from session */
        ph_dashboard_config_t dcfg;
        memset(&dcfg, 0, sizeof(dcfg));
        dcfg.status_text = NULL;  /* symbol rendered directly by status bar */

        /* info box lines */
        int ii = 0;
        dcfg.info_lines[ii].label = "https";
        dcfg.info_lines[ii].value = url_buf;
        dcfg.info_lines[ii].color = PH_INFO_CYAN;
        ii++;

        dcfg.info_lines[ii].label = "bind";
        dcfg.info_lines[ii].value = ns_bind_val;
        dcfg.info_lines[ii].color = PH_INFO_GREEN;
        ii++;

        dcfg.info_lines[ii].label = "port";
        dcfg.info_lines[ii].value = port_buf;
        dcfg.info_lines[ii].color = PH_INFO_GREEN;
        ii++;

        if (!cfg.skip_redirect) {
            dcfg.info_lines[ii].label = "http";
            dcfg.info_lines[ii].value = http_buf;
            dcfg.info_lines[ii].color = PH_INFO_GREEN;
            ii++;

            dcfg.info_lines[ii].label = "";
            dcfg.info_lines[ii].value = redir_buf;
            dcfg.info_lines[ii].color = PH_INFO_DIM;
            ii++;
        }

        if (cfg.ns.watch) {
            dcfg.info_lines[ii].label = "watch";
            dcfg.info_lines[ii].value = "on";
            dcfg.info_lines[ii].color = PH_INFO_YELLOW;
            ii++;
        }
        dcfg.info_count = ii;

        int pi = 0;
        dcfg.panels[pi].name = "neonsignal";
        dcfg.panels[pi].id = PH_PANEL_NEONSIGNAL;
        dcfg.panels[pi].stdout_fd = ph_serve_ns_stdout_fd(session);
        dcfg.panels[pi].stderr_fd = ph_serve_ns_stderr_fd(session);
        dcfg.panels[pi].pid = ph_serve_ns_pid(session);
        dcfg.panels[pi].tab_count = 2;
        dcfg.panels[pi].tabs[0].name = "live-stream";
        dcfg.panels[pi].tabs[0].source_stream = 0; /* stdout */
        dcfg.panels[pi].tabs[1].name = "debug-stream";
        dcfg.panels[pi].tabs[1].source_stream = 1; /* stderr */
        pi++;

        if (!cfg.skip_redirect) {
            dcfg.panels[pi].name = "redirect";
            dcfg.panels[pi].id = PH_PANEL_REDIRECT;
            dcfg.panels[pi].stdout_fd = ph_serve_redir_stdout_fd(session);
            dcfg.panels[pi].stderr_fd = ph_serve_redir_stderr_fd(session);
            dcfg.panels[pi].pid = ph_serve_redir_pid(session);
            pi++;
        }

        if (cfg.ns.watch) {
            dcfg.panels[pi].name = "watcher";
            dcfg.panels[pi].id = PH_PANEL_WATCHER;
            dcfg.panels[pi].stdout_fd = ph_serve_watch_stdout_fd(session);
            dcfg.panels[pi].stderr_fd = ph_serve_watch_stderr_fd(session);
            dcfg.panels[pi].pid = ph_serve_watch_pid(session);
            pi++;
        }

        dcfg.panel_count = pi;

        /* pass spawn config so the dashboard can start/stop */
        dcfg.serve_cfg = &cfg;
        dcfg.session_ptr = &session;

        /* build fuzzy exclude list: .gitignore + [fuzzy].exclude */
        char *fz_owned[128];   /* heap copies freed after dashboard */
        const char *fz_excludes[128];
        int fz_count = 0;
        int fz_owned_count = 0;

        /* parse .gitignore (one pattern per line, skip comments/blanks).
         * audit fix: read from project_root_abs, not the caller's cwd, so
         * `phosphor serve --project /elsewhere` picks up the right file. */
        {
            char *gi_path = ph_path_join(project_root_abs, ".gitignore");
            FILE *gi = gi_path ? fopen(gi_path, "r") : NULL;
            ph_free(gi_path);
            if (gi) {
                char line[256];
                while (fgets(line, (int)sizeof(line), gi) && fz_count < 120) {
                    int len = (int)strlen(line);
                    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                           || line[len-1] == ' ')) line[--len] = '\0';
                    if (len == 0 || line[0] == '#') continue;
                    if (len > 0 && line[len-1] == '/') line[--len] = '\0';
                    if (len == 0) continue;
                    char *copy = ph_alloc((size_t)len + 1);
                    if (copy) {
                        memcpy(copy, line, (size_t)len + 1);
                        fz_owned[fz_owned_count++] = copy;
                        fz_excludes[fz_count++] = copy;
                    }
                }
                fclose(gi);
            }
        }

        /* append manifest [fuzzy].exclude */
        if (has_manifest && manifest.fuzzy.present) {
            for (size_t i = 0; i < manifest.fuzzy.exclude_count && fz_count < 128; i++)
                fz_excludes[fz_count++] = manifest.fuzzy.exclude[i];
        }

        dcfg.fuzzy_excludes = fz_count > 0 ? fz_excludes : NULL;
        dcfg.fuzzy_exclude_count = fz_count;

        ph_dashboard_t *db = NULL;
        if (ph_dashboard_create(&dcfg, &db) == PH_OK) {
            exit_code = ph_dashboard_run(db);
            ph_dashboard_destroy(db);
        } else {
            ph_log_error("serve: failed to create dashboard, "
                          "falling back to raw mode");
            exit_code = ph_serve_wait(session);
        }

        /* free .gitignore copies */
        for (int i = 0; i < fz_owned_count; i++)
            ph_free(fz_owned[i]);
    } else
#endif
    {
        exit_code = ph_serve_wait(session);
    }

    /* step 11: cleanup */
    ph_serve_destroy(session);
    serve_derived_paths_free(&derived);
    if (has_certs) ph_certs_config_destroy(&certs_cfg);
    if (has_manifest) ph_manifest_destroy(&manifest);
    ph_free(project_root_abs);

    return exit_code;
}

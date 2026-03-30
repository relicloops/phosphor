#include "phosphor/alloc.h"
#include "phosphor/args.h"
#include "phosphor/certs.h"
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

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- status indicators ---- */

static const char *S_OK   = "[ok]";
static const char *S_WARN = "[warn]";
static const char *S_FAIL = "[FAIL]";

/* ---- check: manifest ---- */

static int check_manifest(const char *project_root) {
    char *path1 = ph_path_join(project_root, "template.phosphor.toml");
    char *path2 = ph_path_join(project_root, "phosphor.toml");

    ph_fs_stat_t st;
    if (path1 && ph_fs_stat(path1, &st) == PH_OK && st.is_file) {
        ph_log_info("%s manifest: template.phosphor.toml found", S_OK);
        ph_free(path1);
        ph_free(path2);
        return 0;
    }
    if (path2 && ph_fs_stat(path2, &st) == PH_OK && st.is_file) {
        ph_log_info("%s manifest: phosphor.toml found", S_OK);
        ph_free(path1);
        ph_free(path2);
        return 0;
    }

    ph_log_info("%s manifest: no phosphor manifest found", S_WARN);
    ph_free(path1);
    ph_free(path2);
    return 1;
}

/* ---- check: tool on PATH via `which` ---- */

static int check_tool(const char *name) {
    /* use sh -c to suppress stdout from which */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", name);

    ph_argv_builder_t b;
    if (ph_argv_init(&b, 4) != PH_OK) return -1;
    ph_argv_push(&b, "sh");
    ph_argv_push(&b, "-c");
    ph_argv_push(&b, cmd);
    char **argv = ph_argv_finalize(&b);
    if (!argv) return -1;

    ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
    int exit_code = 0;
    ph_result_t rc = ph_proc_exec(&opts, &exit_code);
    ph_argv_free(argv);

    if (rc != PH_OK || exit_code != 0) {
        ph_log_info("%s tools: %s not found", S_WARN, name);
        return 1;
    }

    ph_log_info("%s tools: %s found", S_OK, name);
    return 0;
}

/* ---- check: node deps ---- */

static int check_node(const char *project_root) {
    int warnings = 0;

    char *pkg = ph_path_join(project_root, "package.json");
    ph_fs_stat_t st;
    if (pkg && ph_fs_stat(pkg, &st) == PH_OK && st.is_file) {
        ph_log_info("%s node: package.json found", S_OK);
    } else {
        ph_log_info("%s node: package.json not found", S_WARN);
        warnings++;
    }
    ph_free(pkg);

    char *nm = ph_path_join(project_root, "node_modules");
    if (nm && ph_fs_stat(nm, &st) == PH_OK && st.is_dir) {
        ph_log_info("%s node: node_modules/ present", S_OK);
    } else {
        ph_log_info("%s node: node_modules/ missing (run: npm install)",
                     S_WARN);
        warnings++;
    }
    ph_free(nm);

    return warnings;
}

/* ---- check: build state ---- */

static int check_build(const char *project_root) {
    char *bd = ph_path_join(project_root, "build");
    ph_fs_stat_t st;
    if (bd && ph_fs_stat(bd, &st) == PH_OK && st.is_dir) {
        ph_log_info("%s build: build/ directory present", S_OK);
    } else {
        ph_log_info("%s build: build/ directory missing (run: phosphor build)",
                     S_WARN);
        ph_free(bd);
        return 1;
    }
    ph_free(bd);
    return 0;
}

/* ---- check: stale staging dirs ---- */

static int check_stale(const char *project_root) {
    static const char prefix[] = ".phosphor-staging-";
    DIR *d = opendir(project_root);
    if (!d) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, prefix, sizeof(prefix) - 1) == 0)
            count++;
    }
    closedir(d);

    if (count > 0) {
        ph_log_info("%s staging: %d stale .phosphor-staging-* dir(s) "
                     "(run: phosphor clean --stale)", S_WARN, count);
        return count;
    }

    ph_log_info("%s staging: no stale directories", S_OK);
    return 0;
}

/* ---- check: cert files + expiry ---- */

static void check_cert_file(const char *cert_path, const char *label) {
    ph_fs_stat_t st;
    if (ph_fs_stat(cert_path, &st) != PH_OK || !st.is_file) {
        ph_log_info("%s certs: %s not found", S_WARN, label);
        return;
    }

    /* use openssl to check expiry */
    ph_argv_builder_t b;
    if (ph_argv_init(&b, 8) != PH_OK) {
        ph_log_info("%s certs: %s exists (cannot check expiry)", S_WARN, label);
        return;
    }
    ph_argv_push(&b, "openssl");
    ph_argv_push(&b, "x509");
    ph_argv_push(&b, "-checkend");
    ph_argv_push(&b, "2592000"); /* 30 days in seconds */
    ph_argv_push(&b, "-noout");
    ph_argv_push(&b, "-in");
    ph_argv_push(&b, cert_path);
    char **argv = ph_argv_finalize(&b);
    if (!argv) {
        ph_log_info("%s certs: %s exists (cannot check expiry)", S_WARN, label);
        return;
    }

    ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
    int exit_code = 0;
    ph_result_t rc = ph_proc_exec(&opts, &exit_code);
    ph_argv_free(argv);

    if (rc != PH_OK) {
        ph_log_info("%s certs: %s exists (openssl error)", S_WARN, label);
    } else if (exit_code == 0) {
        ph_log_info("%s certs: %s valid (> 30 days remaining)", S_OK, label);
    } else {
        ph_log_info("%s certs: %s expiring or expired (< 30 days)", S_FAIL,
                     label);
    }
}

static void check_certs(const char *project_root) {
    /* try loading certs config -- only if manifest file exists */
    char *toml_path = ph_path_join(project_root, "template.phosphor.toml");
    if (!toml_path) return;

    ph_fs_stat_t toml_st;
    if (ph_fs_stat(toml_path, &toml_st) != PH_OK || !toml_st.is_file) {
        ph_free(toml_path);
        return;
    }

    ph_error_t *err = NULL;
    ph_certs_config_t cfg;
    if (ph_certs_config_parse(toml_path, &cfg, &err) != PH_OK) {
        ph_error_destroy(err);
        ph_free(toml_path);
        return;
    }
    ph_free(toml_path);

    if (!cfg.present) {
        ph_certs_config_destroy(&cfg);
        return;
    }

    /* check CA */
    char *ca_dir = ph_path_join(project_root, cfg.output_dir);
    if (ca_dir) {
        char *ca_cert = ph_path_join(ca_dir, "ca/root.crt");
        if (ca_cert) {
            check_cert_file(ca_cert, "ca/root.crt");
            ph_free(ca_cert);
        }
        ph_free(ca_dir);
    }

    /* check domain certs */
    for (size_t i = 0; i < cfg.domain_count; i++) {
        const ph_cert_domain_t *d = &cfg.domains[i];
        const char *dir_name = d->dir_name ? d->dir_name : d->name;

        char *base = ph_path_join(project_root, cfg.output_dir);
        if (!base) continue;
        char *dom_dir = ph_path_join(base, dir_name);
        ph_free(base);
        if (!dom_dir) continue;

        char *fc = ph_path_join(dom_dir, "fullchain.pem");
        if (fc) {
            check_cert_file(fc, dir_name);
            ph_free(fc);
        }
        ph_free(dom_dir);
    }

    ph_certs_config_destroy(&cfg);
}

/* ---- main doctor pipeline ---- */

int ph_cmd_doctor(const ph_cli_config_t *config,
                  const ph_parsed_args_t *args) {
    (void)config;

    const char *project_val = ph_args_get_flag(args, "project");
    bool verbose = ph_args_has_flag(args, "verbose");

    if (verbose)
        ph_log_set_level(PH_LOG_DEBUG);

    /* resolve project root */
    char *project_root_abs = NULL;

    if (project_val) {
        if (ph_path_is_absolute(project_val)) {
            project_root_abs = ph_path_normalize(project_val);
        } else {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) {
                ph_log_error("doctor: failed to get current directory");
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
            ph_log_error("doctor: failed to get current directory");
            return PH_ERR_INTERNAL;
        }
        project_root_abs = ph_path_normalize(cwd);
    }

    if (!project_root_abs) {
        ph_log_error("doctor: failed to resolve project root");
        return PH_ERR_INTERNAL;
    }

    /* verify project root exists */
    ph_fs_stat_t root_st;
    if (ph_fs_stat(project_root_abs, &root_st) != PH_OK ||
        !root_st.exists || !root_st.is_dir) {
        ph_log_error("doctor: project root does not exist: %s",
                      project_root_abs);
        ph_free(project_root_abs);
        return PH_ERR_VALIDATE;
    }

    int warnings = 0;

    /* 1. manifest */
    warnings += check_manifest(project_root_abs);

    /* 2. tools */
    warnings += check_tool("openssl");
    check_tool("esbuild");
    check_tool("neonsignal");
    check_tool("neonsignal_redirect");

    /* 3. node deps */
    warnings += check_node(project_root_abs);

    /* 4. build state */
    warnings += check_build(project_root_abs);

    /* 5. stale staging */
    warnings += check_stale(project_root_abs);

    /* 6. cert status */
    check_certs(project_root_abs);

    if (warnings > 0)
        ph_log_info("doctor: %d warning(s) found", warnings);
    else
        ph_log_info("doctor: all checks passed");

    ph_free(project_root_abs);
    return 0;
}

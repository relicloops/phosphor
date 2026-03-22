#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/proc.h"
#include "phosphor/signal.h"

#include <stdio.h>
#include <string.h>

/* ---- openssl preflight ---- */

static ph_result_t check_openssl(ph_error_t **err) {
    ph_argv_builder_t b;
    if (ph_argv_init(&b, 4) != PH_OK) return PH_ERR;
    ph_argv_push(&b, "openssl");
    ph_argv_push(&b, "version");
    char **argv = ph_argv_finalize(&b);
    if (!argv) return PH_ERR;

    ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
    int exit_code = 0;
    ph_result_t rc = ph_proc_exec(&opts, &exit_code);
    ph_argv_free(argv);

    if (rc != PH_OK || exit_code != 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "openssl not found or not working; "
                "install openssl CLI to use cert generation");
        return PH_ERR;
    }
    return PH_OK;
}

/* ---- CA generation ---- */

ph_result_t ph_certs_gen_ca(const ph_certs_config_t *config,
                             const char *project_root,
                             bool dry_run, bool force,
                             ph_error_t **err) {
    if (!config || !project_root) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_certs_gen_ca: NULL argument");
        return PH_ERR;
    }

    /* build paths */
    char *certs_dir = ph_path_join(project_root, config->output_dir);
    if (!certs_dir) return PH_ERR;
    char *ca_dir = ph_path_join(certs_dir, "ca");
    ph_free(certs_dir);
    if (!ca_dir) return PH_ERR;

    char *key_path = ph_path_join(ca_dir, "root.key");
    char *crt_path = ph_path_join(ca_dir, "root.crt");
    if (!key_path || !crt_path) {
        ph_free(ca_dir);
        ph_free(key_path);
        ph_free(crt_path);
        return PH_ERR;
    }

    if (dry_run) {
        ph_log_info("dry-run: would generate root CA");
        ph_log_info("  key: %s (%d bits)", key_path, config->ca_bits);
        ph_log_info("  crt: %s (CN=%s, %d days)",
                     crt_path, config->ca_cn, config->ca_days);
        ph_free(ca_dir);
        ph_free(key_path);
        ph_free(crt_path);
        return PH_OK;
    }

    /* check if CA already exists */
    if (!force) {
        ph_fs_stat_t st;
        if (ph_fs_stat(crt_path, &st) == PH_OK && st.is_file) {
            ph_log_warn("certs: %s already exists (use --force to overwrite)",
                        crt_path);
            ph_free(ca_dir);
            ph_free(key_path);
            ph_free(crt_path);
            return PH_OK;
        }
    }

    /* check openssl is available */
    if (check_openssl(err) != PH_OK) {
        ph_free(ca_dir);
        ph_free(key_path);
        ph_free(crt_path);
        return PH_ERR;
    }

    /* create output directory */
    if (ph_fs_mkdir_p(ca_dir, 0755) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create CA directory: %s", ca_dir);
        ph_free(ca_dir);
        ph_free(key_path);
        ph_free(crt_path);
        return PH_ERR;
    }

    /* step 1: openssl genrsa -out root.key <bits> */
    {
        ph_argv_builder_t b;
        if (ph_argv_init(&b, 8) != PH_OK) goto fail;
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "genrsa");
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, key_path);
        ph_argv_pushf(&b, "%d", config->ca_bits);
        char **argv = ph_argv_finalize(&b);
        if (!argv) goto fail;

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl genrsa failed (exit %d)", exit_code);
            goto fail;
        }
    }

    if (ph_signal_interrupted()) goto fail_signal;

    /* set key permissions: 600 */
    ph_fs_chmod(key_path, 0600);

    /* step 2: openssl req -x509 -new -nodes ... */
    {
        char subj[256];
        snprintf(subj, sizeof(subj), "/CN=%s", config->ca_cn);

        ph_argv_builder_t b;
        if (ph_argv_init(&b, 16) != PH_OK) goto fail;
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "req");
        ph_argv_push(&b, "-x509");
        ph_argv_push(&b, "-new");
        ph_argv_push(&b, "-nodes");
        ph_argv_push(&b, "-key");
        ph_argv_push(&b, key_path);
        ph_argv_push(&b, "-sha256");
        ph_argv_push(&b, "-days");
        ph_argv_pushf(&b, "%d", config->ca_days);
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, crt_path);
        ph_argv_push(&b, "-subj");
        ph_argv_push(&b, subj);
        ph_argv_push(&b, "-config");
        ph_argv_push(&b, "/dev/null");
        char **argv = ph_argv_finalize(&b);
        if (!argv) goto fail;

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl req (CA cert) failed (exit %d)", exit_code);
            goto fail;
        }
    }

    ph_log_info("certs: CA key -> %s", key_path);
    ph_log_info("certs: CA crt -> %s", crt_path);
    ph_log_info("");
    ph_log_info("certs: to trust this CA system-wide, run:");
#if defined(PH_PLATFORM_MACOS)
    ph_log_info("  sudo security add-trusted-cert -d -r trustRoot "
                "-k /Library/Keychains/System.keychain %s", crt_path);
#elif defined(PH_PLATFORM_LINUX)
    ph_log_info("  # Debian/Ubuntu:");
    ph_log_info("  sudo cp %s /usr/local/share/ca-certificates/phosphor-ca.crt",
                crt_path);
    ph_log_info("  sudo update-ca-certificates");
    ph_log_info("  # Fedora/RHEL:");
    ph_log_info("  sudo cp %s /etc/pki/ca-trust/source/anchors/phosphor-ca.crt",
                crt_path);
    ph_log_info("  sudo update-ca-trust");
#endif

    ph_free(ca_dir);
    ph_free(key_path);
    ph_free(crt_path);
    return PH_OK;

fail_signal:
    if (err)
        *err = ph_error_createf(PH_ERR_SIGNAL, 0, "interrupted");
fail:
    ph_free(ca_dir);
    ph_free(key_path);
    ph_free(crt_path);
    return PH_ERR;
}

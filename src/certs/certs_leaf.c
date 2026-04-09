#include "phosphor/certs.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/proc.h"
#include "phosphor/signal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- leaf cert generation ---- */

ph_result_t ph_certs_gen_leaf(const ph_certs_config_t *config,
                               const ph_cert_domain_t *domain,
                               const char *project_root,
                               bool dry_run, bool force,
                               ph_error_t **err) {
    if (!config || !domain || !project_root) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_certs_gen_leaf: NULL argument");
        return PH_ERR;
    }

    /* build paths */
    char *certs_dir = ph_path_join(project_root, config->output_dir);
    if (!certs_dir) return PH_ERR;

    const char *dir_name = domain->dir_name ? domain->dir_name : domain->name;
    char *leaf_dir = ph_path_join(certs_dir, dir_name);
    if (!leaf_dir) { ph_free(certs_dir); return PH_ERR; }

    char *ca_dir = ph_path_join(certs_dir, "ca");
    ph_free(certs_dir);
    if (!ca_dir) { ph_free(leaf_dir); return PH_ERR; }

    /* audit fix: belt-and-braces containment re-check for leaf_dir. */
    if (!ph_path_is_under(leaf_dir, project_root)) {
        if (err)
            *err = ph_error_createf(PH_ERR_VALIDATE, 0,
                "certs: resolved leaf directory escapes project root: %s",
                leaf_dir);
        ph_free(leaf_dir);
        ph_free(ca_dir);
        return PH_ERR;
    }

    char *ca_key = ph_path_join(ca_dir, "root.key");
    char *ca_crt = ph_path_join(ca_dir, "root.crt");
    char *privkey_path = ph_path_join(leaf_dir, "privkey.pem");
    char *fullchain_path = ph_path_join(leaf_dir, "fullchain.pem");
    char *csr_path = ph_path_join(leaf_dir, "leaf.csr");
    char *cnf_path = ph_path_join(leaf_dir, "san.cnf");

    /* audit fix (2026-04-08T11-07-17Z): effective SAN list always
     * has domain->name at index 0 followed by unique entries from
     * domain->san. Declared here so every goto label can free it
     * unconditionally, and built before dry-run so logging mirrors
     * the real pipeline. */
    char **eff_sans = NULL;
    size_t eff_count = 0;

    if (!ca_key || !ca_crt || !privkey_path || !fullchain_path ||
        !csr_path || !cnf_path) {
        goto fail_alloc;
    }

    if (ph_cert_domain_effective_sans(domain, &eff_sans, &eff_count,
                                        err) != PH_OK) {
        goto fail_alloc;
    }

    if (dry_run) {
        ph_log_info("dry-run: would generate leaf cert for %s", domain->name);
        ph_log_info("  key: %s (%d bits)", privkey_path, config->leaf_bits);
        ph_log_info("  cert: %s (signed by %s, %d days)",
                     fullchain_path, ca_crt, config->leaf_days);
        ph_log_info("  SANs: %zu", eff_count);
        for (size_t i = 0; i < eff_count; i++)
            ph_log_info("    %s", eff_sans[i]);
        goto success;
    }

    /* check if cert already exists */
    if (!force) {
        ph_fs_stat_t st;
        if (ph_fs_stat(fullchain_path, &st) == PH_OK && st.is_file) {
            ph_log_warn("certs: %s already exists (use --force to overwrite)",
                        fullchain_path);
            goto success;
        }
    }

    /* verify CA files exist */
    {
        ph_fs_stat_t st;
        if (ph_fs_stat(ca_key, &st) != PH_OK || !st.is_file) {
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                    "CA key not found: %s (run --local --ca-only first)",
                    ca_key);
            goto fail;
        }
        if (ph_fs_stat(ca_crt, &st) != PH_OK || !st.is_file) {
            if (err)
                *err = ph_error_createf(PH_ERR_FS, 0,
                    "CA cert not found: %s (run --local --ca-only first)",
                    ca_crt);
            goto fail;
        }
    }

    /* create output directory */
    if (ph_fs_mkdir_p(leaf_dir, 0755) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create directory: %s", leaf_dir);
        goto fail;
    }

    /* step 1: openssl genrsa -out privkey.pem <bits> */
    {
        ph_argv_builder_t b;
        if (ph_argv_init(&b, 8) != PH_OK) goto fail;
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "genrsa");
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, privkey_path);
        ph_argv_pushf(&b, "%d", config->leaf_bits);
        char **argv = ph_argv_finalize(&b);
        if (!argv) goto fail;

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl genrsa (leaf) failed (exit %d)", exit_code);
            goto fail;
        }
    }

    /* set key permissions: 600 */
    if (ph_fs_chmod(privkey_path, 0600) != PH_OK) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot set permissions on leaf key: %s", privkey_path);
        goto fail;
    }

    if (ph_signal_interrupted()) goto fail_signal;

    /* step 2: write SAN config file (always; eff_sans is guaranteed
     * to contain at least domain->name at index 0, so
     * ph_cert_san_write_cnf never sees count == 0). */
    if (ph_cert_san_write_cnf(cnf_path,
                                (const char *const *)eff_sans,
                                eff_count, err) != PH_OK) {
        goto fail;
    }

    if (ph_signal_interrupted()) goto fail_signal;

    /* step 3: openssl req -new -key ... -out leaf.csr -config san.cnf */
    {
        char subj[256];
        snprintf(subj, sizeof(subj), "/CN=%s", domain->name);

        ph_argv_builder_t b;
        if (ph_argv_init(&b, 16) != PH_OK) goto fail;
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "req");
        ph_argv_push(&b, "-new");
        ph_argv_push(&b, "-key");
        ph_argv_push(&b, privkey_path);
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, csr_path);
        ph_argv_push(&b, "-subj");
        ph_argv_push(&b, subj);
        ph_argv_push(&b, "-config");
        ph_argv_push(&b, cnf_path);
        char **argv = ph_argv_finalize(&b);
        if (!argv) goto fail;

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl req (CSR) failed (exit %d)", exit_code);
            goto fail;
        }
    }

    if (ph_signal_interrupted()) goto fail_signal;

    /* step 4: openssl x509 -req ... sign with CA */
    {
        ph_argv_builder_t b;
        if (ph_argv_init(&b, 20) != PH_OK) goto fail;
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "x509");
        ph_argv_push(&b, "-req");
        ph_argv_push(&b, "-in");
        ph_argv_push(&b, csr_path);
        ph_argv_push(&b, "-CA");
        ph_argv_push(&b, ca_crt);
        ph_argv_push(&b, "-CAkey");
        ph_argv_push(&b, ca_key);
        ph_argv_push(&b, "-CAcreateserial");
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, fullchain_path);
        ph_argv_push(&b, "-days");
        ph_argv_pushf(&b, "%d", config->leaf_days);
        ph_argv_push(&b, "-sha256");
        /* audit fix (2026-04-08T11-07-17Z): always carry the SAN
         * extension through to the signed cert, because eff_sans
         * is guaranteed non-empty and the CN-only path produced
         * certs that modern TLS clients reject. */
        ph_argv_push(&b, "-extfile");
        ph_argv_push(&b, cnf_path);
        ph_argv_push(&b, "-extensions");
        ph_argv_push(&b, "v3_req");
        char **argv = ph_argv_finalize(&b);
        if (!argv) goto fail;

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl x509 (sign) failed (exit %d)", exit_code);
            goto fail;
        }
    }

    /* step 5: cleanup temp files */
    unlink(csr_path);
    unlink(cnf_path);

    ph_log_info("certs: leaf %s -> %s", domain->name, fullchain_path);

success:
    ph_cert_domain_sans_free(eff_sans, eff_count);
    ph_free(ca_dir);
    ph_free(ca_key);
    ph_free(ca_crt);
    ph_free(leaf_dir);
    ph_free(privkey_path);
    ph_free(fullchain_path);
    ph_free(csr_path);
    ph_free(cnf_path);
    return PH_OK;

fail_signal:
    if (err)
        *err = ph_error_createf(PH_ERR_SIGNAL, 0, "interrupted");
fail:
    /* cleanup partial artifacts */
    unlink(csr_path);
    unlink(cnf_path);
fail_alloc:
    ph_cert_domain_sans_free(eff_sans, eff_count);
    ph_free(ca_dir);
    ph_free(ca_key);
    ph_free(ca_crt);
    ph_free(leaf_dir);
    ph_free(privkey_path);
    ph_free(fullchain_path);
    ph_free(csr_path);
    ph_free(cnf_path);
    return PH_ERR;
}

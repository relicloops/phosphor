#include "phosphor/alloc.h"
#include "phosphor/certs.h"
#include "phosphor/cli.h"
#include "phosphor/commands.h"
#include "phosphor/error.h"
#include "phosphor/fs.h"
#include "phosphor/log.h"
#include "phosphor/path.h"
#include "phosphor/platform.h"
#include "phosphor/proc.h"
#include "phosphor/signal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- local cert pipeline ---- */

static int run_local(const ph_certs_config_t *config,
                     const char *project_root,
                     bool ca_only, const char *domain_filter,
                     bool dry_run, bool force) {
    ph_error_t *err = NULL;

    /* generate CA */
    if (ph_certs_gen_ca(config, project_root, dry_run, force, &err) != PH_OK) {
        ph_log_error("certs: CA generation failed: %s",
                     err ? err->message : "unknown");
        int rc = err ? (int)err->category : PH_ERR_PROCESS;
        ph_error_destroy(err);
        return rc;
    }

    if (ca_only) {
        ph_log_info("certs: root CA generated");
        return 0;
    }

    if (ph_signal_interrupted()) return PH_ERR_SIGNAL;

    /* generate leaf certs for each local domain */
    size_t generated = 0;
    for (size_t i = 0; i < config->domain_count; i++) {
        const ph_cert_domain_t *d = &config->domains[i];
        if (d->mode != PH_CERT_LOCAL) continue;
        if (domain_filter && strcmp(d->name, domain_filter) != 0) continue;

        if (ph_signal_interrupted()) return PH_ERR_SIGNAL;

        if (ph_certs_gen_leaf(config, d, project_root, dry_run, force,
                               &err) != PH_OK) {
            ph_log_error("certs: leaf generation failed for %s: %s",
                         d->name, err ? err->message : "unknown");
            int rc = err ? (int)err->category : PH_ERR_PROCESS;
            ph_error_destroy(err);
            return rc;
        }
        generated++;
    }

    if (domain_filter && generated == 0) {
        ph_log_error("certs: no local domain matching '%s'", domain_filter);
        return PH_ERR_CONFIG;
    }

    ph_log_info("certs: local -- CA + %zu leaf cert(s) generated", generated);
    return 0;
}

/* ---- letsencrypt pipeline ---- */

static int run_letsencrypt(const ph_certs_config_t *config,
                           const char *project_root,
                           const char *domain_filter,
                           const char *action,
                           const char *directory_url,
                           bool dry_run, bool force) {
#ifndef PHOSPHOR_HAS_LIBCURL
    (void)config; (void)project_root; (void)domain_filter;
    (void)action; (void)directory_url; (void)dry_run; (void)force;
    ph_log_error("certs: Let's Encrypt requires phosphor built with "
                 "-Dlibcurl=true");
    return PH_ERR_CONFIG;
#else
    (void)force;

    if (!action) action = "request";

    /* resolve account key path */
    const char *key_path = config->account_key;
    char *default_key = NULL;
    if (!key_path || !*key_path) {
        const char *home = getenv("HOME");
        if (home) {
            char *dir = ph_path_join(home, ".phosphor/acme");
            if (dir) {
                default_key = ph_path_join(dir, "account.key");
                ph_free(dir);
            }
        }
        key_path = default_key;
    }

    if (!key_path) {
        ph_log_error("certs: cannot determine ACME account key path");
        return PH_ERR_CONFIG;
    }

    size_t processed = 0;
    for (size_t i = 0; i < config->domain_count; i++) {
        const ph_cert_domain_t *d = &config->domains[i];
        if (d->mode != PH_CERT_LETSENCRYPT) continue;
        if (domain_filter && strcmp(d->name, domain_filter) != 0) continue;

        if (ph_signal_interrupted()) {
            ph_free(default_key);
            return PH_ERR_SIGNAL;
        }

        /* determine output directory */
        const char *dir_name = d->dir_name ? d->dir_name : d->name;
        char *cert_dir = ph_path_join(project_root, config->output_dir);
        char *domain_dir = cert_dir ? ph_path_join(cert_dir, dir_name) : NULL;
        ph_free(cert_dir);

        if (!domain_dir) {
            ph_free(default_key);
            return PH_ERR_INTERNAL;
        }

        if (dry_run) {
            if (strcmp(action, "request") == 0) {
                ph_log_info("dry-run: would request LE cert for %s", d->name);
                ph_log_info("  SANs: %zu", d->san_count);
                ph_log_info("  email: %s", d->email);
                ph_log_info("  webroot: %s", d->webroot);
                ph_log_info("  output: %s", domain_dir);
                ph_log_info("  account key: %s", key_path);
                ph_log_info("  ACME flow:");
                ph_log_info("    1. ensure account key exists");
                ph_log_info("    2. GET directory from LE");
                ph_log_info("    3. HEAD newNonce for replay nonce");
                ph_log_info("    4. POST newAccount (register/find)");
                ph_log_info("    5. POST newOrder with domain identifiers");
                ph_log_info("    6. GET authorization URLs");
                ph_log_info("    7. extract HTTP-01 challenge");
                ph_log_info("    8. compute key authorization");
                ph_log_info("    9. write token to %s/.well-known/acme-challenge/",
                            d->webroot);
                ph_log_info("   10. POST challenge response");
                ph_log_info("   11. poll authorization until valid");
                ph_log_info("   12. POST finalize with CSR");
                ph_log_info("   13. GET certificate chain -> %s/fullchain.pem",
                            domain_dir);
            } else if (strcmp(action, "renew") == 0) {
                ph_log_info("dry-run: would renew LE cert for %s", d->name);
            } else if (strcmp(action, "verify") == 0) {
                ph_log_info("dry-run: would verify LE cert for %s", d->name);
            }
            ph_free(domain_dir);
            processed++;
            continue;
        }

        ph_error_t *err = NULL;

        if (strcmp(action, "request") == 0 || strcmp(action, "renew") == 0) {
            /* ensure account key */
            if (ph_acme_account_ensure(key_path, &err) != PH_OK) {
                ph_log_error("certs: account key setup failed: %s",
                             err ? err->message : "unknown");
                int rc = err ? (int)err->category : PH_ERR_PROCESS;
                ph_error_destroy(err);
                ph_free(domain_dir);
                ph_free(default_key);
                return rc;
            }

            /* register account */
            char *account_url = NULL;
            if (ph_acme_account_register(key_path, d->email, directory_url,
                                          &account_url, &err) != PH_OK) {
                ph_log_error("certs: ACME registration failed: %s",
                             err ? err->message : "unknown");
                int rc = err ? (int)err->category : PH_ERR_PROCESS;
                ph_error_destroy(err);
                ph_free(domain_dir);
                ph_free(default_key);
                return rc;
            }

            /* create order */
            char *order_url = NULL;
            char *finalize_url = NULL;
            char **auth_urls = NULL;
            size_t auth_count = 0;

            /* newOrder URL -- derived from the directory base.
             * production: .../acme/new-order
             * staging:    .../acme/new-order  (same path, different host) */
            size_t dir_len = strlen(directory_url);
            /* strip trailing "/directory" to get base */
            const char *dir_suffix = "/directory";
            size_t suffix_len = strlen(dir_suffix);
            size_t base_len = dir_len;
            if (dir_len > suffix_len &&
                strcmp(directory_url + dir_len - suffix_len, dir_suffix) == 0)
                base_len = dir_len - suffix_len;

            size_t new_order_cap = base_len + 32;
            char *new_order_url = ph_alloc(new_order_cap);
            if (!new_order_url) {
                ph_free(account_url);
                ph_free(domain_dir);
                ph_free(default_key);
                return PH_ERR_INTERNAL;
            }
            snprintf(new_order_url, new_order_cap, "%.*s/acme/new-order",
                     (int)base_len, directory_url);

            if (ph_acme_order_create(key_path, account_url, new_order_url,
                                      directory_url,
                                      (const char *const *)d->san,
                                      d->san_count,
                                      &order_url, &finalize_url,
                                      &auth_urls, &auth_count,
                                      &err) != PH_OK) {
                ph_free(new_order_url);
                ph_log_error("certs: ACME order failed: %s",
                             err ? err->message : "unknown");
                int rc = err ? (int)err->category : PH_ERR_PROCESS;
                ph_error_destroy(err);
                ph_free(account_url);
                ph_free(domain_dir);
                ph_free(default_key);
                return rc;
            }
            ph_free(new_order_url);

            /* respond to challenges */
            for (size_t a = 0; a < auth_count; a++) {
                if (ph_signal_interrupted()) {
                    for (size_t j = 0; j < auth_count; j++)
                        ph_free(auth_urls[j]);
                    ph_free(auth_urls);
                    ph_free(order_url);
                    ph_free(finalize_url);
                    ph_free(account_url);
                    ph_free(domain_dir);
                    ph_free(default_key);
                    return PH_ERR_SIGNAL;
                }

                if (ph_acme_challenge_respond(key_path, account_url,
                                               auth_urls[a], d->webroot,
                                               directory_url,
                                               &err) != PH_OK) {
                    ph_log_error("certs: ACME challenge failed: %s",
                                 err ? err->message : "unknown");
                    int rc = err ? (int)err->category : PH_ERR_PROCESS;
                    ph_error_destroy(err);
                    for (size_t j = 0; j < auth_count; j++)
                        ph_free(auth_urls[j]);
                    ph_free(auth_urls);
                    ph_free(order_url);
                    ph_free(finalize_url);
                    ph_free(account_url);
                    ph_free(domain_dir);
                    ph_free(default_key);
                    return rc;
                }
            }

            /* finalize + download cert */
            char *privkey_path = ph_path_join(domain_dir, "privkey.pem");
            char *cert_path = ph_path_join(domain_dir, "fullchain.pem");

            if (privkey_path && cert_path) {
                /* ensure output directory */
                ph_fs_mkdir_p(domain_dir, 0755);

                if (ph_acme_finalize(key_path, account_url, finalize_url,
                                      order_url, directory_url,
                                      (const char *const *)d->san,
                                      d->san_count,
                                      privkey_path, cert_path,
                                      &err) != PH_OK) {
                    ph_log_error("certs: ACME finalize failed: %s",
                                 err ? err->message : "unknown");
                    int rc = err ? (int)err->category : PH_ERR_PROCESS;
                    ph_error_destroy(err);
                    ph_free(privkey_path);
                    ph_free(cert_path);
                    for (size_t j = 0; j < auth_count; j++)
                        ph_free(auth_urls[j]);
                    ph_free(auth_urls);
                    ph_free(order_url);
                    ph_free(finalize_url);
                    ph_free(account_url);
                    ph_free(domain_dir);
                    ph_free(default_key);
                    return rc;
                }
            }

            ph_free(privkey_path);
            ph_free(cert_path);
            for (size_t j = 0; j < auth_count; j++)
                ph_free(auth_urls[j]);
            ph_free(auth_urls);
            ph_free(order_url);
            ph_free(finalize_url);
            ph_free(account_url);

        } else if (strcmp(action, "verify") == 0) {
            /* check cert expiry via openssl CLI */
            char *cert_path = ph_path_join(domain_dir, "fullchain.pem");
            if (cert_path) {
                ph_fs_stat_t st;
                if (ph_fs_stat(cert_path, &st) != PH_OK || !st.is_file) {
                    ph_log_error("certs: no certificate found at %s",
                                 cert_path);
                    ph_free(cert_path);
                    ph_free(domain_dir);
                    ph_free(default_key);
                    return PH_ERR_FS;
                }

                /* check if cert expires within 30 days (2592000 seconds) */
                ph_argv_builder_t vb;
                if (ph_argv_init(&vb, 8) != PH_OK) {
                    ph_free(cert_path);
                    ph_free(domain_dir);
                    ph_free(default_key);
                    return PH_ERR_INTERNAL;
                }
                ph_argv_push(&vb, "openssl");
                ph_argv_push(&vb, "x509");
                ph_argv_push(&vb, "-checkend");
                ph_argv_push(&vb, "2592000");
                ph_argv_push(&vb, "-noout");
                ph_argv_push(&vb, "-in");
                ph_argv_push(&vb, cert_path);
                char **vargv = ph_argv_finalize(&vb);

                if (!vargv) {
                    ph_free(cert_path);
                    ph_free(domain_dir);
                    ph_free(default_key);
                    return PH_ERR_INTERNAL;
                }

                ph_proc_opts_t vopts = {
                    .argv = vargv, .cwd = NULL, .env = NULL
                };
                int vexit = 0;
                ph_result_t vrc = ph_proc_exec(&vopts, &vexit);
                ph_argv_free(vargv);

                if (vrc != PH_OK) {
                    ph_log_error("certs: openssl x509 failed for %s",
                                 d->name);
                    ph_free(cert_path);
                    ph_free(domain_dir);
                    ph_free(default_key);
                    return PH_ERR_PROCESS;
                }

                if (vexit == 0) {
                    ph_log_info("certs: %s -- valid (> 30 days remaining)",
                                d->name);
                } else {
                    ph_log_warn("certs: %s -- expiring soon or expired "
                                "(< 30 days)", d->name);
                }

                ph_free(cert_path);
            }
        } else {
            ph_log_error("certs: unknown action '%s' "
                         "(expected: request, renew, verify)", action);
            ph_free(domain_dir);
            ph_free(default_key);
            return PH_ERR_USAGE;
        }

        ph_free(domain_dir);
        processed++;
    }

    ph_free(default_key);

    if (domain_filter && processed == 0) {
        ph_log_error("certs: no letsencrypt domain matching '%s'",
                     domain_filter);
        return PH_ERR_CONFIG;
    }

    ph_log_info("certs: letsencrypt -- %zu domain(s) processed", processed);
    return 0;
#endif /* PHOSPHOR_HAS_LIBCURL */
}

/* ---- main entry point ---- */

int ph_cmd_certs(const ph_cli_config_t *config,
                  const ph_parsed_args_t *args) {
    (void)config;

    /* extract flags */
    bool f_generate    = ph_args_has_flag(args, "generate");
    bool f_local       = ph_args_has_flag(args, "local");
    bool f_letsencrypt = ph_args_has_flag(args, "letsencrypt");
    bool f_ca_only     = ph_args_has_flag(args, "ca-only");
    bool f_dry_run     = ph_args_has_flag(args, "dry-run");
    bool f_force       = ph_args_has_flag(args, "force");
    bool f_verbose     = ph_args_has_flag(args, "verbose");
    bool f_staging     = ph_args_has_flag(args, "staging");
    const char *domain_filter = ph_args_get_flag(args, "domain");
    const char *action        = ph_args_get_flag(args, "action");
    const char *project_flag  = ph_args_get_flag(args, "project");
    const char *output_flag   = ph_args_get_flag(args, "output");

    if (f_verbose) {
        ph_log_set_level(PH_LOG_DEBUG);
    }

    /* require at least one mode flag */
    if (!f_generate && !f_local && !f_letsencrypt) {
        ph_log_error("certs: specify --generate, --local, or --letsencrypt");
        return PH_ERR_USAGE;
    }

    /* resolve project root */
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        ph_log_error("certs: cannot determine working directory");
        return PH_ERR_INTERNAL;
    }
    const char *project_root = project_flag ? project_flag : cwd;

    /* find and load TOML manifest */
    char *toml_path = ph_path_join(project_root, "template.phosphor.toml");
    if (!toml_path) return PH_ERR_INTERNAL;

    ph_error_t *err = NULL;
    ph_certs_config_t certs_cfg;
    if (ph_certs_config_parse(toml_path, &certs_cfg, &err) != PH_OK) {
        ph_log_error("certs: %s", err ? err->message : "unknown");
        int rc = err ? (int)err->category : PH_ERR_CONFIG;
        ph_error_destroy(err);
        ph_free(toml_path);
        return rc;
    }
    ph_free(toml_path);

    if (!certs_cfg.present) {
        ph_log_error("certs: no [certs] section in template.phosphor.toml");
        ph_certs_config_destroy(&certs_cfg);
        return PH_ERR_CONFIG;
    }

    /* override output directory if --output given */
    if (output_flag) {
        ph_free(certs_cfg.output_dir);
        size_t len = strlen(output_flag);
        certs_cfg.output_dir = ph_alloc(len + 1);
        if (certs_cfg.output_dir)
            memcpy(certs_cfg.output_dir, output_flag, len + 1);
    }

    /* select ACME directory URL */
    const char *directory_url = f_staging
        ? PH_ACME_DIRECTORY_STAGING
        : PH_ACME_DIRECTORY_PRODUCTION;

    if (f_staging)
        ph_log_info("certs: using Let's Encrypt staging endpoint");

    /* dispatch */
    int exit_code = 0;

    if (f_generate) {
        /* CA + all local + all LE */
        exit_code = run_local(&certs_cfg, project_root, false, NULL,
                              f_dry_run, f_force);
        if (exit_code == 0 && !ph_signal_interrupted()) {
            exit_code = run_letsencrypt(&certs_cfg, project_root, NULL,
                                         "request", directory_url,
                                         f_dry_run, f_force);
        }
    } else if (f_local) {
        exit_code = run_local(&certs_cfg, project_root, f_ca_only,
                              domain_filter, f_dry_run, f_force);
    } else if (f_letsencrypt) {
        exit_code = run_letsencrypt(&certs_cfg, project_root, domain_filter,
                                     action, directory_url,
                                     f_dry_run, f_force);
    }

    ph_certs_config_destroy(&certs_cfg);
    return exit_code;
}

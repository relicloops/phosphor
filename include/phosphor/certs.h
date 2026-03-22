#ifndef PHOSPHOR_CERTS_H
#define PHOSPHOR_CERTS_H

#include "phosphor/types.h"
#include "phosphor/error.h"

#include <stdbool.h>
#include <stddef.h>

/* ---- constants ---- */

#define PH_MAX_CERT_DOMAINS  64
#define PH_CERTS_DEFAULT_DIR "certs"
#define PH_CERTS_DEFAULT_CA_CN "phosphor-local-CA"
#define PH_CERTS_DEFAULT_CA_BITS 4096
#define PH_CERTS_DEFAULT_CA_DAYS 3650
#define PH_CERTS_DEFAULT_LEAF_BITS 2048
#define PH_CERTS_DEFAULT_LEAF_DAYS 825

#define PH_ACME_DIRECTORY_PRODUCTION \
    "https://acme-v02.api.letsencrypt.org/directory"
#define PH_ACME_DIRECTORY_STAGING \
    "https://acme-staging-v02.api.letsencrypt.org/directory"

/* ---- cert mode ---- */

typedef enum {
    PH_CERT_LOCAL,
    PH_CERT_LETSENCRYPT
} ph_cert_mode_t;

/* ---- domain entry ---- */

/*
 * ph_cert_domain_t -- single domain entry from [[certs.domains]].
 *
 * ownership:
 *   name, san[], dir_name, email, webroot -- owner: self (heap-allocated)
 */
typedef struct {
    char            *name;
    ph_cert_mode_t   mode;
    char           **san;
    size_t           san_count;
    char            *dir_name;     /* nullable override */
    char            *email;        /* LE only */
    char            *webroot;      /* LE only */
} ph_cert_domain_t;

/* ---- certs config ---- */

/*
 * ph_certs_config_t -- parsed [certs] section from TOML manifest.
 *
 * ownership:
 *   output_dir, ca_cn, account_key, domains -- owner: self (heap-allocated)
 */
typedef struct {
    char              *output_dir;   /* "certs" */
    char              *ca_cn;        /* "phosphor-local-CA" */
    int                ca_bits;      /* 4096 */
    int                ca_days;      /* 3650 */
    int                leaf_bits;    /* 2048 */
    int                leaf_days;    /* 825 */
    char              *account_key;  /* NULL = ~/.phosphor/acme/account.key */
    ph_cert_domain_t  *domains;
    size_t             domain_count;
    bool               present;
} ph_certs_config_t;

/* ---- TOML parsing ---- */

/*
 * ph_certs_config_parse -- load [certs] section from a TOML file.
 * sets config->present = false if no [certs] section exists (not an error).
 */
ph_result_t ph_certs_config_parse(const char *toml_path,
                                   ph_certs_config_t *config,
                                   ph_error_t **err);

void ph_certs_config_destroy(ph_certs_config_t *config);

/* ---- SAN helpers ---- */

/*
 * ph_cert_san_is_ip -- returns true if the SAN entry looks like an IP address.
 */
bool ph_cert_san_is_ip(const char *san);

/*
 * ph_cert_san_write_cnf -- write an openssl SAN config file.
 * returns PH_OK on success or PH_ERR with err set.
 */
ph_result_t ph_cert_san_write_cnf(const char *cnf_path,
                                    const char *const *san,
                                    size_t san_count,
                                    ph_error_t **err);

/* ---- local CA generation ---- */

/*
 * ph_certs_gen_ca -- generate a self-signed root CA.
 * creates output_dir/ca/root.key and output_dir/ca/root.crt.
 */
ph_result_t ph_certs_gen_ca(const ph_certs_config_t *config,
                             const char *project_root,
                             bool dry_run, bool force,
                             ph_error_t **err);

/* ---- local leaf generation ---- */

/*
 * ph_certs_gen_leaf -- generate a CA-signed leaf certificate.
 * creates output_dir/<dir>/privkey.pem and output_dir/<dir>/fullchain.pem.
 */
ph_result_t ph_certs_gen_leaf(const ph_certs_config_t *config,
                               const ph_cert_domain_t *domain,
                               const char *project_root,
                               bool dry_run, bool force,
                               ph_error_t **err);

/* ---- base64url encoding (no libcurl dependency) ---- */

/*
 * ph_acme_base64url_encode -- base64url-encode raw bytes (no padding).
 * caller owns returned string (free with ph_free).
 */
char *ph_acme_base64url_encode(const uint8_t *data, size_t len);

/* ---- ACME (Let's Encrypt) ---- */

#ifdef PHOSPHOR_HAS_LIBCURL

/*
 * ph_acme_http_get -- perform an HTTPS GET request via libcurl.
 * caller owns *out_body (free with ph_free).
 */
ph_result_t ph_acme_http_get(const char *url,
                               char **out_body, size_t *out_len,
                               ph_error_t **err);

/*
 * ph_acme_http_head -- perform an HTTPS HEAD request; extract a header.
 * caller owns *out_header_value (free with ph_free).
 */
ph_result_t ph_acme_http_head(const char *url,
                                const char *header_name,
                                char **out_header_value,
                                ph_error_t **err);

/*
 * ph_acme_http_post -- perform an HTTPS POST with JWS body.
 * caller owns *out_body, *out_nonce, *out_location (free with ph_free).
 */
ph_result_t ph_acme_http_post(const char *url,
                                const char *jws_body,
                                char **out_body, size_t *out_len,
                                char **out_nonce,
                                char **out_location,
                                int *out_http_status,
                                ph_error_t **err);

/* ---- JWS signing ---- */

/*
 * ph_acme_jws_sign -- build and sign a JWS (RS256) via openssl CLI.
 * caller owns *out_jws (free with ph_free).
 */
ph_result_t ph_acme_jws_sign(const char *key_path,
                               const char *protected_json,
                               const char *payload_json,
                               char **out_jws,
                               ph_error_t **err);

/*
 * ph_acme_jwk_json -- build canonical JWK JSON for an RSA key.
 * returns {"e":"AQAB","kty":"RSA","n":"..."}.
 * caller owns *out_jwk (free with ph_free).
 */
ph_result_t ph_acme_jwk_json(const char *key_path,
                                char **out_jwk,
                                ph_error_t **err);

/*
 * ph_acme_jwk_thumbprint -- compute JWK SHA-256 thumbprint of an RSA key.
 * caller owns *out_thumbprint (free with ph_free).
 */
ph_result_t ph_acme_jwk_thumbprint(const char *key_path,
                                     char **out_thumbprint,
                                     ph_error_t **err);

/* ---- ACME account ---- */

/*
 * ph_acme_account_ensure -- ensure ACME account key exists; create if needed.
 */
ph_result_t ph_acme_account_ensure(const char *key_path,
                                     ph_error_t **err);

/*
 * ph_acme_account_register -- register/find ACME account with LE directory.
 * caller owns *out_account_url (free with ph_free).
 */
ph_result_t ph_acme_account_register(const char *key_path,
                                       const char *email,
                                       const char *directory_url,
                                       char **out_account_url,
                                       ph_error_t **err);

/* ---- ACME order ---- */

/*
 * ph_acme_order_create -- create a new certificate order.
 * caller owns all output strings (free with ph_free).
 */
ph_result_t ph_acme_order_create(const char *key_path,
                                   const char *account_url,
                                   const char *new_order_url,
                                   const char *directory_url,
                                   const char *const *domains,
                                   size_t domain_count,
                                   char **out_order_url,
                                   char **out_finalize_url,
                                   char ***out_auth_urls,
                                   size_t *out_auth_count,
                                   ph_error_t **err);

/* ---- ACME challenge ---- */

/*
 * ph_acme_challenge_respond -- write token, respond to challenge, poll.
 */
ph_result_t ph_acme_challenge_respond(const char *key_path,
                                        const char *account_url,
                                        const char *auth_url,
                                        const char *webroot,
                                        const char *directory_url,
                                        ph_error_t **err);

/* ---- ACME finalize ---- */

/*
 * ph_acme_finalize -- submit CSR and download certificate.
 */
ph_result_t ph_acme_finalize(const char *key_path,
                               const char *account_url,
                               const char *finalize_url,
                               const char *order_url,
                               const char *directory_url,
                               const char *const *domains,
                               size_t domain_count,
                               const char *privkey_path,
                               const char *cert_path,
                               ph_error_t **err);

#endif /* PHOSPHOR_HAS_LIBCURL */

#endif /* PHOSPHOR_CERTS_H */

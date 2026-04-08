/* base64url encoding -- no libcurl dependency, always compiled */

#include "phosphor/certs.h"
#include "phosphor/alloc.h"

#include <string.h>

static const char b64url_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char *ph_acme_base64url_encode(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        char *empty = ph_alloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    /* base64url output size: ceil(len * 4 / 3), no padding */
    size_t out_cap = ((len + 2) / 3) * 4 + 1;
    char *out = ph_alloc(out_cap);
    if (!out) return NULL;

    size_t i = 0, j = 0;
    while (i < len) {
        uint32_t a = data[i++];
        uint32_t b = (i < len) ? data[i++] : 0;
        uint32_t c = (i < len) ? data[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;

        out[j++] = b64url_table[(triple >> 18) & 0x3F];
        out[j++] = b64url_table[(triple >> 12) & 0x3F];
        out[j++] = b64url_table[(triple >> 6) & 0x3F];
        out[j++] = b64url_table[triple & 0x3F];
    }

    /* remove padding: for every missing input byte, remove one output char */
    size_t pad = (3 - (len % 3)) % 3;
    j -= pad;
    out[j] = '\0';
    return out;
}

#ifdef PHOSPHOR_HAS_LIBCURL

#include "phosphor/log.h"
#include "phosphor/proc.h"
#include "phosphor/platform.h"
#include "phosphor/fs.h"
#include "phosphor/path.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- base64url encode a string ---- */

static char *base64url_encode_str(const char *str) {
    return ph_acme_base64url_encode((const uint8_t *)str, strlen(str));
}

/*
 * exec_to_file -- run a child process with stdout redirected to a file.
 *
 * argv is a NULL-terminated argv array for execvp. stdout_path is opened
 * with O_WRONLY|O_CREAT|O_TRUNC (0600). stderr is redirected to /dev/null
 * to suppress noise on expected failures (matches the old 2>/dev/null).
 *
 * returns PH_OK and writes the child's exit code to *out_exit on success
 * (child spawned and waited). returns PH_ERR on fork/open failure, or if
 * the child was killed by a signal.
 *
 * this replaces `sh -c "openssl ... '<var>' > '<out>'"` which is vulnerable
 * to shell injection via single-quoted variable contents.
 */
static ph_result_t exec_to_file(char *const argv[], const char *stdout_path,
                                 int *out_exit) {
    if (!argv || !argv[0] || !stdout_path || !out_exit) return PH_ERR;

    pid_t pid = fork();
    if (pid < 0) return PH_ERR;

    if (pid == 0) {
        /* child */
        int fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) _exit(127);
        if (dup2(fd, STDOUT_FILENO) < 0) _exit(127);
        close(fd);

        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        execvp(argv[0], argv);
        _exit(127);
    }

    /* parent */
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return PH_ERR;
    if (WIFEXITED(status)) {
        *out_exit = WEXITSTATUS(status);
        return PH_OK;
    }
    return PH_ERR;
}

/* ---- JWS signing via openssl CLI ---- */

ph_result_t ph_acme_jws_sign(const char *key_path,
                               const char *protected_json,
                               const char *payload_json,
                               char **out_jws,
                               ph_error_t **err) {
    if (!key_path || !protected_json || !out_jws) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_jws_sign: NULL argument");
        return PH_ERR;
    }

    /* base64url-encode protected header and payload */
    char *b64_protected = base64url_encode_str(protected_json);
    char *b64_payload = NULL;
    if (payload_json && *payload_json) {
        b64_payload = base64url_encode_str(payload_json);
    } else {
        /* empty payload for POST-as-GET */
        b64_payload = ph_alloc(1);
        if (b64_payload) b64_payload[0] = '\0';
    }

    if (!b64_protected || !b64_payload) {
        ph_free(b64_protected);
        ph_free(b64_payload);
        return PH_ERR;
    }

    /* build signing input: "{protected}.{payload}" */
    size_t input_len = strlen(b64_protected) + 1 + strlen(b64_payload);
    char *signing_input = ph_alloc(input_len + 1);
    if (!signing_input) {
        ph_free(b64_protected);
        ph_free(b64_payload);
        return PH_ERR;
    }
    snprintf(signing_input, input_len + 1, "%s.%s", b64_protected, b64_payload);

    /* write signing input to temp file */
    char tmp_input[] = "/tmp/ph_jws_input_XXXXXX";
    int fd = mkstemp(tmp_input);
    if (fd < 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create temp file for JWS signing");
        ph_free(signing_input);
        ph_free(b64_protected);
        ph_free(b64_payload);
        return PH_ERR;
    }
    write(fd, signing_input, input_len);
    close(fd);

    /* temp file for signature output */
    char tmp_sig[] = "/tmp/ph_jws_sig_XXXXXX";
    int sig_fd = mkstemp(tmp_sig);
    if (sig_fd < 0) {
        unlink(tmp_input);
        ph_free(signing_input);
        ph_free(b64_protected);
        ph_free(b64_payload);
        return PH_ERR;
    }
    close(sig_fd);

    /* sign: openssl dgst -sha256 -sign <key> -out <sig> <input> */
    {
        ph_argv_builder_t b;
        if (ph_argv_init(&b, 10) != PH_OK) {
            unlink(tmp_input);
            unlink(tmp_sig);
            ph_free(signing_input);
            ph_free(b64_protected);
            ph_free(b64_payload);
            return PH_ERR;
        }
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "dgst");
        ph_argv_push(&b, "-sha256");
        ph_argv_push(&b, "-sign");
        ph_argv_push(&b, key_path);
        ph_argv_push(&b, "-out");
        ph_argv_push(&b, tmp_sig);
        ph_argv_push(&b, tmp_input);
        char **argv = ph_argv_finalize(&b);
        if (!argv) {
            unlink(tmp_input);
            unlink(tmp_sig);
            ph_free(signing_input);
            ph_free(b64_protected);
            ph_free(b64_payload);
            return PH_ERR;
        }

        ph_proc_opts_t opts = {.argv = argv, .cwd = NULL, .env = NULL};
        int exit_code = 0;
        ph_result_t rc = ph_proc_exec(&opts, &exit_code);
        ph_argv_free(argv);

        if (rc != PH_OK || exit_code != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_PROCESS, 0,
                    "openssl dgst -sign failed (exit %d)", exit_code);
            unlink(tmp_input);
            unlink(tmp_sig);
            ph_free(signing_input);
            ph_free(b64_protected);
            ph_free(b64_payload);
            return PH_ERR;
        }
    }

    /* read signature */
    uint8_t *sig_data = NULL;
    size_t sig_len = 0;
    if (ph_fs_read_file(tmp_sig, &sig_data, &sig_len) != PH_OK || !sig_data) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot read JWS signature from %s", tmp_sig);
        unlink(tmp_input);
        unlink(tmp_sig);
        ph_free(signing_input);
        ph_free(b64_protected);
        ph_free(b64_payload);
        return PH_ERR;
    }

    unlink(tmp_input);
    unlink(tmp_sig);

    /* base64url-encode the signature */
    char *b64_signature = ph_acme_base64url_encode(sig_data, sig_len);
    ph_free(sig_data);

    if (!b64_signature) {
        ph_free(signing_input);
        ph_free(b64_protected);
        ph_free(b64_payload);
        return PH_ERR;
    }

    /* assemble JWS JSON:
     * {"protected":"","payload":"","signature":""} = 45 chars overhead */
    size_t jws_len = 48 + strlen(b64_protected) + strlen(b64_payload) +
                     strlen(b64_signature);
    char *jws = ph_alloc(jws_len + 1);
    if (!jws) {
        ph_free(b64_signature);
        ph_free(signing_input);
        ph_free(b64_protected);
        ph_free(b64_payload);
        return PH_ERR;
    }

    snprintf(jws, jws_len + 1,
        "{\"protected\":\"%s\",\"payload\":\"%s\",\"signature\":\"%s\"}",
        b64_protected, b64_payload, b64_signature);

    ph_free(signing_input);
    ph_free(b64_protected);
    ph_free(b64_payload);
    ph_free(b64_signature);

    *out_jws = jws;
    return PH_OK;
}

/* ---- JWK JSON ---- */

/*
 * Extract the RSA modulus from a key file via openssl CLI,
 * build the canonical JWK: {"e":"AQAB","kty":"RSA","n":"..."}
 */
ph_result_t ph_acme_jwk_json(const char *key_path,
                                char **out_jwk,
                                ph_error_t **err) {
    if (!key_path || !out_jwk) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_jwk_json: NULL argument");
        return PH_ERR;
    }

    /* get modulus hex: openssl rsa -in <key> -modulus -noout */
    char tmp_modout[] = "/tmp/ph_jwk_modout_XXXXXX";
    int modout_fd = mkstemp(tmp_modout);
    if (modout_fd < 0) {
        if (err)
            *err = ph_error_createf(PH_ERR_FS, 0,
                "cannot create temp file for JWK");
        return PH_ERR;
    }
    close(modout_fd);

    {
        /* audit fix: direct argv execvp via exec_to_file, no shell -- key_path
         * can no longer inject shell metacharacters through single quotes. */
        ph_argv_builder_t b;
        if (ph_argv_init(&b, 8) != PH_OK) {
            unlink(tmp_modout);
            return PH_ERR;
        }
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "rsa");
        ph_argv_push(&b, "-in");
        ph_argv_push(&b, key_path);
        ph_argv_push(&b, "-modulus");
        ph_argv_push(&b, "-noout");
        char **argv = ph_argv_finalize(&b);
        if (!argv) {
            unlink(tmp_modout);
            return PH_ERR;
        }

        int exit_code = 0;
        exec_to_file(argv, tmp_modout, &exit_code);
        ph_argv_free(argv);
    }

    /* read modulus output */
    uint8_t *mod_data = NULL;
    size_t mod_len = 0;
    if (ph_fs_read_file(tmp_modout, &mod_data, &mod_len) != PH_OK ||
        !mod_data) {
        unlink(tmp_modout);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "cannot read modulus from openssl");
        return PH_ERR;
    }
    unlink(tmp_modout);

    /* parse "Modulus=<hex>\n" */
    char *hex_start = strstr((char *)mod_data, "Modulus=");
    if (!hex_start) {
        ph_free(mod_data);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "unexpected openssl modulus output");
        return PH_ERR;
    }
    hex_start += 8; /* skip "Modulus=" */

    /* find end of hex string */
    size_t hex_len = 0;
    while (hex_start[hex_len] && hex_start[hex_len] != '\n' &&
           hex_start[hex_len] != '\r')
        hex_len++;

    /* convert hex to binary */
    size_t bin_len = hex_len / 2;
    uint8_t *mod_bin = ph_alloc(bin_len);
    if (!mod_bin) {
        ph_free(mod_data);
        return PH_ERR;
    }

    for (size_t i = 0; i < bin_len; i++) {
        unsigned int byte;
        sscanf(hex_start + i * 2, "%2x", &byte);
        mod_bin[i] = (uint8_t)byte;
    }
    ph_free(mod_data);

    /* skip leading zeros in modulus */
    size_t mod_offset = 0;
    while (mod_offset < bin_len && mod_bin[mod_offset] == 0)
        mod_offset++;

    /* base64url encode modulus */
    char *b64_n = ph_acme_base64url_encode(mod_bin + mod_offset,
                                             bin_len - mod_offset);
    ph_free(mod_bin);
    if (!b64_n) return PH_ERR;

    /* build canonical JWK: {"e":"AQAB","kty":"RSA","n":"..."} */
    size_t jwk_cap = 40 + strlen(b64_n);
    char *jwk = ph_alloc(jwk_cap);
    if (!jwk) { ph_free(b64_n); return PH_ERR; }
    snprintf(jwk, jwk_cap,
        "{\"e\":\"AQAB\",\"kty\":\"RSA\",\"n\":\"%s\"}", b64_n);
    ph_free(b64_n);

    *out_jwk = jwk;
    return PH_OK;
}

/* ---- JWK thumbprint ---- */

ph_result_t ph_acme_jwk_thumbprint(const char *key_path,
                                     char **out_thumbprint,
                                     ph_error_t **err) {
    if (!key_path || !out_thumbprint) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                "ph_acme_jwk_thumbprint: NULL argument");
        return PH_ERR;
    }

    /* get the canonical JWK JSON */
    char *jwk = NULL;
    if (ph_acme_jwk_json(key_path, &jwk, err) != PH_OK)
        return PH_ERR;

    /* SHA-256 of the JWK JSON */
    char tmp_jwk[] = "/tmp/ph_jwk_json_XXXXXX";
    int jwk_fd = mkstemp(tmp_jwk);
    if (jwk_fd < 0) { ph_free(jwk); return PH_ERR; }
    write(jwk_fd, jwk, strlen(jwk));
    close(jwk_fd);
    ph_free(jwk);

    char tmp_hash[] = "/tmp/ph_jwk_hash_XXXXXX";
    int hash_fd = mkstemp(tmp_hash);
    if (hash_fd < 0) { unlink(tmp_jwk); return PH_ERR; }
    close(hash_fd);

    {
        /* audit fix: direct argv execvp via exec_to_file, no shell. */
        ph_argv_builder_t b;
        if (ph_argv_init(&b, 8) != PH_OK) {
            unlink(tmp_jwk);
            unlink(tmp_hash);
            return PH_ERR;
        }
        ph_argv_push(&b, "openssl");
        ph_argv_push(&b, "dgst");
        ph_argv_push(&b, "-sha256");
        ph_argv_push(&b, "-binary");
        ph_argv_push(&b, tmp_jwk);
        char **argv = ph_argv_finalize(&b);
        if (!argv) {
            unlink(tmp_jwk);
            unlink(tmp_hash);
            return PH_ERR;
        }

        int exit_code = 0;
        exec_to_file(argv, tmp_hash, &exit_code);
        ph_argv_free(argv);
    }

    unlink(tmp_jwk);

    /* read hash */
    uint8_t *hash_data = NULL;
    size_t hash_len = 0;
    if (ph_fs_read_file(tmp_hash, &hash_data, &hash_len) != PH_OK ||
        hash_len != 32) {
        ph_free(hash_data);
        unlink(tmp_hash);
        if (err)
            *err = ph_error_createf(PH_ERR_PROCESS, 0,
                "SHA-256 hash of JWK failed");
        return PH_ERR;
    }
    unlink(tmp_hash);

    *out_thumbprint = ph_acme_base64url_encode(hash_data, hash_len);
    ph_free(hash_data);

    if (!*out_thumbprint) return PH_ERR;
    return PH_OK;
}

#endif /* PHOSPHOR_HAS_LIBCURL */

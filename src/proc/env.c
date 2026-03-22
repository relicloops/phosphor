#include "phosphor/proc.h"
#include "phosphor/alloc.h"

#include <string.h>

extern char **environ;

static const char *const system_allowlist[] = {
    "PATH",
    "HOME",
    "USER",
    "LOGNAME",
    "LANG",
    "LC_ALL",
    "LC_COLLATE",
    "LC_CTYPE",
    "LC_MESSAGES",
    "LC_MONETARY",
    "LC_NUMERIC",
    "LC_TIME",
    "TERM",
    "SHELL",
    "TMPDIR",
    "XDG_RUNTIME_DIR",
    NULL
};

/* extract key length from "KEY=value" entry */
static size_t key_len(const char *entry) {
    const char *eq = strchr(entry, '=');
    return eq ? (size_t)(eq - entry) : strlen(entry);
}

/* check if key matches an exact name */
static bool match_exact(const char *key, size_t klen, const char *name) {
    return strlen(name) == klen && memcmp(key, name, klen) == 0;
}

/* check if key starts with prefix */
static bool match_prefix(const char *key, size_t klen, const char *prefix) {
    size_t plen = strlen(prefix);
    return klen >= plen && memcmp(key, prefix, plen) == 0;
}

static bool is_allowed(const char *entry, const char *const extras[]) {
    size_t klen = key_len(entry);

    /* PHOSPHOR_ prefix always allowed */
    if (match_prefix(entry, klen, "PHOSPHOR_")) return true;

    /* system allowlist (exact match) */
    for (const char *const *p = system_allowlist; *p; p++) {
        if (match_exact(entry, klen, *p)) return true;
    }

    /* caller extras */
    if (extras) {
        for (const char *const *p = extras; *p; p++) {
            const char *extra = *p;
            size_t elen = strlen(extra);
            /* trailing _ means prefix match */
            if (elen > 0 && extra[elen - 1] == '_') {
                if (match_prefix(entry, klen, extra)) return true;
            } else {
                if (match_exact(entry, klen, extra)) return true;
            }
        }
    }

    return false;
}

ph_result_t ph_env_build(const char *const extras[], ph_env_t *out) {
    if (!out) return PH_ERR;

    *out = (ph_env_t){ .entries = NULL, .count = 0 };

    if (!environ) {
        /* empty environment -- allocate just the NULL terminator */
        out->entries = ph_calloc(1, sizeof(char *));
        if (!out->entries) return PH_ERR;
        return PH_OK;
    }

    /* count environ entries */
    size_t total = 0;
    for (char **e = environ; *e; e++) total++;

    /* allocate worst-case (all pass + NULL terminator) */
    char **buf = ph_calloc(total + 1, sizeof(char *));
    if (!buf) return PH_ERR;

    size_t count = 0;
    for (char **e = environ; *e; e++) {
        if (is_allowed(*e, extras)) {
            buf[count] = ph_alloc(strlen(*e) + 1);
            if (!buf[count]) {
                /* cleanup on allocation failure */
                for (size_t i = 0; i < count; i++) ph_free(buf[i]);
                ph_free(buf);
                return PH_ERR;
            }
            strcpy(buf[count], *e);
            count++;
        }
    }
    buf[count] = NULL;

    out->entries = buf;
    out->count   = count;
    return PH_OK;
}

ph_result_t ph_env_set(ph_env_t *env, const char *key, const char *value) {
    if (!env || !key || !value) return PH_ERR;

    size_t klen = strlen(key);
    size_t vlen = strlen(value);
    size_t elen = klen + 1 + vlen + 1;    /* "KEY=VALUE\0" */

    char *entry = ph_alloc(elen);
    if (!entry) return PH_ERR;
    memcpy(entry, key, klen);
    entry[klen] = '=';
    memcpy(entry + klen + 1, value, vlen);
    entry[elen - 1] = '\0';

    /* scan for existing key */
    if (env->entries) {
        for (size_t i = 0; i < env->count; i++) {
            size_t existing_klen = key_len(env->entries[i]);
            if (existing_klen == klen &&
                memcmp(env->entries[i], key, klen) == 0) {
                /* replace */
                ph_free(env->entries[i]);
                env->entries[i] = entry;
                return PH_OK;
            }
        }
    }

    /* grow array: count + new entry + NULL terminator */
    char **grown = ph_realloc(env->entries,
                              (env->count + 2) * sizeof(char *));
    if (!grown) {
        ph_free(entry);
        return PH_ERR;
    }

    grown[env->count]     = entry;
    grown[env->count + 1] = NULL;
    env->entries = grown;
    env->count++;
    return PH_OK;
}

void ph_env_destroy(ph_env_t *env) {
    if (!env || !env->entries) return;

    for (size_t i = 0; i < env->count; i++) {
        ph_free(env->entries[i]);
    }
    ph_free(env->entries);
    env->entries = NULL;
    env->count   = 0;
}

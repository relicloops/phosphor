#include "phosphor/proc.h"
#include "phosphor/alloc.h"
#include "phosphor/signal.h"
#include "phosphor/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* --- argv builder ------------------------------------------------------- */

ph_result_t ph_argv_init(ph_argv_builder_t *b, size_t initial_cap) {
    if (!b) return PH_ERR;
    if (initial_cap == 0) initial_cap = 4;

    /* +1 for NULL terminator */
    b->items = ph_calloc(initial_cap + 1, sizeof(char *));
    if (!b->items) return PH_ERR;

    b->count = 0;
    b->cap   = initial_cap;
    return PH_OK;
}

ph_result_t ph_argv_push(ph_argv_builder_t *b, const char *arg) {
    if (!b || !b->items || !arg) return PH_ERR;

    if (b->count >= b->cap) {
        size_t new_cap = b->cap * 2;
        char **grown = ph_realloc(b->items, (new_cap + 1) * sizeof(char *));
        if (!grown) return PH_ERR;
        b->items = grown;
        b->cap   = new_cap;
    }

    size_t len = strlen(arg);
    b->items[b->count] = ph_alloc(len + 1);
    if (!b->items[b->count]) return PH_ERR;
    memcpy(b->items[b->count], arg, len + 1);
    b->count++;
    b->items[b->count] = NULL;    /* keep NULL-terminated */
    return PH_OK;
}

ph_result_t ph_argv_pushf(ph_argv_builder_t *b, const char *fmt, ...) {
    if (!b || !fmt) return PH_ERR;

    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) return PH_ERR;

    char *buf = ph_alloc((size_t)needed + 1);
    if (!buf) return PH_ERR;

    va_start(ap, fmt);
    vsnprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    /* push takes ownership via copy, so we push then free our temp */
    ph_result_t rc = ph_argv_push(b, buf);
    ph_free(buf);
    return rc;
}

char **ph_argv_finalize(ph_argv_builder_t *b) {
    if (!b || !b->items) return NULL;

    char **result = b->items;

    /* invalidate builder */
    b->items = NULL;
    b->count = 0;
    b->cap   = 0;

    return result;
}

void ph_argv_free(char **argv) {
    if (!argv) return;
    for (char **p = argv; *p; p++) {
        ph_free(*p);
    }
    ph_free(argv);
}

void ph_argv_destroy(ph_argv_builder_t *b) {
    if (!b) return;
    if (b->items) {
        ph_argv_free(b->items);
        b->items = NULL;
    }
    b->count = 0;
    b->cap   = 0;
}

/* --- process execution -------------------------------------------------- */

ph_result_t ph_proc_exec(const ph_proc_opts_t *opts, int *out_exit) {
    if (!opts || !opts->argv || !out_exit) return PH_ERR;

    if (opts->timeout_sec > 0) {
        ph_log_warn("timeout_sec=%d ignored (not implemented in v1)",
                    opts->timeout_sec);
    }

    const char *const *envp = NULL;
    if (opts->env && opts->env->entries) {
        envp = (const char *const *)opts->env->entries;
    }

    ph_proc_result_t result;
    ph_result_t rc = ph_proc_spawn((const char *const *)opts->argv,
                                    envp, opts->cwd, &result);
    if (rc != PH_OK) return PH_ERR;

    if (ph_signal_interrupted()) {
        *out_exit = 8;
        return PH_OK;
    }

    *out_exit = ph_proc_map_exit(&result);
    return PH_OK;
}

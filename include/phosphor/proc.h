#ifndef PHOSPHOR_PROC_H
#define PHOSPHOR_PROC_H

#include "phosphor/types.h"
#include "phosphor/platform.h"

/* --- environment -------------------------------------------------------- */

/*
 * ph_env_t -- sanitized environment for child processes.
 *
 * ownership:
 *   entries  -- owner: self (heap-allocated, NULL-terminated for execve)
 *   each entry string is heap-allocated and owned by this struct.
 */
typedef struct {
    char  **entries;
    size_t  count;
} ph_env_t;

ph_result_t ph_env_build(const char *const extras[], ph_env_t *out);
ph_result_t ph_env_set(ph_env_t *env, const char *key, const char *value);
void        ph_env_destroy(ph_env_t *env);

/* --- exit code mapping -------------------------------------------------- */

/*
 * map a raw child exit result to a phosphor exit code (0-8).
 *
 *   0       -> 0  (success)
 *   1       -> 1  (general)
 *   2-7     -> 2-7  (direct pass-through)
 *   8+      -> 1  (general/unmapped)
 *   128+    -> 8  (signal)
 *   signaled -> 8  (signal)
 *   NULL    -> 1  (general)
 */
int ph_proc_map_exit(const ph_proc_result_t *result);

/* --- argv builder ------------------------------------------------------- */

/*
 * ph_argv_builder_t -- dynamic argv construction.
 *
 * ownership:
 *   items  -- owner: self (heap-allocated, NULL-terminated)
 *   each string is heap-allocated and owned by this struct until finalize.
 */
typedef struct {
    char  **items;
    size_t  count;
    size_t  cap;
} ph_argv_builder_t;

ph_result_t  ph_argv_init(ph_argv_builder_t *b, size_t initial_cap);
ph_result_t  ph_argv_push(ph_argv_builder_t *b, const char *arg);

PH_PRINTF(2, 3)
ph_result_t  ph_argv_pushf(ph_argv_builder_t *b, const char *fmt, ...);

/*
 * finalize: transfers ownership of the argv array to the caller.
 * the builder is invalidated after this call.
 * returns the NULL-terminated argv array.
 */
char       **ph_argv_finalize(ph_argv_builder_t *b);

/* free a finalized argv array (NULL-safe) */
void         ph_argv_free(char **argv);

/* destroy a builder that was NOT finalized (NULL-safe) */
void         ph_argv_destroy(ph_argv_builder_t *b);

/* --- process execution -------------------------------------------------- */

/*
 * ph_proc_opts_t -- options for ph_proc_exec.
 *
 * ownership: all pointers are borrowed (caller retains ownership).
 */
typedef struct {
    char       **argv;
    const char  *cwd;
    ph_env_t    *env;
    int          timeout_sec;    /* reserved for future use; ignored in v1 */
} ph_proc_opts_t;

/*
 * execute a child process with full pipeline:
 *   1. extract envp from opts->env (NULL = inherit parent)
 *   2. spawn via ph_proc_spawn
 *   3. check ph_signal_interrupted() after wait
 *   4. map exit code via ph_proc_map_exit
 *
 * returns PH_OK if child was spawned and waited on (regardless of child exit).
 * returns PH_ERR if the spawn itself failed.
 * mapped exit code is written to *out_exit.
 */
ph_result_t ph_proc_exec(const ph_proc_opts_t *opts, int *out_exit);

#endif /* PHOSPHOR_PROC_H */

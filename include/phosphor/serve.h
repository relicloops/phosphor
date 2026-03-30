#ifndef PHOSPHOR_SERVE_H
#define PHOSPHOR_SERVE_H

#include "phosphor/types.h"
#include "phosphor/error.h"

#include <stdbool.h>
#include <sys/types.h>

/* ---- neonsignal process configuration ---- */

typedef struct {
    const char *bin_path;       /* NULL = search PATH for "neonsignal" */
    int         threads;        /* 0 = default (3) */
    const char *host;           /* NULL = default (0.0.0.0) */
    int         port;           /* 0 = default (9443) */
    const char *www_root;       /* NULL = default (public) */
    const char *certs_root;     /* NULL = default (certs) */
    const char *working_dir;    /* NULL = inherit cwd */
    const char *upload_dir;     /* NULL = omit */
    const char *augments_dir;   /* NULL = omit */
    const char *grafts_dir;     /* NULL = omit */
    bool        watch;          /* start file watcher process */
    const char *watch_cmd;      /* shell command; NULL = default */
    const char *deploy_dir;     /* deploy public_dir for watcher; NULL = omit */
} ph_serve_ns_config_t;

/* ---- neonsignal_redirect process configuration ---- */

typedef struct {
    const char *bin_path;       /* NULL = search PATH for "neonsignal_redirect" */
    int         instances;      /* 0 = default (2) */
    const char *host;           /* NULL = default (0.0.0.0) */
    int         port;           /* 0 = default (9090) */
    int         target_port;    /* 0 = default (443) */
    const char *acme_webroot;   /* NULL = omit */
    const char *working_dir;    /* NULL = inherit cwd */
} ph_serve_redir_config_t;

/* ---- combined serve configuration ---- */

typedef struct {
    ph_serve_ns_config_t     ns;
    ph_serve_redir_config_t  redir;
    bool                     skip_redirect;  /* true = only spawn neonsignal */
    bool                     verbose;
    bool                     capture_output; /* pipe child I/O for dashboard */
} ph_serve_config_t;

/* ---- serve session (opaque) ---- */

typedef struct ph_serve_session ph_serve_session_t;

/* ---- API ---- */

/*
 * ph_serve_check_binaries -- verify required executables exist.
 *
 * checks neonsignal (and redirect unless skip_redirect) are accessible.
 * if bin_path is set, checks that specific path; otherwise searches PATH.
 * returns PH_OK if all found, PH_ERR with diagnostic in *err.
 */
ph_result_t ph_serve_check_binaries(const ph_serve_config_t *cfg,
                                     ph_error_t **err);

/*
 * ph_serve_start -- spawn neonsignal (and optionally redirect).
 *
 * both processes receive "spin" as the first argument.
 * children are isolated in their own process groups.
 * caller must call ph_serve_wait() or ph_serve_stop(), then ph_serve_destroy().
 */
ph_result_t ph_serve_start(const ph_serve_config_t *cfg,
                            ph_serve_session_t **out,
                            ph_error_t **err);

/*
 * ph_serve_wait -- block until all children exit (or signal received).
 *
 * forwards SIGINT/SIGTERM to child process groups.
 * returns the worst (highest) child exit code, or 8 if signaled.
 */
int ph_serve_wait(ph_serve_session_t *session);

/*
 * ph_serve_stop -- send SIGTERM to all children and wait for exit.
 */
void ph_serve_stop(ph_serve_session_t *session);

/*
 * ph_serve_destroy -- free session resources.
 *
 * must call after ph_serve_wait() or ph_serve_stop().
 */
void ph_serve_destroy(ph_serve_session_t *session);

/* ---- accessors for dashboard integration ---- */

/* pipe read-end fds (-1 if not captured or already closed) */
int   ph_serve_ns_stdout_fd(const ph_serve_session_t *s);
int   ph_serve_ns_stderr_fd(const ph_serve_session_t *s);
int   ph_serve_redir_stdout_fd(const ph_serve_session_t *s);
int   ph_serve_redir_stderr_fd(const ph_serve_session_t *s);
int   ph_serve_watch_stdout_fd(const ph_serve_session_t *s);
int   ph_serve_watch_stderr_fd(const ph_serve_session_t *s);

/* child pids (0 if not spawned or already exited) */
pid_t ph_serve_ns_pid(const ph_serve_session_t *s);
pid_t ph_serve_redir_pid(const ph_serve_session_t *s);
pid_t ph_serve_watch_pid(const ph_serve_session_t *s);

/* child count */
int   ph_serve_child_count(const ph_serve_session_t *s);

#endif /* PHOSPHOR_SERVE_H */

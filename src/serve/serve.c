#include "phosphor/serve.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/proc.h"
#include "phosphor/signal.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- session ---- */

/* default watch command for cathode/phosphor projects.
 * argv form so we can spawn it without sh -c, which keeps deploy_dir from
 * being interpreted as shell text when it contains spaces or metacharacters. */
static const char *const PH_DEFAULT_WATCH_ARGV[] = {
    "node", "scripts/_default/build.mjs", "--watch", NULL
};

struct ph_serve_session {
    pid_t ns_pid;
    pid_t redir_pid;
    pid_t watch_pid;
    int   child_count;
    /* pipe read-end fds for dashboard (-1 = not connected) */
    int   ns_stdout_fd;
    int   ns_stderr_fd;
    int   redir_stdout_fd;
    int   redir_stderr_fd;
    int   watch_stdout_fd;
    int   watch_stderr_fd;
};

/* ---- binary guard ---- */

static bool find_in_path(const char *name) {
    const char *path = getenv("PATH");
    if (!path) return false;

    size_t nlen = strlen(name);
    const char *p = path;

    while (*p) {
        const char *sep = strchr(p, ':');
        size_t dlen = sep ? (size_t)(sep - p) : strlen(p);

        if (dlen > 0 && dlen + nlen + 2 < 4096) {
            char buf[4096];
            memcpy(buf, p, dlen);
            buf[dlen] = '/';
            memcpy(buf + dlen + 1, name, nlen + 1);
            if (access(buf, X_OK) == 0)
                return true;
        }

        if (!sep) break;
        p = sep + 1;
    }
    return false;
}

/*
 * audit fix (2026-04-08T11-07-17Z): return true if `s` is a bare
 * binary name (non-empty, no slash). Bare names must resolve via
 * PATH rather than the filesystem -- previously any non-NULL
 * bin_path fell through to access(), which only looked in the
 * current working directory and broke --neonsignal-bin=neonsignal
 * and manifest values like `bin = "neonsignal"`.
 */
static bool is_bare_name(const char *s) {
    return s && *s && strchr(s, '/') == NULL;
}

ph_result_t ph_serve_check_binaries(const ph_serve_config_t *cfg,
                                     ph_error_t **err) {
    if (!cfg) return PH_ERR;

    /* check neonsignal: bare names route through PATH, anything
     * with a slash (absolute or relative) is a filesystem path. */
    const char *ns_bin = cfg->ns.bin_path;
    if (ns_bin && !is_bare_name(ns_bin)) {
        if (access(ns_bin, X_OK) != 0) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                    "neonsignal not found at '%s'", ns_bin);
            return PH_ERR;
        }
    } else {
        const char *name = ns_bin ? ns_bin : "neonsignal";
        if (!find_in_path(name)) {
            if (err)
                *err = ph_error_createf(PH_ERR_CONFIG, 0,
                    "neonsignal not found in PATH; "
                    "use --neonsignal-bin=<path>");
            return PH_ERR;
        }
    }

    /* check redirect */
    if (!cfg->skip_redirect) {
        const char *redir_bin = cfg->redir.bin_path;
        if (redir_bin && !is_bare_name(redir_bin)) {
            if (access(redir_bin, X_OK) != 0) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "neonsignal_redirect not found at '%s'",
                        redir_bin);
                return PH_ERR;
            }
        } else {
            const char *name = redir_bin ? redir_bin : "neonsignal_redirect";
            if (!find_in_path(name)) {
                if (err)
                    *err = ph_error_createf(PH_ERR_CONFIG, 0,
                        "neonsignal_redirect not found in PATH; "
                        "use --redirect-bin=<path>");
                return PH_ERR;
            }
        }
    }

    return PH_OK;
}

/* ---- argv builders ---- */

static char **build_ns_argv(const ph_serve_ns_config_t *ns) {
    ph_argv_builder_t ab;
    if (ph_argv_init(&ab, 16) != PH_OK) return NULL;

    ph_argv_push(&ab, ns->bin_path ? ns->bin_path : "neonsignal");
    ph_argv_push(&ab, "spin");

    if (ns->threads > 0)
        ph_argv_pushf(&ab, "--threads=%d", ns->threads);
    if (ns->host)
        ph_argv_pushf(&ab, "--host=%s", ns->host);
    if (ns->port > 0)
        ph_argv_pushf(&ab, "--port=%d", ns->port);
    if (ns->www_root)
        ph_argv_pushf(&ab, "--www-root=%s", ns->www_root);
    if (ns->certs_root)
        ph_argv_pushf(&ab, "--certs-root=%s", ns->certs_root);
    if (ns->working_dir)
        ph_argv_pushf(&ab, "--working-dir=%s", ns->working_dir);
    if (ns->upload_dir)
        ph_argv_pushf(&ab, "--upload-dir=%s", ns->upload_dir);
    if (ns->augments_dir)
        ph_argv_pushf(&ab, "--augments-dir=%s", ns->augments_dir);
    if (ns->grafts_dir)
        ph_argv_pushf(&ab, "--grafts-dir=%s", ns->grafts_dir);

    /* logging flags */
    if (ns->enable_debug)
        ph_argv_push(&ab, "--enable-debug");
    if (ns->enable_log)
        ph_argv_push(&ab, "--enable-log");
    if (ns->enable_log_color)
        ph_argv_push(&ab, "--enable-log-color");
    if (ns->enable_file_log)
        ph_argv_push(&ab, "--enable-file-log");
    if (ns->log_directory)
        ph_argv_pushf(&ab, "--log-directory=%s", ns->log_directory);
    if (ns->disable_proxies_check)
        ph_argv_push(&ab, "--disable-proxies-check");

    return ph_argv_finalize(&ab);
}

static char **build_redir_argv(const ph_serve_redir_config_t *redir) {
    ph_argv_builder_t ab;
    if (ph_argv_init(&ab, 12) != PH_OK) return NULL;

    ph_argv_push(&ab, redir->bin_path ? redir->bin_path
                                       : "neonsignal_redirect");
    ph_argv_push(&ab, "spin");

    if (redir->instances > 0)
        ph_argv_pushf(&ab, "--instances=%d", redir->instances);
    if (redir->host)
        ph_argv_pushf(&ab, "--host=%s", redir->host);
    if (redir->port > 0)
        ph_argv_pushf(&ab, "--port=%d", redir->port);
    if (redir->target_port > 0)
        ph_argv_pushf(&ab, "--target-port=%d", redir->target_port);
    if (redir->acme_webroot)
        ph_argv_pushf(&ab, "--acme-webroot=%s", redir->acme_webroot);
    if (redir->working_dir)
        ph_argv_pushf(&ab, "--working-dir=%s", redir->working_dir);

    return ph_argv_finalize(&ab);
}

static void log_argv(const char *label, char **argv) {
    if (!argv) return;
    ph_log_debug("serve: %s argv:", label);
    for (int i = 0; argv[i]; i++)
        ph_log_debug("  [%d] %s", i, argv[i]);
}

/* ---- helpers ---- */

static void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* ---- spawn ---- */

/*
 * spawn_child -- fork+exec with optional pipe capture.
 *
 * if out_stdout_fd / out_stderr_fd are non-NULL, child stdout/stderr are
 * redirected to pipes and the read-end fds are returned (set non-blocking).
 * pass NULL to inherit the parent's streams (legacy behavior).
 */
static pid_t spawn_child(char **argv, const char *cwd,
                         int *out_stdout_fd, int *out_stderr_fd) {
    if (!argv || !argv[0]) return -1;

    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};

    if (out_stdout_fd) {
        if (pipe(out_pipe) < 0) {
            ph_log_error("serve: pipe(stdout): %s", strerror(errno));
            return -1;
        }
    }
    if (out_stderr_fd) {
        if (pipe(err_pipe) < 0) {
            ph_log_error("serve: pipe(stderr): %s", strerror(errno));
            if (out_pipe[0] >= 0) { close(out_pipe[0]); close(out_pipe[1]); }
            return -1;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        ph_log_error("serve: fork: %s", strerror(errno));
        if (out_pipe[0] >= 0) { close(out_pipe[0]); close(out_pipe[1]); }
        if (err_pipe[0] >= 0) { close(err_pipe[0]); close(err_pipe[1]); }
        return -1;
    }

    if (pid == 0) {
        /* child: isolate into own process group */
        setpgid(0, 0);

        /* redirect stdout/stderr to pipes if requested */
        if (out_pipe[1] >= 0) {
            dup2(out_pipe[1], STDOUT_FILENO);
            close(out_pipe[0]);
            close(out_pipe[1]);
        }
        if (err_pipe[1] >= 0) {
            dup2(err_pipe[1], STDERR_FILENO);
            close(err_pipe[0]);
            close(err_pipe[1]);
        }

        if (cwd && chdir(cwd) != 0)
            _exit(127);
        execvp(argv[0], argv);
        _exit(127);
    }

    /* parent: close write ends, set read ends non-blocking */
    if (out_pipe[1] >= 0) close(out_pipe[1]);
    if (err_pipe[1] >= 0) close(err_pipe[1]);

    if (out_stdout_fd) {
        set_nonblock(out_pipe[0]);
        *out_stdout_fd = out_pipe[0];
    }
    if (out_stderr_fd) {
        set_nonblock(err_pipe[0]);
        *out_stderr_fd = err_pipe[0];
    }

    /* parent: set child's pgid (race avoidance) */
    if (setpgid(pid, pid) < 0 && errno != EACCES && errno != ESRCH)
        ph_log_debug("serve: setpgid(parent): %s", strerror(errno));

    return pid;
}

ph_result_t ph_serve_start(const ph_serve_config_t *cfg,
                            ph_serve_session_t **out,
                            ph_error_t **err) {
    if (!cfg || !out) return PH_ERR;

    ph_serve_session_t *s = ph_calloc(1, sizeof(*s));
    if (!s) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "serve: allocation failed");
        return PH_ERR;
    }
    s->ns_pid = 0;
    s->redir_pid = 0;
    s->watch_pid = 0;
    s->child_count = 0;
    s->ns_stdout_fd = -1;
    s->ns_stderr_fd = -1;
    s->redir_stdout_fd = -1;
    s->redir_stderr_fd = -1;
    s->watch_stdout_fd = -1;
    s->watch_stderr_fd = -1;

    bool cap = cfg->capture_output;

    /* build and spawn neonsignal */
    char **ns_argv = build_ns_argv(&cfg->ns);
    if (!ns_argv) {
        ph_free(s);
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "serve: failed to build neonsignal argv");
        return PH_ERR;
    }

    if (cfg->verbose)
        log_argv("neonsignal", ns_argv);

    s->ns_pid = spawn_child(ns_argv, cfg->ns.working_dir,
                             cap ? &s->ns_stdout_fd : NULL,
                             cap ? &s->ns_stderr_fd : NULL);
    ph_argv_free(ns_argv);

    if (s->ns_pid < 0) {
        ph_free(s);
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "serve: failed to spawn neonsignal");
        return PH_ERR;
    }
    s->child_count++;
    ph_log_info("serve: neonsignal started (pid %d)", (int)s->ns_pid);

    /* build and spawn redirect */
    if (!cfg->skip_redirect) {
        char **redir_argv = build_redir_argv(&cfg->redir);
        if (!redir_argv) {
            /* kill neonsignal before bailing */
            kill(-(s->ns_pid), SIGTERM);
            waitpid(s->ns_pid, NULL, 0);
            ph_free(s);
            if (err)
                *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                    "serve: failed to build redirect argv");
            return PH_ERR;
        }

        if (cfg->verbose)
            log_argv("neonsignal_redirect", redir_argv);

        s->redir_pid = spawn_child(redir_argv, cfg->redir.working_dir,
                                    cap ? &s->redir_stdout_fd : NULL,
                                    cap ? &s->redir_stderr_fd : NULL);
        ph_argv_free(redir_argv);

        if (s->redir_pid < 0) {
            kill(-(s->ns_pid), SIGTERM);
            waitpid(s->ns_pid, NULL, 0);
            ph_free(s);
            if (err)
                *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                    "serve: failed to spawn neonsignal_redirect");
            return PH_ERR;
        }
        s->child_count++;
        ph_log_info("serve: neonsignal_redirect started (pid %d)",
                     (int)s->redir_pid);
    }

    /* spawn file watcher */
    if (cfg->ns.watch) {
        ph_argv_builder_t wb;
        if (ph_argv_init(&wb, 8) != PH_OK) {
            ph_serve_stop(s);
            ph_free(s);
            if (err)
                *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                    "serve: failed to build watch argv");
            return PH_ERR;
        }

        /* audit fix: build the default watcher as explicit argv. previously
         * we snprintf'd "<cmd> --deploy <deploy_dir>" into a buffer and ran
         * it via sh -c, which interpreted any spaces/metacharacters in
         * deploy_dir (a manifest-controlled value) as shell text. argv keeps
         * deploy_dir verbatim for the child. user-supplied watch_cmd still
         * goes through sh -c because it is opt-in shell input. */
        if (cfg->ns.watch_cmd) {
            ph_argv_push(&wb, "sh");
            ph_argv_push(&wb, "-c");
            ph_argv_push(&wb, cfg->ns.watch_cmd);
        } else {
            for (size_t i = 0; PH_DEFAULT_WATCH_ARGV[i]; i++)
                ph_argv_push(&wb, PH_DEFAULT_WATCH_ARGV[i]);
            if (cfg->ns.deploy_dir) {
                ph_argv_push(&wb, "--deploy");
                ph_argv_push(&wb, cfg->ns.deploy_dir);
            }
        }

        char **watch_argv = ph_argv_finalize(&wb);
        if (!watch_argv) {
            ph_serve_stop(s);
            ph_free(s);
            if (err)
                *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                    "serve: failed to build watch argv");
            return PH_ERR;
        }

        if (cfg->verbose)
            log_argv("watch", watch_argv);

        s->watch_pid = spawn_child(watch_argv, cfg->ns.working_dir,
                                    cap ? &s->watch_stdout_fd : NULL,
                                    cap ? &s->watch_stderr_fd : NULL);
        ph_argv_free(watch_argv);

        if (s->watch_pid < 0) {
            ph_serve_stop(s);
            ph_free(s);
            if (err)
                *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                    "serve: failed to spawn watch process");
            return PH_ERR;
        }
        s->child_count++;
        ph_log_info("serve: watch started (pid %d)", (int)s->watch_pid);
    }

    /* install signal handlers for clean forwarding */
    ph_signal_install();

    *out = s;
    return PH_OK;
}

/* ---- wait ---- */

int ph_serve_wait(ph_serve_session_t *session) {
    if (!session || session->child_count == 0) return 0;

    int worst_exit = 0;
    int remaining = session->child_count;
    /* audit fix (finding 10): latch signal state so we can return
     * PH_ERR_SIGNAL instead of a raw child exit code. The SIGTERM
     * forward to children stays unchanged; only the final return
     * value is normalized. */
    bool signaled = false;

    while (remaining > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);

        if (pid < 0) {
            if (errno == EINTR) {
                /* signal received -- forward SIGTERM to all children */
                signaled = true;
                if (session->ns_pid > 0)
                    kill(-(session->ns_pid), SIGTERM);
                if (session->redir_pid > 0)
                    kill(-(session->redir_pid), SIGTERM);
                if (session->watch_pid > 0)
                    kill(-(session->watch_pid), SIGTERM);
                continue;
            }
            if (errno == ECHILD) break;
            ph_log_error("serve: waitpid: %s", strerror(errno));
            break;
        }

        int code = 0;
        if (WIFEXITED(status)) {
            code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            code = 128 + WTERMSIG(status);
        }

        /* audit fix: if an "authoritative" child (neonsignal, or the
         * redirect when it is enabled) exits, tear down the rest of the
         * stack instead of waiting on survivors. previously the loop just
         * decremented `remaining`, leaving the watcher / redirect running
         * after neonsignal died and making `phosphor serve` look hung. */
        bool authoritative_exit = false;

        if (pid == session->ns_pid) {
            ph_log_info("serve: neonsignal exited (code %d)", code);
            session->ns_pid = 0;
            remaining--;
            authoritative_exit = true;
        } else if (pid == session->redir_pid) {
            ph_log_info("serve: neonsignal_redirect exited (code %d)", code);
            session->redir_pid = 0;
            remaining--;
            authoritative_exit = true;
        } else if (pid == session->watch_pid) {
            ph_log_info("serve: watch exited (code %d)", code);
            session->watch_pid = 0;
            remaining--;
            /* the watcher is a helper -- its exit alone does not collapse
             * the stack. fall through without setting authoritative_exit. */
        }

        if (code > worst_exit) worst_exit = code;

        if (authoritative_exit && remaining > 0) {
            ph_serve_stop(session);
            /* ph_serve_stop reaped what it killed and zeroed the pids. drain
             * any pre-existing zombies that might still be queued, then exit
             * the loop. */
            remaining = 0;
        }
    }

    return signaled ? PH_ERR_SIGNAL : worst_exit;
}

/* ---- fd cleanup ---- */

static void close_if_open(int *fd) {
    if (*fd >= 0) { close(*fd); *fd = -1; }
}

static void close_all_fds(ph_serve_session_t *s) {
    close_if_open(&s->ns_stdout_fd);
    close_if_open(&s->ns_stderr_fd);
    close_if_open(&s->redir_stdout_fd);
    close_if_open(&s->redir_stderr_fd);
    close_if_open(&s->watch_stdout_fd);
    close_if_open(&s->watch_stderr_fd);
}

/* ---- stop ---- */

void ph_serve_stop(ph_serve_session_t *session) {
    if (!session) return;

    if (session->ns_pid > 0) {
        kill(-(session->ns_pid), SIGTERM);
        waitpid(session->ns_pid, NULL, 0);
        ph_log_debug("serve: neonsignal stopped");
        session->ns_pid = 0;
    }
    if (session->redir_pid > 0) {
        kill(-(session->redir_pid), SIGTERM);
        waitpid(session->redir_pid, NULL, 0);
        ph_log_debug("serve: neonsignal_redirect stopped");
        session->redir_pid = 0;
    }
    if (session->watch_pid > 0) {
        kill(-(session->watch_pid), SIGTERM);
        waitpid(session->watch_pid, NULL, 0);
        ph_log_debug("serve: watch stopped");
        session->watch_pid = 0;
    }
    session->child_count = 0;
}

/* ---- destroy ---- */

void ph_serve_destroy(ph_serve_session_t *session) {
    if (!session) return;
    /* stop any remaining children */
    ph_serve_stop(session);
    close_all_fds(session);
    ph_free(session);
}

/* ---- accessors ---- */

int   ph_serve_ns_stdout_fd(const ph_serve_session_t *s)    { return s ? s->ns_stdout_fd : -1; }
int   ph_serve_ns_stderr_fd(const ph_serve_session_t *s)    { return s ? s->ns_stderr_fd : -1; }
int   ph_serve_redir_stdout_fd(const ph_serve_session_t *s) { return s ? s->redir_stdout_fd : -1; }
int   ph_serve_redir_stderr_fd(const ph_serve_session_t *s) { return s ? s->redir_stderr_fd : -1; }
int   ph_serve_watch_stdout_fd(const ph_serve_session_t *s) { return s ? s->watch_stdout_fd : -1; }
int   ph_serve_watch_stderr_fd(const ph_serve_session_t *s) { return s ? s->watch_stderr_fd : -1; }

pid_t ph_serve_ns_pid(const ph_serve_session_t *s)          { return s ? s->ns_pid : 0; }
pid_t ph_serve_redir_pid(const ph_serve_session_t *s)       { return s ? s->redir_pid : 0; }
pid_t ph_serve_watch_pid(const ph_serve_session_t *s)       { return s ? s->watch_pid : 0; }

int   ph_serve_child_count(const ph_serve_session_t *s)     { return s ? s->child_count : 0; }

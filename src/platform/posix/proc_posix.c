#include "phosphor/platform.h"
#include "phosphor/log.h"

#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

extern char **environ;

ph_result_t ph_proc_spawn(const char *const argv[],
                           const char *const envp[],
                           const char *cwd,
                           ph_proc_result_t *out) {
    if (!argv || !argv[0] || !out) return PH_ERR;

    *out = (ph_proc_result_t){ .exit_code = -1, .signaled = false,
                                .signal_num = 0 };

    pid_t pid = fork();
    if (pid < 0) {
        ph_log_error("fork: %s", strerror(errno));
        return PH_ERR;
    }

    if (pid == 0) {
        /* child: isolate into own process group */
        setpgid(0, 0);

        if (cwd && chdir(cwd) != 0) {
            _exit(127);
        }
        if (envp) {
            environ = (char **)envp;
        }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    /* parent: set child's pgid (race avoidance -- EACCES is harmless) */
    if (setpgid(pid, pid) < 0 && errno != EACCES && errno != ESRCH) {
        ph_log_debug("setpgid(parent): %s", strerror(errno));
    }

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            /* forward SIGINT to child process group */
            kill(-pid, SIGINT);
            continue;
        }
        ph_log_error("waitpid: %s", strerror(errno));
        return PH_ERR;
    }

    if (WIFEXITED(status)) {
        out->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        out->signaled   = true;
        out->signal_num = WTERMSIG(status);
        out->exit_code  = 128 + out->signal_num;
    }

    return PH_OK;
}

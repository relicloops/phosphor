#include "phosphor/signal.h"

/* SIGWINCH is not in POSIX.1-2008 strict mode; define if missing */
#ifndef SIGWINCH
#  define SIGWINCH 28
#endif

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

static volatile sig_atomic_t g_interrupted = 0;
static volatile sig_atomic_t g_signal_num  = 0;
static volatile sig_atomic_t g_winch       = 0;

/* self-pipe for waking poll() from signal handlers */
static int g_signal_pipe[2] = {-1, -1};

static void signal_handler(int signum) {
    g_interrupted = 1;
    g_signal_num  = signum;
    /* wake poll() via self-pipe */
    if (g_signal_pipe[1] >= 0) {
        char c = 's';
        (void)write(g_signal_pipe[1], &c, 1);
    }
}

static void winch_handler(int signum) {
    (void)signum;
    g_winch = 1;
    if (g_signal_pipe[1] >= 0) {
        char c = 'w';
        (void)write(g_signal_pipe[1], &c, 1);
    }
}

void ph_signal_install(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);
}

void ph_signal_install_winch(void) {
    struct sigaction sa;
    sa.sa_handler = winch_handler;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGWINCH, &sa, NULL);
}

int ph_signal_pipe_init(void) {
    if (g_signal_pipe[0] >= 0) return g_signal_pipe[0];
    if (pipe(g_signal_pipe) < 0) return -1;
    /* non-blocking so drain() never stalls the event loop */
    fcntl(g_signal_pipe[0], F_SETFL,
          fcntl(g_signal_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(g_signal_pipe[1], F_SETFL,
          fcntl(g_signal_pipe[1], F_GETFL) | O_NONBLOCK);
    return g_signal_pipe[0];
}

int ph_signal_pipe_fd(void) {
    return g_signal_pipe[0];
}

void ph_signal_pipe_drain(void) {
    if (g_signal_pipe[0] < 0) return;
    char buf[64];
    while (read(g_signal_pipe[0], buf, sizeof(buf)) > 0)
        ;
}

bool ph_signal_interrupted(void) {
    return g_interrupted != 0;
}

int ph_signal_caught(void) {
    return (int)g_signal_num;
}

bool ph_signal_winch_pending(void) {
    return g_winch != 0;
}

void ph_signal_winch_clear(void) {
    g_winch = 0;
}

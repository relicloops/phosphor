#include "phosphor/signal.h"

#include <signal.h>

static volatile sig_atomic_t g_interrupted = 0;
static volatile sig_atomic_t g_signal_num  = 0;

static void signal_handler(int signum) {
    g_interrupted = 1;
    g_signal_num  = signum;
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

bool ph_signal_interrupted(void) {
    return g_interrupted != 0;
}

int ph_signal_caught(void) {
    return (int)g_signal_num;
}

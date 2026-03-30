#ifndef PHOSPHOR_SIGNAL_H
#define PHOSPHOR_SIGNAL_H

#include "phosphor/types.h"

/*
 * signal handling for graceful cleanup.
 *
 * - SIGINT/SIGTERM: handler sets a volatile flag; no heap work in handler.
 * - SIGPIPE: ignored (SIG_IGN) to prevent silent crashes on broken pipes.
 * - SIGWINCH: handler sets a flag for terminal resize (dashboard).
 *
 * long-running loops check ph_signal_interrupted() between iterations
 * and abort with cleanup when true.
 */

void ph_signal_install(void);
void ph_signal_install_winch(void);
bool ph_signal_interrupted(void);

/* returns the signal number that caused interruption, or 0 */
int  ph_signal_caught(void);

/* SIGWINCH (terminal resize) */
bool ph_signal_winch_pending(void);
void ph_signal_winch_clear(void);

/* self-pipe for waking poll() from signal handlers */
int  ph_signal_pipe_init(void);   /* returns read-end fd */
int  ph_signal_pipe_fd(void);     /* returns read-end fd (-1 if not init) */
void ph_signal_pipe_drain(void);  /* read and discard pipe bytes */

#endif /* PHOSPHOR_SIGNAL_H */

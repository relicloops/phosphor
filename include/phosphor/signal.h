#ifndef PHOSPHOR_SIGNAL_H
#define PHOSPHOR_SIGNAL_H

#include "phosphor/types.h"

/*
 * signal handling for graceful cleanup.
 *
 * - SIGINT/SIGTERM: handler sets a volatile flag; no heap work in handler.
 * - SIGPIPE: ignored (SIG_IGN) to prevent silent crashes on broken pipes.
 *
 * long-running loops check ph_signal_interrupted() between iterations
 * and abort with cleanup when true.
 */

void ph_signal_install(void);
bool ph_signal_interrupted(void);

/* returns the signal number that caused interruption, or 0 */
int  ph_signal_caught(void);

#endif /* PHOSPHOR_SIGNAL_H */

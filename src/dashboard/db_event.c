#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/signal.h"

#include <curses.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

void collect_events(ph_dashboard_t *db, db_event_t *events,
                    int *nevents, int sig_fd) {
    *nevents = 0;

    /* build pollfd array from panel pipe fds + signal pipe */
    struct pollfd fds[MAX_FDS];
    int nfds = 0;

    int fd_panel[MAX_FDS];
    int fd_stderr[MAX_FDS];

    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->stdout_fd >= 0) {
            fd_panel[nfds] = i;
            fd_stderr[nfds] = 0;
            fds[nfds].fd = p->stdout_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }
        if (p->stderr_fd >= 0) {
            fd_panel[nfds] = i;
            fd_stderr[nfds] = 1;
            fds[nfds].fd = p->stderr_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }
    }

    /* shell screen PTY fds */
    int shell_fd_view[MAX_FDS];
    int shell_fd_screen[MAX_FDS];
    int shell_fd_start = nfds;

    if (db->shell_open) {
        for (int v = 0; v < db->shell_view_count; v++) {
            db_shell_view_t *view = &db->shell_views[v];
            for (int s = 0; s < view->screen_count; s++) {
                db_shell_screen_t *scr = &view->screens[s];
                if (scr->pty_master_fd >= 0 && nfds < MAX_FDS) {
                    shell_fd_view[nfds] = v;
                    shell_fd_screen[nfds] = s;
                    fds[nfds].fd = scr->pty_master_fd;
                    fds[nfds].events = POLLIN;
                    fds[nfds].revents = 0;
                    nfds++;
                }
            }
        }
    }

    /* stdin -- so poll() wakes on keyboard input immediately,
     * allowing ncurses to reassemble multi-byte escape sequences
     * (Shift+Up = \033[1;2A) before getch() splits them */
    int stdin_idx = nfds;
    fds[nfds].fd = STDIN_FILENO;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;
    nfds++;

    /* signal pipe */
    int sig_idx = -1;
    if (sig_fd >= 0) {
        sig_idx = nfds;
        fds[nfds].fd = sig_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;
    }

    int pr = poll(fds, (nfds_t)nfds, POLL_TIMEOUT_MS);

    /* drain signal pipe (non-blocking) */
    if (sig_idx >= 0 && (pr > 0 && (fds[sig_idx].revents & POLLIN)))
        ph_signal_pipe_drain();

    /* check signal flags -- always, because poll may return EINTR
     * on SIGWINCH/SIGINT without setting revents */
    if (pr == -1 && errno == EINTR)
        ph_signal_pipe_drain();

    if (ph_signal_interrupted() && *nevents < MAX_EVENTS) {
        events[*nevents].type = DB_EVT_SIGNAL;
        (*nevents)++;
    }

    if (ph_signal_winch_pending() && *nevents < MAX_EVENTS) {
        events[*nevents].type = DB_EVT_WINCH;
        (*nevents)++;
    }

    /* read pipe data (skip stdin + signal pipe -- handled separately) */
    for (int f = 0; f < nfds && *nevents < MAX_EVENTS; f++) {
        if (f == stdin_idx || f == sig_idx) continue;
        if (!(fds[f].revents & (POLLIN | POLLHUP))) continue;

        char buf[4096];
        ssize_t n = read(fds[f].fd, buf, sizeof(buf));

        if (f >= shell_fd_start && f < stdin_idx) {
            /* shell PTY data */
            if (n > 0 && *nevents < MAX_EVENTS) {
                db_event_t *evt = &events[*nevents];
                evt->type = DB_EVT_SHELL_DATA;
                evt->d.shell.view_idx = shell_fd_view[f];
                evt->d.shell.screen_idx = shell_fd_screen[f];
                int len = (int)n;
                if (len > (int)sizeof(evt->d.shell.buf))
                    len = (int)sizeof(evt->d.shell.buf);
                memcpy(evt->d.shell.buf, buf, (size_t)len);
                evt->d.shell.len = len;
                (*nevents)++;
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EIO)) {
                if (*nevents < MAX_EVENTS) {
                    db_event_t *evt = &events[*nevents];
                    evt->type = DB_EVT_SHELL_EOF;
                    evt->d.shell.view_idx = shell_fd_view[f];
                    evt->d.shell.screen_idx = shell_fd_screen[f];
                    evt->d.shell.len = 0;
                    (*nevents)++;
                }
            }
        } else {
            /* panel pipe data */
            if (n > 0) {
                db_event_t *evt = &events[*nevents];
                evt->type = DB_EVT_PIPE_DATA;
                evt->d.pipe.panel_idx = fd_panel[f];
                evt->d.pipe.is_stderr = fd_stderr[f] != 0;
                int len = (int)n;
                if (len > (int)sizeof(evt->d.pipe.buf))
                    len = (int)sizeof(evt->d.pipe.buf);
                memcpy(evt->d.pipe.buf, buf, (size_t)len);
                evt->d.pipe.len = len;
                (*nevents)++;
            } else if (n == 0) {
                if (*nevents < MAX_EVENTS) {
                    db_event_t *evt = &events[*nevents];
                    evt->type = DB_EVT_PIPE_EOF;
                    evt->d.pipe.panel_idx = fd_panel[f];
                    evt->d.pipe.is_stderr = fd_stderr[f] != 0;
                    evt->d.pipe.len = 0;
                    (*nevents)++;
                }
            }
        }
    }

    /* reap children */
    reap_children(db, events, nevents);

    /* keyboard -- when stdin has data, use a short timeout so ncurses
     * can reassemble multi-byte escape sequences (e.g. Shift+Up =
     * \033[1;2A).  nodelay mode returns \033 immediately before the
     * rest of the bytes arrive, breaking define_key sequences. */
    if (fds[stdin_idx].revents & POLLIN)
        wtimeout(stdscr, 25);

    int ch;
    while ((ch = getch()) != ERR && *nevents < MAX_EVENTS) {
        events[*nevents].type = DB_EVT_KEY;
        events[*nevents].d.key.ch = ch;
        (*nevents)++;
    }

    nodelay(stdscr, TRUE);  /* restore non-blocking */

    /* if nothing happened, emit a tick */
    if (*nevents == 0) {
        events[0].type = DB_EVT_TICK;
        *nevents = 1;
    }
}

void handle_event(ph_dashboard_t *db, const db_event_t *evt) {
    switch (evt->type) {
    case DB_EVT_SIGNAL:     handle_signal(db);                    break;
    case DB_EVT_WINCH:      handle_winch(db);                     break;
    case DB_EVT_PIPE_DATA:  handle_pipe_data(db, &evt->d.pipe);   break;
    case DB_EVT_PIPE_EOF:   handle_pipe_eof(db, &evt->d.pipe);    break;
    case DB_EVT_CHILD_EXIT: handle_child_exit(db, &evt->d.child); break;
    case DB_EVT_KEY:        handle_key(db, evt->d.key.ch);        break;
    case DB_EVT_TICK:       handle_tick(db);                       break;
    case DB_EVT_SHELL_DATA: handle_shell_data(db, &evt->d.shell);  break;
    case DB_EVT_SHELL_EOF:  handle_shell_eof(db, &evt->d.shell);   break;
    case DB_EVT_NONE:       break;
    }
}

#endif /* PHOSPHOR_HAS_NCURSES */

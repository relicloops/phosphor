#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"

#include <sys/wait.h>

void handle_child_exit(ph_dashboard_t *db, const db_evt_child_t *c) {
    if (c->panel_idx < 0 || c->panel_idx >= db->panel_count) return;

    db_panel_t *p = &db->panels[c->panel_idx];
    p->exit_code = c->exit_code;
    p->status = PANEL_EXITED;
    p->pid = 0;
    db->alive--;
}

void reap_children(ph_dashboard_t *db, db_event_t *events, int *nevents) {
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->pid <= 0 || p->status != PANEL_RUNNING) continue;

        int status;
        pid_t r = waitpid(p->pid, &status, WNOHANG);
        if (r <= 0) continue;

        int code = 0;
        if (WIFEXITED(status))
            code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            code = 128 + WTERMSIG(status);

        if (*nevents < MAX_EVENTS) {
            db_event_t *evt = &events[*nevents];
            evt->type = DB_EVT_CHILD_EXIT;
            evt->d.child.panel_idx = i;
            evt->d.child.exit_code = code;
            (*nevents)++;
        }
    }

    /* reap shell screen children */
    if (db->shell_open) {
        for (int v = 0; v < db->shell_view_count; v++) {
            db_shell_view_t *view = &db->shell_views[v];
            for (int s = 0; s < view->screen_count; s++) {
                db_shell_screen_t *scr = &view->screens[s];
                if (scr->pid <= 0 || scr->status != DB_SCREEN_RUNNING)
                    continue;
                int status;
                pid_t r = waitpid(scr->pid, &status, WNOHANG);
                if (r <= 0) continue;
                scr->status = DB_SCREEN_EXITED;
                scr->exit_code = WIFEXITED(status) ? WEXITSTATUS(status)
                               : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
                scr->pid = 0;
                view->busy = false;
            }
        }
    }
}

#endif /* PHOSPHOR_HAS_NCURSES */

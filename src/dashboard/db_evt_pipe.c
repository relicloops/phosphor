#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"

#include <unistd.h>

void handle_pipe_data(ph_dashboard_t *db, const db_evt_pipe_t *p) {
    if (p->panel_idx < 0 || p->panel_idx >= db->panel_count) return;

    db_panel_t *pan = &db->panels[p->panel_idx];
    db_accum_t *acc = p->is_stderr ? &pan->err_acc : &pan->out_acc;

    if (pan->tab_count > 0) {
        /* route to matching tab(s) based on source */
        db_ringbuf_t *targets[DB_MAX_TABS];
        int ntargets = 0;
        int active_old = -1;

        for (int t = 0; t < pan->tab_count; t++) {
            db_tab_t *tab = &pan->tabs[t];
            bool match = (tab->source == DB_TAB_BOTH)
                      || (tab->source == DB_TAB_STDOUT && !p->is_stderr)
                      || (tab->source == DB_TAB_STDERR && p->is_stderr);
            if (match) {
                if (t == pan->active_tab)
                    active_old = tab->ring.count;
                targets[ntargets++] = &tab->ring;
            }
        }

        if (ntargets > 0)
            feed_accum_multi(acc, targets, ntargets, p->buf, p->len,
                             p->is_stderr);

        /* freeze viewport on active tab only */
        db_tab_t *at = &pan->tabs[pan->active_tab];
        if (active_old >= 0 && at->scroll > 0) {
            int added = at->ring.count - active_old;
            if (added > 0)
                at->scroll += added;
        }
    } else {
        /* legacy: single ring buffer */
        int old_count = pan->ring.count;
        feed_accum(acc, &pan->ring, p->buf, p->len, p->is_stderr);

        if (pan->scroll > 0) {
            int added = pan->ring.count - old_count;
            if (added > 0)
                pan->scroll += added;
        }
    }
}

void handle_pipe_eof(ph_dashboard_t *db, const db_evt_pipe_t *p) {
    if (p->panel_idx < 0 || p->panel_idx >= db->panel_count) return;

    db_panel_t *pan = &db->panels[p->panel_idx];
    if (p->is_stderr) {
        if (pan->stderr_fd >= 0) {
            close(pan->stderr_fd);
            pan->stderr_fd = -1;
        }
    } else {
        if (pan->stdout_fd >= 0) {
            close(pan->stdout_fd);
            pan->stdout_fd = -1;
        }
    }
}

#endif /* PHOSPHOR_HAS_NCURSES */

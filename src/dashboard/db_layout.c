#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"

void layout_panels(ph_dashboard_t *db) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* info box */
    int info_h = 0;
    if (db->info_count > 0)
        info_h = db->info_count + 2;

    if (info_h > 0) {
        if (db->info_win) delwin(db->info_win);
        db->info_win = newwin(info_h, cols, 0, 0);
    }

    int active = db->panel_count;
    if (active <= 0) return;

    int shell_h = db->shell_open ? db->shell_height : 0;

    int panel_start = info_h;
    int avail_rows = rows - info_h - 1 - shell_h; /* -1 status bar, -shell_h shell */
    if (avail_rows < 3) avail_rows = 3;

    if (db->zoomed) {
        /* fullscreen: only the focused panel gets a window */
        for (int i = 0; i < active; i++) {
            if (db->panels[i].win) { delwin(db->panels[i].win); db->panels[i].win = NULL; }
        }
        db->panels[db->focused].win = newwin(avail_rows, cols, panel_start, 0);
        return;
    }

    if (cols >= 40 * active) {
        /* side by side */
        int pw = cols / active;
        for (int i = 0; i < active; i++) {
            if (db->panels[i].win) delwin(db->panels[i].win);
            int w = (i == active - 1) ? (cols - pw * i) : pw;
            db->panels[i].win = newwin(avail_rows, w, panel_start, pw * i);
        }
    } else {
        /* stacked vertically */
        int ph = avail_rows / active;
        for (int i = 0; i < active; i++) {
            if (db->panels[i].win) delwin(db->panels[i].win);
            int h = (i == active - 1) ? (avail_rows - ph * i) : ph;
            db->panels[i].win = newwin(h, cols, panel_start + ph * i, 0);
        }
    }

    /* shell window -- between panels and status bar */
    if (db->shell_open && shell_h > 0) {
        int shell_y = rows - 1 - shell_h;
        if (db->shell_win) delwin(db->shell_win);
        db->shell_win = newwin(shell_h, cols, shell_y, 0);
    } else {
        if (db->shell_win) { delwin(db->shell_win); db->shell_win = NULL; }
    }
}

#endif /* PHOSPHOR_HAS_NCURSES */

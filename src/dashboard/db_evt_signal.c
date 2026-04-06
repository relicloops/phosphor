#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/signal.h"

#include <curses.h>

void handle_signal(ph_dashboard_t *db) {
    db->quit = true;
}

void handle_winch(ph_dashboard_t *db) {
    ph_signal_winch_clear();
    endwin();
    refresh();
    layout_panels(db);
    /* popup window is recreated by draw_popup() on next draw_all() */
}

#endif /* PHOSPHOR_HAS_NCURSES */

#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"

#include <string.h>

/* ---- popup geometry ---- */

#define HELP_W      56
#define HELP_H      42
#define ABOUT_W     46
#define ABOUT_H     10
#define CMDS_W      50
#define CMDS_H      17
#define PH_HELP_W   56
#define PH_HELP_H   20

static void create_popup_win(ph_dashboard_t *db, int want_h, int want_w) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int h = want_h;
    int w = want_w;
    if (h > rows - 2) h = rows - 2;
    if (w > cols - 4) w = cols - 4;
    if (h < 5) h = 5;
    if (w < 20) w = 20;

    int y = (rows - h) / 2;
    int x = (cols - w) / 2;

    if (db->popup_win) delwin(db->popup_win);
    db->popup_win = newwin(h, w, y, x);
}

/* ---- open / close ---- */

void open_popup(ph_dashboard_t *db, db_popup_t which) {
    db->popup = which;
    db->mode = DB_MODE_POPUP;

    switch (which) {
    case DB_POPUP_HELP:     create_popup_win(db, HELP_H, HELP_W);       break;
    case DB_POPUP_ABOUT:    create_popup_win(db, ABOUT_H, ABOUT_W);     break;
    case DB_POPUP_COMMANDS: create_popup_win(db, CMDS_H, CMDS_W);       break;
    case DB_POPUP_PH_HELP:       create_popup_win(db, PH_HELP_H, PH_HELP_W); break;
    case DB_POPUP_JSON_VIEWER:   break;  /* viewer creates its own window */
    case DB_POPUP_NONE:          break;
    }
}

void close_popup(ph_dashboard_t *db) {
    db->popup = DB_POPUP_NONE;
    db->mode = DB_MODE_NORMAL;

    if (db->popup_win) {
        delwin(db->popup_win);
        db->popup_win = NULL;
    }

    /* force full redraw to clear popup shadow */
    touchwin(stdscr);
}

/* ---- draw help content ---- */

static void draw_help_content(ph_dashboard_t *db) {
    WINDOW *w = db->popup_win;
    if (!w) return;

    int rows, cols;
    getmaxyx(w, rows, cols);
    (void)cols;

    /* title */
    wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
    mvwprintw(w, 0, 2, " Keybindings ");
    wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

    /* keybinding entries */
    struct { const char *key; const char *desc; } bindings[] = {
        { "q",       "Quit dashboard" },
        { "Tab",     "Switch panel focus" },
        { "f",       "Toggle fullscreen (zoom)" },
        { "c",       "Clear focused panel" },
        { "/",       "Search in focused panel" },
        { "n",       "Jump to next match" },
        { "N",       "Jump to previous match" },
        { "Up/k",    "Scroll up" },
        { "Down/j",  "Scroll down" },
        { "PgUp",    "Scroll up one page" },
        { "PgDn",    "Scroll down one page" },
        { "Home",    "Scroll to top" },
        { "End",     "Back to auto-follow" },
        { "v",       "Enter select mode" },
        { "V",       "Export selection to JSON" },
        { "g",       "Fuzzy log finder" },
        { "z",       "Toggle JSON fold" },
        { "1..4",    "Switch tab (when available)" },
        { ":",       "Enter command mode" },
        { ":start",  "Start server" },
        { ":stop",   "Stop server" },
        { ":save",   "Save panel log to file" },
        { ":clear",  "Clear focused panel" },
        { "?",       "Show this help" },
        { "a",       "About phosphor" },
        { "Ctrl-S",  "Select Start button" },
        { "Ctrl-T",  "Select Stop button" },
        { "Enter",   "Activate selected button" },
        { "Esc",     "Clear selection / search / popup" },
        { "",        "" },
        { "Ctrl-P",  "New shell view (up to 4)" },
        { "Ctrl-G",  "Focus back to shell" },
        { "Ctrl-B",  "Next shell view tab" },
        { "Ctrl-R",  "Prev shell view tab" },
        { "Ctrl-Q",  "Close shell (kill all)" },
        { "Ctrl-D",  "Open cmd bar from shell" },
        { "Ctrl-N",  "Next screen (shell)" },
        { "Ctrl-X",  "Minimize screen (shell)" },
        { "Ctrl-W",  "Save screen to file (shell)" },
    };
    int nbind = (int)(sizeof(bindings) / sizeof(bindings[0]));

    for (int i = 0; i < nbind && i + 2 < rows - 2; i++) {
        wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
        mvwprintw(w, i + 2, 2, "%-10s", bindings[i].key);
        wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

        wattron(w, COLOR_PAIR(CP_POPUP_TEXT));
        wprintw(w, " %s", bindings[i].desc);
        wattroff(w, COLOR_PAIR(CP_POPUP_TEXT));
    }

    /* footer with navigation */
    wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
    mvwprintw(w, rows - 1, 2, "c:commands  h:phosphor-help  Esc:close");
    wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
}

/* ---- draw about content ---- */

static void draw_about_content(ph_dashboard_t *db) {
    WINDOW *w = db->popup_win;
    if (!w) return;

    int rows, cols;
    getmaxyx(w, rows, cols);
    (void)cols;

    /* title */
    wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
    mvwprintw(w, 0, 2, " About ");
    wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

    int y = 2;
    wattron(w, COLOR_PAIR(CP_POPUP_TEXT) | A_BOLD);
    mvwprintw(w, y++, 2, "phosphor");
    wattroff(w, COLOR_PAIR(CP_POPUP_TEXT) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_POPUP_TEXT));
#ifdef PHOSPHOR_VERSION
    mvwprintw(w, y++, 2, "Version: %s", PHOSPHOR_VERSION);
#else
    mvwprintw(w, y++, 2, "Version: unknown");
#endif
    y++;
    mvwprintw(w, y++, 2, "A pure C CLI for the NeonSignal");
    mvwprintw(w, y++, 2, "ecosystem. Scaffolds, builds,");
    mvwprintw(w, y++, 2, "serves, and manages TLS certs.");
    wattroff(w, COLOR_PAIR(CP_POPUP_TEXT));

    /* footer */
    if (y < rows - 1) {
        wattron(w, COLOR_PAIR(CP_POPUP_TEXT) | A_DIM);
        mvwprintw(w, rows - 1, 2, "Press Esc/Enter/q to close");
        wattroff(w, COLOR_PAIR(CP_POPUP_TEXT) | A_DIM);
    }
}

/* ---- draw commands content ---- */

static void draw_commands_content(ph_dashboard_t *db) {
    WINDOW *w = db->popup_win;
    if (!w) return;

    int rows, cols;
    getmaxyx(w, rows, cols);
    (void)cols;

    /* title */
    wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
    mvwprintw(w, 0, 2, " Dashboard Commands ");
    wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

    struct { const char *cmd; const char *desc; } cmds[] = {
        { ":start",       "Start the server" },
        { ":stop",        "Stop the server" },
        { ":clear",       "Clear focused panel log" },
        { ":save <path>", "Save panel to JSON, clear" },
        { ":saveall",     "Save all panels to JSON, clear" },
        { ":shell",       "Open embedded shell (Ctrl-P)" },
        { ":shellclose",  "Close embedded shell (Ctrl-Q)" },
        { ":filament",    "(not yet implemented)" },
    };
    int ncmds = (int)(sizeof(cmds) / sizeof(cmds[0]));

    for (int i = 0; i < ncmds && i + 2 < rows - 1; i++) {
        wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
        mvwprintw(w, i + 2, 2, "%-16s", cmds[i].cmd);
        wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

        wattron(w, COLOR_PAIR(CP_POPUP_TEXT));
        wprintw(w, " %s", cmds[i].desc);
        wattroff(w, COLOR_PAIR(CP_POPUP_TEXT));
    }

    /* footer */
    wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
    mvwprintw(w, rows - 1, 2, "?:help  Esc:close");
    wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
}

/* ---- draw phosphor help content ---- */

static void draw_ph_help_content(ph_dashboard_t *db) {
    WINDOW *w = db->popup_win;
    if (!w) return;

    int rows, cols;
    getmaxyx(w, rows, cols);
    (void)cols;

    /* title */
    wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
    mvwprintw(w, 0, 2, " phosphor help ");
    wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

    struct { const char *cmd; const char *desc; } cmds[] = {
        { "create",   "Scaffold a new project from a template" },
        { "glow",     "Scaffold a Cathode landing page from embedded template" },
        { "build",    "Bundle and deploy a Cathode JSX project via esbuild" },
        { "serve",    "Start neonsignal dev server with dashboard" },
        { "clean",    "Remove build artifacts and stale staging directories" },
        { "rm",       "Remove a specific path within the project" },
        { "certs",    "Generate TLS certificates (local CA or Let's Encrypt)" },
        { "doctor",   "Run project diagnostics" },
        { "filament", "[experimental] reserved for future functionality" },
        { "version",  "Print phosphor version" },
        { "help",     "Show help for a command" },
    };
    int ncmds = (int)(sizeof(cmds) / sizeof(cmds[0]));

    int y = 2;
    wattron(w, COLOR_PAIR(CP_POPUP_TEXT) | A_BOLD);
    mvwprintw(w, y++, 2, "Available commands:");
    wattroff(w, COLOR_PAIR(CP_POPUP_TEXT) | A_BOLD);
    y++;

    for (int i = 0; i < ncmds && y < rows - 1; i++, y++) {
        wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
        mvwprintw(w, y, 2, "  %-12s", cmds[i].cmd);
        wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

        wattron(w, COLOR_PAIR(CP_POPUP_TEXT));
        wprintw(w, " %s", cmds[i].desc);
        wattroff(w, COLOR_PAIR(CP_POPUP_TEXT));
    }

    /* footer */
    wattron(w, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
    mvwprintw(w, rows - 1, 2, "?:help  Esc:close");
    wattroff(w, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
}

/* ---- draw popup ---- */

void draw_popup(ph_dashboard_t *db) {
    if (db->popup == DB_POPUP_NONE || !db->popup_win) return;

    /* JSON viewer handles its own rendering */
    if (db->popup == DB_POPUP_JSON_VIEWER) {
        draw_json_viewer(db);
        return;
    }

    /* recreate window for current terminal size */
    switch (db->popup) {
    case DB_POPUP_HELP:     create_popup_win(db, HELP_H, HELP_W);       break;
    case DB_POPUP_ABOUT:    create_popup_win(db, ABOUT_H, ABOUT_W);     break;
    case DB_POPUP_COMMANDS: create_popup_win(db, CMDS_H, CMDS_W);       break;
    case DB_POPUP_PH_HELP:       create_popup_win(db, PH_HELP_H, PH_HELP_W); break;
    case DB_POPUP_JSON_VIEWER:   break;
    case DB_POPUP_NONE:          break;
    }

    WINDOW *w = db->popup_win;
    werase(w);

    /* border */
    wattron(w, COLOR_PAIR(CP_POPUP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_POPUP_BORDER));

    /* content */
    switch (db->popup) {
    case DB_POPUP_HELP:     draw_help_content(db);     break;
    case DB_POPUP_ABOUT:    draw_about_content(db);    break;
    case DB_POPUP_COMMANDS: draw_commands_content(db);  break;
    case DB_POPUP_PH_HELP:       draw_ph_help_content(db);  break;
    case DB_POPUP_JSON_VIEWER:   break;
    case DB_POPUP_NONE:          break;
    }

    wnoutrefresh(w);
}

#endif /* PHOSPHOR_HAS_NCURSES */

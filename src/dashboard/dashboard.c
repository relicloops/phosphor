#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/signal.h"

#include <locale.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* ---- public API ---- */

ph_result_t ph_dashboard_create(const ph_dashboard_config_t *cfg,
                                 ph_dashboard_t **out) {
    if (!cfg || !out) return PH_ERR;

    ph_dashboard_t *db = ph_calloc(1, sizeof(*db));
    if (!db) return PH_ERR;

    db->panel_count = cfg->panel_count;
    db->focused = 0;
    db->alive = 0;
    db->status_text = cfg->status_text;
    db->quit = false;
    db->info_win = NULL;
    db->popup_win = NULL;
    db->mode = DB_MODE_NORMAL;
    db->popup = DB_POPUP_NONE;
    db->btn_selected = DB_BTN_NONE;
    db->btn_flash = 0;
    db->zoomed = false;
    db->search_active = false;
    db->search_pat[0] = '\0';
    db->search_buf[0] = '\0';
    db->search_len = 0;
    db->jv_nodes = NULL;
    db->jv_node_count = 0;
    db->jv_cursor = 0;
    db->jv_scroll = 0;
    db->jv_title[0] = '\0';
    db->shell_open = false;
    db->shell_height = DB_SHELL_DEFAULT_H;
    db->shell_view_count = 0;
    db->shell_active_view = 0;
    db->shell_win = NULL;

    /* borrowed pointers for start/stop lifecycle */
    db->serve_cfg = cfg->serve_cfg;
    db->session_ptr = cfg->session_ptr;

    /* copy info lines */
    db->info_count = cfg->info_count;
    for (int i = 0; i < cfg->info_count && i < PH_DASHBOARD_MAX_INFO_LINES; i++)
        db->info_lines[i] = cfg->info_lines[i];

    for (int i = 0; i < cfg->panel_count && i < PH_DASHBOARD_MAX_PANELS; i++) {
        db->panels[i].name = cfg->panels[i].name;
        db->panels[i].id = cfg->panels[i].id;
        db->panels[i].pid = cfg->panels[i].pid;
        db->panels[i].stdout_fd = cfg->panels[i].stdout_fd;
        db->panels[i].stderr_fd = cfg->panels[i].stderr_fd;
        db->panels[i].status = (cfg->panels[i].pid > 0) ? PANEL_RUNNING
                                                          : PANEL_EXITED;
        db->panels[i].exit_code = 0;
        db->panels[i].scroll = 0;
        db->panels[i].cursor = -1;
        db->panels[i].sel_anchor = -1;
        db->panels[i].json_fold_idx = -1;
        db->panels[i].json_fold_text = NULL;
        db->panels[i].json_fold_lines = 0;
        db->panels[i].win = NULL;
        memset(&db->panels[i].ring, 0, sizeof(db_ringbuf_t));
        memset(&db->panels[i].out_acc, 0, sizeof(db_accum_t));
        memset(&db->panels[i].err_acc, 0, sizeof(db_accum_t));

        /* tab init */
        int tc = cfg->panels[i].tab_count;
        if (tc > DB_MAX_TABS) tc = DB_MAX_TABS;
        db->panels[i].tab_count = tc;
        db->panels[i].active_tab = 0;
        for (int t = 0; t < tc; t++) {
            db_tab_t *tab = &db->panels[i].tabs[t];
            const char *tname = cfg->panels[i].tabs[t].name;
            if (tname) {
                strncpy(tab->name, tname, DB_TAB_NAME_LEN - 1);
                tab->name[DB_TAB_NAME_LEN - 1] = '\0';
            } else {
                tab->name[0] = '\0';
            }
            tab->source = (db_tab_source_t)cfg->panels[i].tabs[t].source_stream;
            memset(&tab->ring, 0, sizeof(db_ringbuf_t));
            tab->scroll = 0;
            tab->cursor = -1;
            tab->sel_anchor = -1;
            tab->json_fold_idx = -1;
            tab->json_fold_text = NULL;
            tab->json_fold_lines = 0;
        }

        if (cfg->panels[i].pid > 0) db->alive++;
    }

    /* init ncurses */
    setlocale(LC_ALL, "");
    set_escdelay(25);  /* 25ms: long enough for escape sequences, short for Esc key */
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

    /* register xterm Shift+arrow sequences for multi-select */
    define_key("\033[1;2A", KEY_SR);   /* Shift+Up */
    define_key("\033[1;2B", KEY_SF);   /* Shift+Down */



    /* disable XON/XOFF so Ctrl-S reaches the application */
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_iflag &= ~(tcflag_t)(IXON | IXOFF);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    if (has_colors()) {
        start_color();
        use_default_colors();

        init_pair(CP_BORDER_NORMAL,  COLOR_WHITE,   -1);
        init_pair(CP_BORDER_FOCUSED, COLOR_CYAN,    -1);
        init_pair(CP_TITLE_NS,       COLOR_BLACK,   COLOR_CYAN);
        init_pair(CP_TITLE_REDIR,    COLOR_BLACK,   COLOR_GREEN);
        init_pair(CP_TITLE_WATCH,    COLOR_BLACK,   COLOR_YELLOW);
        init_pair(CP_STATUS_RUN,     COLOR_GREEN,   -1);
        init_pair(CP_STATUS_EXIT,    COLOR_RED,     -1);
        init_pair(CP_LOG_STDERR,     COLOR_RED,     -1);
        init_pair(CP_STATUSBAR,      COLOR_BLACK,   COLOR_WHITE);
        init_pair(CP_TITLE_BG,       COLOR_WHITE,   COLOR_BLUE);

        /* info box colors */
        init_pair(CP_INFO_LABEL,     COLOR_WHITE,   -1);
        init_pair(CP_INFO_CYAN,      COLOR_CYAN,    -1);
        init_pair(CP_INFO_GREEN,     COLOR_GREEN,   -1);
        init_pair(CP_INFO_YELLOW,    COLOR_YELLOW,  -1);
        init_pair(CP_INFO_RED,       COLOR_RED,     -1);
        init_pair(CP_INFO_DIM,       COLOR_WHITE,   -1);
        init_pair(CP_INFO_BOX,       COLOR_CYAN,    -1);

        /* ANSI foreground color passthrough (SGR 30-37) */
        init_pair(CP_ANSI_BASE + 0,  COLOR_BLACK,   -1);
        init_pair(CP_ANSI_BASE + 1,  COLOR_RED,     -1);
        init_pair(CP_ANSI_BASE + 2,  COLOR_GREEN,   -1);
        init_pair(CP_ANSI_BASE + 3,  COLOR_YELLOW,  -1);
        init_pair(CP_ANSI_BASE + 4,  COLOR_BLUE,    -1);
        init_pair(CP_ANSI_BASE + 5,  COLOR_MAGENTA, -1);
        init_pair(CP_ANSI_BASE + 6,  COLOR_CYAN,    -1);
        init_pair(CP_ANSI_BASE + 7,  COLOR_WHITE,   -1);

        /* button colors */
        init_pair(CP_BTN_START_ACTIVE, COLOR_WHITE,  COLOR_GREEN);
        init_pair(CP_BTN_STOP_ACTIVE,  COLOR_WHITE,  COLOR_RED);
        init_pair(CP_BTN_DISABLED,     COLOR_WHITE,  COLOR_BLACK);
        init_pair(CP_BTN_SELECTED,     COLOR_BLACK,  COLOR_WHITE);

        /* popup colors */
        init_pair(CP_POPUP_BORDER,     COLOR_CYAN,   -1);
        init_pair(CP_POPUP_TEXT,       COLOR_WHITE,  -1);
        init_pair(CP_POPUP_KEY,        COLOR_GREEN,  -1);

        /* brand symbol badge (green on black) */
        init_pair(CP_SYMBOL,           COLOR_GREEN,  COLOR_BLACK);

        /* info box title */
        init_pair(CP_TITLE_SERVE,      COLOR_BLACK,  COLOR_MAGENTA);

        /* search highlight */
        init_pair(CP_SEARCH_MATCH,     COLOR_BLACK,  COLOR_YELLOW);

        /* cursor + selection */
        init_pair(CP_CURSOR_LINE,      COLOR_BLACK,  COLOR_WHITE);
        init_pair(CP_SELECTED_LINE,    COLOR_WHITE,  COLOR_BLUE);

        /* fuzzy finder */
        init_pair(CP_FUZZY_MATCH,      COLOR_GREEN,  -1);
        init_pair(CP_FUZZY_PROMPT,     COLOR_CYAN,   -1);

        /* json fold syntax */
        init_pair(CP_JSON_KEY,         COLOR_CYAN,   -1);
        init_pair(CP_JSON_STRING,      COLOR_GREEN,  -1);
        init_pair(CP_JSON_NUMBER,      COLOR_YELLOW, -1);
        init_pair(CP_JSON_BOOL,        COLOR_MAGENTA,-1);

        /* panel tabs */
        init_pair(CP_TAB_ACTIVE,       COLOR_WHITE,  -1);
        init_pair(CP_TAB_INACTIVE,     COLOR_WHITE,  -1);

        /* shell */
        init_pair(CP_SHELL_BORDER,       COLOR_CYAN,   -1);
        init_pair(CP_SHELL_INPUT,        COLOR_WHITE,  -1);
        init_pair(CP_SHELL_PROMPT,       COLOR_GREEN,  -1);
        init_pair(CP_SHELL_TAB_ACTIVE,   COLOR_WHITE,  -1);
        init_pair(CP_SHELL_TAB_INACTIVE, COLOR_WHITE,  -1);
        init_pair(CP_SCREEN_BORDER,      COLOR_YELLOW, -1);
        init_pair(CP_SCREEN_TITLE,       COLOR_WHITE,  -1);
    }

    /* install SIGWINCH handler */
    ph_signal_install_winch();

    /* create panel windows */
    layout_panels(db);

    *out = db;
    return PH_OK;
}

int ph_dashboard_run(ph_dashboard_t *db) {
    if (!db) return 1;

    int sig_fd = ph_signal_pipe_init();
    draw_all(db);

    while (!db->quit) {
        db_event_t events[MAX_EVENTS];
        int nevents = 0;

        collect_events(db, events, &nevents, sig_fd);

        for (int i = 0; i < nevents; i++)
            handle_event(db, &events[i]);

        draw_all(db);

        /* auto-quit only when no restart is possible */
        if (db->alive == 0 && !db->serve_cfg && !any_fd_open(db))
            db->quit = true;
    }

    shutdown_children(db);
    endwin();

    /* print shutdown summary to restored terminal */
    ph_log_info("serve: shutting down...");
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->exit_code == 0)
            ph_log_info("serve: %s stopped (exit 0)", p->name);
        else
            ph_log_warn("serve: %s stopped (exit %d)", p->name, p->exit_code);
    }

    return compute_worst_exit(db);
}

void ph_dashboard_destroy(ph_dashboard_t *db) {
    if (!db) return;

    shell_close_all(db);
    fuzzy_unload_disk(db);
    if (db->jv_nodes) {
        for (int i = 0; i < db->jv_node_count; i++) {
            if (db->jv_nodes[i].key) ph_free(db->jv_nodes[i].key);
            if (db->jv_nodes[i].value) ph_free(db->jv_nodes[i].value);
        }
        ph_free(db->jv_nodes);
    }
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->tab_count > 0) {
            for (int t = 0; t < p->tab_count; t++) {
                db_tab_t *tab = &p->tabs[t];
                ringbuf_destroy(&tab->ring);
                if (tab->json_fold_text) { ph_free(tab->json_fold_text); tab->json_fold_text = NULL; }
            }
        } else {
            json_fold_cleanup(p);
            ringbuf_destroy(&p->ring);
        }
        if (p->win) delwin(p->win);
        if (p->stdout_fd >= 0) close(p->stdout_fd);
        if (p->stderr_fd >= 0) close(p->stderr_fd);
    }

    if (db->info_win) delwin(db->info_win);
    if (db->popup_win) delwin(db->popup_win);

    ph_free(db);
}

#endif /* PHOSPHOR_HAS_NCURSES */

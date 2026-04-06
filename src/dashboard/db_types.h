#ifndef DB_TYPES_H
#define DB_TYPES_H

#ifdef PHOSPHOR_HAS_NCURSES

#include "phosphor/dashboard.h"
#include "phosphor/serve.h"

#include <curses.h>
#include <stdbool.h>
#include <sys/types.h>

/* ---- constants ---- */

#define MAX_LINES       2000    /* ring buffer capacity per panel */
#define MAX_LINE_LEN    4096    /* max bytes per output line */
#define POLL_TIMEOUT_MS 100
#define MAX_SHELL_FDS   16  /* one pty fd per active screen */
#define MAX_FDS         (PH_DASHBOARD_MAX_PANELS * 2 + 2 + MAX_SHELL_FDS)
#define MAX_EVENTS      32

/* shell constants */
#define DB_SHELL_MAX_VIEWS    4
#define DB_SHELL_MAX_SCREENS  16
#define DB_SHELL_INPUT_LEN    1024
#define DB_SHELL_MIN_HEIGHT   3
#define DB_SHELL_MAX_HEIGHT   40
#define DB_SHELL_DEFAULT_H    8

/* ---- color pairs ---- */

enum {
    CP_BORDER_NORMAL  = 1,
    CP_BORDER_FOCUSED = 2,
    CP_TITLE_NS       = 3,
    CP_TITLE_REDIR    = 4,
    CP_TITLE_WATCH    = 5,
    CP_STATUS_RUN     = 6,
    CP_STATUS_EXIT    = 7,
    CP_LOG_STDERR     = 8,
    CP_STATUSBAR      = 9,
    CP_TITLE_BG       = 10,
    CP_INFO_LABEL     = 11,
    CP_INFO_CYAN      = 12,
    CP_INFO_GREEN     = 13,
    CP_INFO_YELLOW    = 14,
    CP_INFO_RED       = 15,
    CP_INFO_DIM       = 16,
    CP_INFO_BOX       = 17,
    /* ANSI foreground colors: pair 20+i where i=0..7 maps to COLOR_i */
    CP_ANSI_BASE      = 20,
    /* button colors */
    CP_BTN_START_ACTIVE = 30,   /* white on green */
    CP_BTN_STOP_ACTIVE  = 31,   /* white on red */
    CP_BTN_DISABLED     = 32,   /* white on black (grey) */
    CP_BTN_SELECTED     = 33,   /* black on white (reverse) */
    /* popup colors */
    CP_POPUP_BORDER     = 34,   /* cyan on default */
    CP_POPUP_TEXT       = 35,   /* white on default */
    CP_POPUP_KEY        = 36,   /* green on default */
    /* brand symbol */
    CP_SYMBOL           = 37,   /* green on black */
    /* info box title */
    CP_TITLE_SERVE      = 38,   /* black on magenta */
    /* search */
    CP_SEARCH_MATCH     = 39,   /* black on yellow */
    /* cursor + selection */
    CP_CURSOR_LINE      = 40,   /* black on white (reverse bar) */
    CP_SELECTED_LINE    = 41,   /* white on blue */
    /* fuzzy finder */
    CP_FUZZY_MATCH      = 42,   /* green on default */
    CP_FUZZY_PROMPT     = 43,   /* cyan on default */
    /* json fold */
    CP_JSON_KEY         = 44,   /* cyan on default */
    CP_JSON_STRING      = 45,   /* green on default */
    CP_JSON_NUMBER      = 46,   /* yellow on default */
    CP_JSON_BOOL        = 47,   /* magenta on default */
    /* panel tabs */
    CP_TAB_ACTIVE       = 48,   /* white bold on default */
    CP_TAB_INACTIVE     = 49,   /* dim on default */
    /* shell */
    CP_SHELL_BORDER       = 50,   /* cyan on default */
    CP_SHELL_INPUT        = 51,   /* white on default */
    CP_SHELL_PROMPT       = 52,   /* green on default */
    CP_SHELL_TAB_ACTIVE   = 53,   /* white bold on default */
    CP_SHELL_TAB_INACTIVE = 54,   /* dim on default */
    CP_SCREEN_BORDER      = 55,   /* yellow on default */
    CP_SCREEN_TITLE       = 56,   /* white on default */
};

/* ---- UI mode ---- */

typedef enum {
    DB_MODE_NORMAL,
    DB_MODE_COMMAND,
    DB_MODE_POPUP,
    DB_MODE_SEARCH,
    DB_MODE_FUZZY,
    DB_MODE_SHELL,
} db_mode_t;

typedef enum {
    DB_POPUP_NONE = 0,
    DB_POPUP_HELP,
    DB_POPUP_ABOUT,
    DB_POPUP_COMMANDS,
    DB_POPUP_PH_HELP,
    DB_POPUP_JSON_VIEWER,
} db_popup_t;

typedef enum {
    DB_BTN_NONE = 0,
    DB_BTN_START,
    DB_BTN_STOP,
} db_button_sel_t;

/* ---- event types ---- */

typedef enum {
    DB_EVT_NONE = 0,
    DB_EVT_PIPE_DATA,
    DB_EVT_PIPE_EOF,
    DB_EVT_CHILD_EXIT,
    DB_EVT_KEY,
    DB_EVT_SIGNAL,
    DB_EVT_WINCH,
    DB_EVT_TICK,
    DB_EVT_SHELL_DATA,
    DB_EVT_SHELL_EOF,
} db_evt_type_t;

typedef struct {
    int   panel_idx;
    bool  is_stderr;
    char  buf[4096];
    int   len;
} db_evt_pipe_t;

typedef struct {
    int   panel_idx;
    int   exit_code;
} db_evt_child_t;

typedef struct {
    int   ch;
} db_evt_key_t;

typedef struct {
    int   view_idx;
    int   screen_idx;
    char  buf[4096];
    int   len;
} db_evt_shell_t;

typedef struct {
    db_evt_type_t type;
    union {
        db_evt_pipe_t  pipe;
        db_evt_child_t child;
        db_evt_key_t   key;
        db_evt_shell_t shell;
    } d;
} db_event_t;

/* ---- json viewer node ---- */

typedef enum {
    JN_OBJECT, JN_ARRAY, JN_STRING, JN_NUMBER,
    JN_BOOL, JN_NULL, JN_CLOSE,
} db_jn_type_t;

typedef struct {
    db_jn_type_t type;
    int   depth;
    char *key;           /* key name; NULL for root/array items/close */
    char *value;         /* leaf string; "}" or "]" for close */
    int   child_count;
    int   subtree_end;   /* index past this subtree (for fold skip) */
    bool  folded;
    bool  is_last;       /* last child in parent (no trailing comma) */
} db_json_node_t;

/* ---- line entry ---- */

typedef struct {
    char *text;
    int   len;
    int   vis_width;
    bool  is_stderr;
} db_line_t;

/* ---- ring buffer ---- */

typedef struct {
    db_line_t  lines[MAX_LINES];
    int        head;
    int        count;
} db_ringbuf_t;

/* ---- line accumulator ---- */

typedef struct {
    char  buf[MAX_LINE_LEN];
    int   pos;
} db_accum_t;

/* ---- shell key constants ---- */

/* shell keys -- Ctrl+H (0x08) and Ctrl+L (0x0C) checked as raw bytes */

/* ---- shell screen + view ---- */

typedef enum {
    DB_SCREEN_RUNNING,
    DB_SCREEN_EXITED,
} db_screen_status_t;

typedef struct {
    char                 title[256];      /* command string */
    pid_t                pid;
    int                  pty_master_fd;   /* -1 when closed */
    db_screen_status_t   status;
    int                  exit_code;
    db_ringbuf_t         ring;
    db_accum_t           accum;
    int                  scroll;
    bool                 minimized;
    WINDOW              *win;
} db_shell_screen_t;

typedef struct {
    char                  input[DB_SHELL_INPUT_LEN];
    int                   input_len;
    int                   input_cursor;
    db_shell_screen_t     screens[DB_SHELL_MAX_SCREENS];
    int                   screen_count;
    int                   active_screen;   /* -1 = none focused */
    bool                  busy;            /* blocking command running */
} db_shell_view_t;

/* ---- panel tabs ---- */

#define DB_MAX_TABS     PH_DASHBOARD_MAX_TABS
#define DB_TAB_NAME_LEN 24

typedef enum {
    DB_TAB_STDOUT = 0,   /* receives stdout lines only */
    DB_TAB_STDERR = 1,   /* receives stderr lines only */
    DB_TAB_BOTH   = 2,   /* receives both (legacy) */
} db_tab_source_t;

typedef struct {
    char             name[DB_TAB_NAME_LEN];
    db_tab_source_t  source;
    db_ringbuf_t     ring;
    int              scroll;
    int              cursor;
    int              sel_anchor;
    int              json_fold_idx;
    char            *json_fold_text;
    int              json_fold_lines;
} db_tab_t;

/* ---- panel state ---- */

typedef enum {
    PANEL_RUNNING,
    PANEL_EXITED,
} db_panel_status_t;

typedef struct {
    const char             *name;
    ph_dashboard_panel_id_t id;
    pid_t                   pid;
    int                     stdout_fd;
    int                     stderr_fd;
    db_panel_status_t       status;
    int                     exit_code;
    /* tab system */
    int              tab_count;            /* 0 = legacy single-view */
    int              active_tab;           /* index of visible tab */
    db_tab_t         tabs[DB_MAX_TABS];
    /* legacy single-view (used when tab_count == 0) */
    db_ringbuf_t            ring;
    int                     scroll;   /* offset from bottom; 0 = auto-follow */
    int                     cursor;   /* ring buffer index; -1 = auto-follow */
    int                     sel_anchor; /* selection start; -1 = none */
    int                     json_fold_idx;  /* expanded fold; -1 = none */
    char                   *json_fold_text; /* pretty-printed (allocated) */
    int                     json_fold_lines;/* visual line count */
    /* shared state */
    db_accum_t              out_acc;
    db_accum_t              err_acc;
    WINDOW                 *win;
} db_panel_t;

/* ---- panel accessor inlines ---- */

static inline db_ringbuf_t *panel_ring(db_panel_t *p) {
    return p->tab_count > 0 ? &p->tabs[p->active_tab].ring : &p->ring;
}
static inline int *panel_scroll(db_panel_t *p) {
    return p->tab_count > 0 ? &p->tabs[p->active_tab].scroll : &p->scroll;
}
static inline int *panel_cursor(db_panel_t *p) {
    return p->tab_count > 0 ? &p->tabs[p->active_tab].cursor : &p->cursor;
}
static inline int *panel_sel_anchor(db_panel_t *p) {
    return p->tab_count > 0 ? &p->tabs[p->active_tab].sel_anchor : &p->sel_anchor;
}
static inline int *panel_json_fold_idx(db_panel_t *p) {
    return p->tab_count > 0 ? &p->tabs[p->active_tab].json_fold_idx : &p->json_fold_idx;
}
static inline char **panel_json_fold_text(db_panel_t *p) {
    return p->tab_count > 0 ? &p->tabs[p->active_tab].json_fold_text : &p->json_fold_text;
}
static inline int *panel_json_fold_lines(db_panel_t *p) {
    return p->tab_count > 0 ? &p->tabs[p->active_tab].json_fold_lines : &p->json_fold_lines;
}

/* ---- dashboard ---- */

struct ph_dashboard {
    int         panel_count;
    db_panel_t  panels[PH_DASHBOARD_MAX_PANELS];
    int         focused;
    int         alive;
    const char *status_text;
    bool        quit;
    /* info box */
    int                      info_count;
    ph_dashboard_info_line_t info_lines[PH_DASHBOARD_MAX_INFO_LINES];
    WINDOW                  *info_win;
    /* UI mode */
    db_mode_t       mode;
    db_popup_t      popup;
    WINDOW         *popup_win;
    /* command line */
    char  cmd_buf[256];
    int   cmd_len;
    char  cmd_msg[256];
    int   cmd_msg_frames;
    /* buttons */
    db_button_sel_t btn_selected;
    int             btn_flash;
    /* spawn config for start/stop (borrowed pointers) */
    const ph_serve_config_t  *serve_cfg;
    ph_serve_session_t      **session_ptr;
    /* zoom */
    bool  zoomed;
    /* search */
    char  search_pat[256];
    bool  search_active;
    char  search_buf[256];
    int   search_len;
    /* fuzzy finder (disk-based log search) */
    char   fuzzy_buf[256];
    int    fuzzy_len;
    int    fuzzy_results[MAX_LINES];
    int    fuzzy_result_count;
    int    fuzzy_selected;
    int    fuzzy_saved_scroll;
    int    fuzzy_saved_cursor;
    /* phase 0: file picker */
    char **fuzzy_files;        /* *.json filenames in cwd */
    int    fuzzy_file_count;
    bool   fuzzy_picking;      /* true = picking file, false = searching lines */
    /* phase 1: line search */
    char **fuzzy_disk_lines;   /* loaded from JSON file */
    int    fuzzy_disk_count;
    char   fuzzy_fname[256];   /* file being searched */
    /* json viewer (popup tree browser) */
    db_json_node_t *jv_nodes;
    int             jv_node_count;
    int             jv_cursor;      /* visible-index of cursor */
    int             jv_scroll;      /* first visible row in viewport */
    char            jv_title[256];  /* filename being viewed */
    /* fuzzy excludes (owned copies) */
    char            **fuzzy_excludes;
    int               fuzzy_exclude_count;
    /* shell */
    bool              shell_open;
    int               shell_height;
    db_shell_view_t   shell_views[DB_SHELL_MAX_VIEWS];
    int               shell_view_count;
    int               shell_active_view;
    WINDOW           *shell_win;
};

/* ---- cross-file function declarations ---- */

/* db_ring.c */
int         utf8_seq_len(unsigned char c);
int         clean_line(char *dst, int *vis_width, const char *src, int srclen);
void        ringbuf_push(db_ringbuf_t *rb, const char *text, int len,
                          bool is_stderr);
db_line_t  *ringbuf_get(db_ringbuf_t *rb, int i);
void        ringbuf_destroy(db_ringbuf_t *rb);
void        feed_accum(db_accum_t *acc, db_ringbuf_t *ring,
                       const char *buf, int n, bool is_stderr);
void        feed_accum_multi(db_accum_t *acc, db_ringbuf_t **targets,
                             int ntargets, const char *buf, int n,
                             bool is_stderr);
int         strip_ansi(char *dst, const char *src, int srclen);

/* db_layout.c */
void        layout_panels(ph_dashboard_t *db);

/* db_draw.c */
void        draw_all(ph_dashboard_t *db);
void        draw_info_box(ph_dashboard_t *db);
void        draw_panel(ph_dashboard_t *db, int idx);
void        draw_status_bar(ph_dashboard_t *db);
void        render_line(WINDOW *win, int y, int x,
                        const char *text, int len,
                        int max_cols, bool is_stderr);

/* db_popup.c */
void        open_popup(ph_dashboard_t *db, db_popup_t which);
void        close_popup(ph_dashboard_t *db);
void        draw_popup(ph_dashboard_t *db);

/* db_event.c */
void        collect_events(ph_dashboard_t *db, db_event_t *events,
                           int *nevents, int sig_fd);
void        handle_event(ph_dashboard_t *db, const db_event_t *evt);

/* db_evt_pipe.c */
void        handle_pipe_data(ph_dashboard_t *db, const db_evt_pipe_t *p);
void        handle_pipe_eof(ph_dashboard_t *db, const db_evt_pipe_t *p);

/* db_evt_signal.c */
void        handle_signal(ph_dashboard_t *db);
void        handle_winch(ph_dashboard_t *db);

/* db_evt_child.c */
void        handle_child_exit(ph_dashboard_t *db, const db_evt_child_t *c);
void        reap_children(ph_dashboard_t *db, db_event_t *events, int *nevents);

/* db_evt_key.c */
void        handle_key(ph_dashboard_t *db, int ch);

/* db_evt_tick.c */
void        handle_tick(ph_dashboard_t *db);

/* db_fuzzy.c */
int         fuzzy_score(const char *text, const char *pattern, int patlen);
void        fuzzy_recompute(ph_dashboard_t *db);
void        draw_fuzzy_popup(ph_dashboard_t *db);
void        handle_key_fuzzy(ph_dashboard_t *db, int ch);
bool        fuzzy_scan_json_files(ph_dashboard_t *db);
bool        fuzzy_load_file(ph_dashboard_t *db, const char *path);
void        fuzzy_unload_disk(ph_dashboard_t *db);

/* db_json_fold.c */
void        action_toggle_json_fold(ph_dashboard_t *db);
void        json_fold_cleanup(db_panel_t *p);
int         render_json_fold(WINDOW *win, int start_row, int x,
                             const char *json_text, int max_w, int max_rows);
void        open_json_viewer(ph_dashboard_t *db, const char *path);
void        close_json_viewer(ph_dashboard_t *db);
void        draw_json_viewer(ph_dashboard_t *db);
void        handle_json_viewer_key(ph_dashboard_t *db, int ch);

/* db_shell.c */
void        shell_new_view(ph_dashboard_t *db);
void        shell_focus(ph_dashboard_t *db);
void        shell_next_view(ph_dashboard_t *db);
void        shell_prev_view(ph_dashboard_t *db);
void        shell_close_all(ph_dashboard_t *db);
void        handle_key_shell(ph_dashboard_t *db, int ch);
void        handle_shell_data(ph_dashboard_t *db, const db_evt_shell_t *evt);
void        handle_shell_eof(ph_dashboard_t *db, const db_evt_shell_t *evt);
void        draw_shell(ph_dashboard_t *db);
void        draw_shell_screens(ph_dashboard_t *db);

/* db_lifecycle.c */
void        activate_button(ph_dashboard_t *db);
void        action_start(ph_dashboard_t *db);
void        action_stop(ph_dashboard_t *db);
void        rewire_panels(ph_dashboard_t *db, ph_serve_session_t *s);
void        shutdown_children(ph_dashboard_t *db);
int         compute_worst_exit(ph_dashboard_t *db);
bool        any_child_running(ph_dashboard_t *db);
bool        any_fd_open(ph_dashboard_t *db);
void        set_cmd_msg(ph_dashboard_t *db, const char *msg, int frames);
void        action_clear(ph_dashboard_t *db);
void        action_save(ph_dashboard_t *db, const char *path);
void        action_saveall(ph_dashboard_t *db);
void        action_export_selection(ph_dashboard_t *db);

#endif /* PHOSPHOR_HAS_NCURSES */
#endif /* DB_TYPES_H */

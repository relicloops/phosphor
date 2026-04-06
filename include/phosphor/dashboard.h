#ifndef PHOSPHOR_DASHBOARD_H
#define PHOSPHOR_DASHBOARD_H

#include "phosphor/types.h"
#include "phosphor/error.h"
#include "phosphor/serve.h"

#include <sys/types.h>

/*
 * Serve dashboard -- ncurses TUI for monitoring phosphor serve child
 * processes (neonsignal, neonsignal_redirect, file watcher).
 *
 * Each process gets a panel showing scrolling stdout/stderr output.
 * The dashboard handles terminal resize, keyboard navigation, and
 * clean child process termination.
 *
 * Requires: PHOSPHOR_HAS_NCURSES compile flag.
 */

#define PH_DASHBOARD_MAX_PANELS     3
#define PH_DASHBOARD_MAX_INFO_LINES 8

/* ---- panel IDs ---- */

typedef enum {
    PH_PANEL_NEONSIGNAL = 0,
    PH_PANEL_REDIRECT   = 1,
    PH_PANEL_WATCHER    = 2,
} ph_dashboard_panel_id_t;

/* ---- info line colors ---- */

typedef enum {
    PH_INFO_CYAN = 0,
    PH_INFO_GREEN,
    PH_INFO_YELLOW,
    PH_INFO_RED,
    PH_INFO_DIM,
    PH_INFO_WHITE,
} ph_dashboard_info_color_t;

/* ---- info line (for the serve configuration box) ---- */

typedef struct {
    const char                *label;
    const char                *value;
    ph_dashboard_info_color_t  color;
} ph_dashboard_info_line_t;

/* ---- tab configuration (optional per-panel) ---- */

#define PH_DASHBOARD_MAX_TABS 4

typedef struct {
    const char *name;            /* tab display name, e.g. "live-stream" */
    int         source_stream;   /* 0=stdout, 1=stderr, 2=both */
} ph_dashboard_tab_cfg_t;

/* ---- panel configuration ---- */

typedef struct {
    const char             *name;       /* display name */
    ph_dashboard_panel_id_t id;
    int                     stdout_fd;  /* -1 if not connected */
    int                     stderr_fd;  /* -1 if not connected */
    pid_t                   pid;        /* 0 if not spawned */
    /* tab config (0 = no tabs, legacy single-view behavior) */
    int                    tab_count;
    ph_dashboard_tab_cfg_t tabs[PH_DASHBOARD_MAX_TABS];
} ph_dashboard_panel_cfg_t;

/* ---- dashboard configuration ---- */

typedef struct {
    int                        panel_count;
    ph_dashboard_panel_cfg_t   panels[PH_DASHBOARD_MAX_PANELS];
    const char                *status_text;
    int                        info_count;
    ph_dashboard_info_line_t   info_lines[PH_DASHBOARD_MAX_INFO_LINES];
    /* borrowed pointers for start/stop lifecycle (both may be NULL) */
    const ph_serve_config_t   *serve_cfg;
    ph_serve_session_t       **session_ptr;
} ph_dashboard_config_t;

/* ---- opaque dashboard handle ---- */

typedef struct ph_dashboard ph_dashboard_t;

/* ---- API ---- */

/*
 * ph_dashboard_create -- initialize ncurses and create the dashboard.
 * caller must call ph_dashboard_destroy() when done.
 */
ph_result_t ph_dashboard_create(const ph_dashboard_config_t *cfg,
                                 ph_dashboard_t **out);

/*
 * ph_dashboard_run -- run the event loop (blocks until quit or all exit).
 *
 * monitors child pipes via poll(), renders panels, handles keyboard.
 * sends SIGTERM to children on quit. returns the worst child exit code.
 */
int ph_dashboard_run(ph_dashboard_t *db);

/*
 * ph_dashboard_destroy -- clean up ncurses and free resources.
 */
void ph_dashboard_destroy(ph_dashboard_t *db);

#endif /* PHOSPHOR_DASHBOARD_H */

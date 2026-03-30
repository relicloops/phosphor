#ifdef PHOSPHOR_HAS_NCURSES

#include "phosphor/dashboard.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/signal.h"

#include <curses.h>
#include <errno.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- constants ---- */

#define MAX_LINES       2000    /* ring buffer capacity per panel */
#define MAX_LINE_LEN    4096    /* max bytes per output line */
#define POLL_TIMEOUT_MS 100
#define MAX_FDS         (PH_DASHBOARD_MAX_PANELS * 2 + 1) /* stdout+stderr + signal pipe */

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
};

/* ---- UTF-8 helper ---- */

static int utf8_seq_len(unsigned char c) {
    if (c < 0x80)            return 1;
    if ((c & 0xE0) == 0xC0)  return 2;
    if ((c & 0xF0) == 0xE0)  return 3;
    if ((c & 0xF8) == 0xF0)  return 4;
    return 0;
}

/* ---- line cleaner ---- */

/*
 * clean_line -- clean a raw line for dashboard storage.
 * preserves SGR sequences (\033[...m) and valid UTF-8.
 * strips OSC, non-SGR CSI sequences, and non-printable bytes.
 * writes to dst (must be >= srclen+1). returns byte length.
 * sets *vis_width to the visual column count (excluding escape bytes).
 */
static int clean_line(char *dst, int *vis_width, const char *src, int srclen) {
    int d = 0;
    int vw = 0;
    for (int i = 0; i < srclen; ) {
        unsigned char c = (unsigned char)src[i];

        if (src[i] == '\033') {
            if (i + 1 < srclen && src[i+1] == '[') {
                /* CSI -- check if it ends with 'm' (SGR) */
                int start = i;
                int j = i + 2;
                while (j < srclen && (unsigned char)src[j] < 0x40)
                    j++;
                if (j < srclen && src[j] == 'm') {
                    /* SGR -- keep it */
                    int slen = j + 1 - start;
                    memcpy(dst + d, src + start, (size_t)slen);
                    d += slen;
                    i = j + 1;
                } else {
                    /* non-SGR CSI -- strip */
                    i = (j < srclen) ? j + 1 : srclen;
                }
            } else if (i + 1 < srclen && src[i+1] == ']') {
                /* OSC -- strip until ST or BEL */
                i += 2;
                while (i < srclen) {
                    if (src[i] == '\007') { i++; break; }
                    if (src[i] == '\033' && i + 1 < srclen && src[i+1] == '\\') {
                        i += 2; break;
                    }
                    i++;
                }
            } else {
                /* other ESC -- skip ESC + one char */
                i += 2;
                if (i > srclen) i = srclen;
            }
        } else if (c >= 0x80) {
            /* multi-byte UTF-8 */
            int slen = utf8_seq_len(c);
            if (slen >= 2 && i + slen <= srclen) {
                bool valid = true;
                for (int k = 1; k < slen; k++) {
                    if (((unsigned char)src[i+k] & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    memcpy(dst + d, src + i, (size_t)slen);
                    d += slen;
                    vw++;
                    i += slen;
                } else {
                    i++;
                }
            } else {
                i++;
            }
        } else if (c >= 0x20 || c == '\t') {
            dst[d++] = src[i++];
            vw++;
        } else {
            i++;
        }
    }
    dst[d] = '\0';
    *vis_width = vw;
    return d;
}

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

static void ringbuf_push(db_ringbuf_t *rb, const char *text, int len,
                          bool is_stderr) {
    int idx = (rb->head + rb->count) % MAX_LINES;
    if (rb->count == MAX_LINES) {
        ph_free(rb->lines[rb->head].text);
        rb->head = (rb->head + 1) % MAX_LINES;
    } else {
        rb->count++;
    }
    char *tmp = ph_alloc((size_t)len + 1);
    if (tmp) {
        int vis_w = 0;
        int clean_len = clean_line(tmp, &vis_w, text, len);
        rb->lines[idx].text = tmp;
        rb->lines[idx].len = clean_len;
        rb->lines[idx].vis_width = vis_w;
    } else {
        rb->lines[idx].text = NULL;
        rb->lines[idx].len = 0;
        rb->lines[idx].vis_width = 0;
    }
    rb->lines[idx].is_stderr = is_stderr;
}

static void ringbuf_destroy(db_ringbuf_t *rb) {
    for (int i = 0; i < rb->count; i++) {
        int idx = (rb->head + i) % MAX_LINES;
        ph_free(rb->lines[idx].text);
    }
    rb->head = 0;
    rb->count = 0;
}

static db_line_t *ringbuf_get(db_ringbuf_t *rb, int i) {
    if (i < 0 || i >= rb->count) return NULL;
    return &rb->lines[(rb->head + i) % MAX_LINES];
}

/* ---- line accumulator ---- */

typedef struct {
    char  buf[MAX_LINE_LEN];
    int   pos;
} db_accum_t;

/* ---- panel state ---- */

typedef enum {
    PANEL_RUNNING,
    PANEL_EXITED,
} db_panel_status_t;

typedef struct {
    const char         *name;
    ph_dashboard_panel_id_t id;
    pid_t               pid;
    int                 stdout_fd;
    int                 stderr_fd;
    db_panel_status_t   status;
    int                 exit_code;
    db_ringbuf_t        ring;
    db_accum_t          out_acc;
    db_accum_t          err_acc;
    int                 scroll;   /* offset from bottom; 0 = auto-follow */
    WINDOW             *win;
} db_panel_t;

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
};

/* ---- ANSI color rendering ---- */

/*
 * render_line -- render a stored line (with embedded SGR codes) to an
 * ncurses window. parses SGR sequences and maps them to ncurses color
 * attributes. stops at max_cols visual columns.
 */
static void render_line(WINDOW *win, int y, int x,
                        const char *text, int len,
                        int max_cols, bool is_stderr) {
    int col = 0;
    int base_attr = is_stderr ? (int)COLOR_PAIR(CP_LOG_STDERR) : 0;
    int cur_attr = base_attr;

    wmove(win, y, x);
    wattrset(win, cur_attr);

    int i = 0;
    while (i < len && col < max_cols) {
        if (text[i] == '\033' && i + 1 < len && text[i+1] == '[') {
            /* parse SGR parameters */
            i += 2;
            int params[16];
            int np = 0;
            int val = 0;
            bool has_val = false;

            while (i < len && (unsigned char)text[i] < 0x40) {
                if (text[i] >= '0' && text[i] <= '9') {
                    val = val * 10 + (text[i] - '0');
                    has_val = true;
                } else if (text[i] == ';') {
                    if (np < 16) params[np++] = has_val ? val : 0;
                    val = 0;
                    has_val = false;
                }
                i++;
            }
            if (has_val && np < 16) params[np++] = val;
            if (i < len) i++; /* skip 'm' */

            /* apply SGR codes */
            for (int p = 0; p < np; p++) {
                int code = params[p];
                if (code == 0) {
                    cur_attr = base_attr;
                } else if (code == 1) {
                    cur_attr |= A_BOLD;
                } else if (code == 2) {
                    cur_attr |= A_DIM;
                } else if (code == 4) {
                    cur_attr |= A_UNDERLINE;
                } else if (code == 22) {
                    cur_attr &= ~(A_BOLD | A_DIM);
                } else if (code >= 30 && code <= 37) {
                    cur_attr = (cur_attr & ~((int)A_COLOR))
                             | (int)COLOR_PAIR(CP_ANSI_BASE + code - 30);
                } else if (code == 39) {
                    cur_attr = (cur_attr & ~((int)A_COLOR)) | base_attr;
                } else if (code >= 90 && code <= 97) {
                    cur_attr = (cur_attr & ~((int)A_COLOR))
                             | (int)COLOR_PAIR(CP_ANSI_BASE + code - 90)
                             | A_BOLD;
                }
                /* ignore 256-color (38;5;X) and truecolor (38;2;R;G;B) */
            }
            wattrset(win, cur_attr);
        } else {
            unsigned char c = (unsigned char)text[i];
            if (c >= 0x80) {
                /* UTF-8 multi-byte -- use waddnstr for proper handling */
                int slen = utf8_seq_len(c);
                if (slen >= 2 && i + slen <= len) {
                    waddnstr(win, text + i, slen);
                    i += slen;
                    col++;
                } else {
                    i++;
                }
            } else if (c >= 0x20 || c == '\t') {
                waddch(win, (chtype)c);
                i++;
                col++;
            } else {
                i++;
            }
        }
    }

    wattrset(win, A_NORMAL);
}

/* ---- layout ---- */

static void layout_panels(ph_dashboard_t *db) {
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

    int panel_start = info_h;
    int avail_rows = rows - info_h - 1; /* -1 for status bar */
    if (avail_rows < 3) avail_rows = 3;

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
}

/* ---- drawing ---- */

static int title_color(ph_dashboard_panel_id_t id) {
    switch (id) {
    case PH_PANEL_NEONSIGNAL: return CP_TITLE_NS;
    case PH_PANEL_REDIRECT:   return CP_TITLE_REDIR;
    case PH_PANEL_WATCHER:    return CP_TITLE_WATCH;
    }
    return CP_TITLE_NS;
}

static void draw_info_box(ph_dashboard_t *db) {
    if (!db->info_win || db->info_count <= 0) return;

    int rows, cols;
    getmaxyx(db->info_win, rows, cols);
    (void)rows;
    werase(db->info_win);

    /* border */
    wattron(db->info_win, COLOR_PAIR(CP_INFO_BOX));
    box(db->info_win, 0, 0);
    wattroff(db->info_win, COLOR_PAIR(CP_INFO_BOX));

    /* title */
    wattron(db->info_win, COLOR_PAIR(CP_TITLE_NS) | A_BOLD);
    mvwprintw(db->info_win, 0, 2, " phosphor serve ");
    wattroff(db->info_win, COLOR_PAIR(CP_TITLE_NS) | A_BOLD);

    /* info lines */
    int label_w = 10;
    for (int i = 0; i < db->info_count && i + 1 < rows - 1; i++) {
        const ph_dashboard_info_line_t *line = &db->info_lines[i];

        /* label (dim) */
        wattron(db->info_win, COLOR_PAIR(CP_INFO_LABEL) | A_DIM);
        mvwprintw(db->info_win, i + 1, 2, "%-*s", label_w,
                  line->label ? line->label : "");
        wattroff(db->info_win, COLOR_PAIR(CP_INFO_LABEL) | A_DIM);

        /* value (colored) */
        int cp = 0;
        int extra = 0;
        switch (line->color) {
        case PH_INFO_CYAN:   cp = CP_INFO_CYAN;   break;
        case PH_INFO_GREEN:  cp = CP_INFO_GREEN;   break;
        case PH_INFO_YELLOW: cp = CP_INFO_YELLOW;  break;
        case PH_INFO_RED:    cp = CP_INFO_RED;     break;
        case PH_INFO_DIM:    cp = CP_INFO_DIM; extra = A_DIM; break;
        case PH_INFO_WHITE:  cp = CP_BORDER_NORMAL; break;
        }

        wattron(db->info_win, COLOR_PAIR(cp) | extra);
        /* truncate value to fit in window */
        int max_val = cols - label_w - 4;
        if (line->value) {
            int vlen = (int)strlen(line->value);
            if (vlen > max_val) vlen = max_val;
            waddnstr(db->info_win, line->value, vlen);
        }
        wattroff(db->info_win, COLOR_PAIR(cp) | extra);
    }

    wnoutrefresh(db->info_win);
}

static void draw_panel(ph_dashboard_t *db, int idx) {
    db_panel_t *p = &db->panels[idx];
    if (!p->win) return;

    int rows, cols;
    getmaxyx(p->win, rows, cols);
    werase(p->win);

    /* border */
    int border_cp = (idx == db->focused) ? CP_BORDER_FOCUSED : CP_BORDER_NORMAL;
    wattron(p->win, COLOR_PAIR(border_cp));
    box(p->win, 0, 0);
    wattroff(p->win, COLOR_PAIR(border_cp));

    /* title bar */
    int tcol = title_color(p->id);
    wattron(p->win, COLOR_PAIR(tcol) | A_BOLD);
    mvwprintw(p->win, 0, 2, " %s ", p->name);
    wattroff(p->win, COLOR_PAIR(tcol) | A_BOLD);

    /* status indicator */
    if (p->status == PANEL_RUNNING) {
        wattron(p->win, COLOR_PAIR(CP_STATUS_RUN) | A_BOLD);
        mvwprintw(p->win, 0, cols - 12, " running ");
        wattroff(p->win, COLOR_PAIR(CP_STATUS_RUN) | A_BOLD);
    } else {
        wattron(p->win, COLOR_PAIR(CP_STATUS_EXIT) | A_BOLD);
        mvwprintw(p->win, 0, cols - 14, " exit: %d ", p->exit_code);
        wattroff(p->win, COLOR_PAIR(CP_STATUS_EXIT) | A_BOLD);
    }

    /* log content area */
    int content_h = rows - 2;
    int content_w = cols - 2;
    if (content_h <= 0 || content_w <= 0) {
        wnoutrefresh(p->win);
        return;
    }

    /* visible range from ring buffer */
    int total = p->ring.count;
    int start;
    if (p->scroll == 0) {
        start = total - content_h;
        if (start < 0) start = 0;
    } else {
        start = total - content_h - p->scroll;
        if (start < 0) start = 0;
    }

    for (int r = 0; r < content_h; r++) {
        int li = start + r;
        if (li < 0 || li >= total) continue;
        db_line_t *line = ringbuf_get(&p->ring, li);
        if (!line || !line->text) continue;

        render_line(p->win, r + 1, 1, line->text, line->len,
                    content_w, line->is_stderr);
    }

    wnoutrefresh(p->win);
}

static void draw_status_bar(ph_dashboard_t *db) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    move(rows - 1, 0);
    clrtoeol();

    attron(COLOR_PAIR(CP_STATUSBAR));

    if (db->status_text)
        mvprintw(rows - 1, 1, "%s", db->status_text);

    const char *keys = "q:quit  Tab:focus  Up/Down:scroll  Home/End:top/btm";
    int klen = (int)strlen(keys);
    if (cols - klen - 2 > 0)
        mvprintw(rows - 1, cols - klen - 1, "%s", keys);

    for (int c = getcurx(stdscr); c < cols; c++)
        addch(' ');

    attroff(COLOR_PAIR(CP_STATUSBAR));
    wnoutrefresh(stdscr);
}

static void draw_all(ph_dashboard_t *db) {
    draw_info_box(db);
    for (int i = 0; i < db->panel_count; i++)
        draw_panel(db, i);
    draw_status_bar(db);
    doupdate();
}

/* ---- pipe reading ---- */

static void feed_accum(db_accum_t *acc, db_ringbuf_t *ring,
                       const char *buf, int n, bool is_stderr) {
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            if (acc->pos > 0) {
                ringbuf_push(ring, acc->buf, acc->pos, is_stderr);
                acc->pos = 0;
            }
        } else {
            if (acc->pos < MAX_LINE_LEN - 1)
                acc->buf[acc->pos++] = buf[i];
        }
    }
}

/* ---- event loop ---- */

static void reap_children(ph_dashboard_t *db) {
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->pid <= 0 || p->status != PANEL_RUNNING) continue;

        int status;
        pid_t r = waitpid(p->pid, &status, WNOHANG);
        if (r <= 0) continue;

        if (WIFEXITED(status))
            p->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            p->exit_code = 128 + WTERMSIG(status);

        p->status = PANEL_EXITED;
        p->pid = 0;
        db->alive--;
    }
}

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
        db->panels[i].win = NULL;
        memset(&db->panels[i].ring, 0, sizeof(db_ringbuf_t));
        memset(&db->panels[i].out_acc, 0, sizeof(db_accum_t));
        memset(&db->panels[i].err_acc, 0, sizeof(db_accum_t));
        if (cfg->panels[i].pid > 0) db->alive++;
    }

    /* init ncurses */
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

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

    /* set up self-pipe for signal wakeup */
    int sig_fd = ph_signal_pipe_init();

    draw_all(db);

    while (!db->quit) {
        /* build poll fd set */
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
        (void)pr;

        /* handle signal pipe */
        if (sig_idx >= 0 && (fds[sig_idx].revents & POLLIN)) {
            ph_signal_pipe_drain();

            if (ph_signal_interrupted()) {
                db->quit = true;
                continue;
            }

            if (ph_signal_winch_pending()) {
                ph_signal_winch_clear();
                endwin();
                refresh();
                layout_panels(db);
            }
        }

        /* read pipe data */
        for (int f = 0; f < nfds; f++) {
            if (f == sig_idx) continue;
            if (!(fds[f].revents & (POLLIN | POLLHUP))) continue;

            char buf[4096];
            ssize_t n = read(fds[f].fd, buf, sizeof(buf));
            if (n > 0) {
                int pi = fd_panel[f];
                bool is_err = fd_stderr[f];
                db_accum_t *acc = is_err ? &db->panels[pi].err_acc
                                          : &db->panels[pi].out_acc;
                feed_accum(acc, &db->panels[pi].ring, buf, (int)n, is_err);
            } else if (n == 0) {
                int pi = fd_panel[f];
                if (fd_stderr[f])
                    db->panels[pi].stderr_fd = -1;
                else
                    db->panels[pi].stdout_fd = -1;
                close(fds[f].fd);
            }
        }

        /* reap children */
        reap_children(db);

        /* handle keyboard */
        int ch;
        while ((ch = getch()) != ERR) {
            switch (ch) {
            case 'q':
            case 'Q':
                db->quit = true;
                break;
            case '\t':
                db->focused = (db->focused + 1) % db->panel_count;
                break;
            case KEY_UP:
            case 'k':
                db->panels[db->focused].scroll++;
                if (db->panels[db->focused].scroll > db->panels[db->focused].ring.count)
                    db->panels[db->focused].scroll = db->panels[db->focused].ring.count;
                break;
            case KEY_DOWN:
            case 'j':
                db->panels[db->focused].scroll--;
                if (db->panels[db->focused].scroll < 0)
                    db->panels[db->focused].scroll = 0;
                break;
            case KEY_PPAGE:
                db->panels[db->focused].scroll += 10;
                if (db->panels[db->focused].scroll > db->panels[db->focused].ring.count)
                    db->panels[db->focused].scroll = db->panels[db->focused].ring.count;
                break;
            case KEY_NPAGE:
                db->panels[db->focused].scroll -= 10;
                if (db->panels[db->focused].scroll < 0)
                    db->panels[db->focused].scroll = 0;
                break;
            case KEY_HOME:
                db->panels[db->focused].scroll = db->panels[db->focused].ring.count;
                break;
            case KEY_END:
                db->panels[db->focused].scroll = 0;
                break;
            }
        }

        /* draw */
        draw_all(db);

        /* auto-quit when all children exited and all fds closed */
        if (db->alive == 0) {
            bool any_open = false;
            for (int i = 0; i < db->panel_count; i++) {
                if (db->panels[i].stdout_fd >= 0 ||
                    db->panels[i].stderr_fd >= 0) {
                    any_open = true;
                    break;
                }
            }
            if (!any_open) db->quit = true;
        }
    }

    /* send SIGTERM to any still-running children */
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->pid > 0 && p->status == PANEL_RUNNING) {
            kill(-(p->pid), SIGTERM);
        }
    }

    /* wait for children */
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->pid > 0) {
            int status;
            waitpid(p->pid, &status, 0);
            if (WIFEXITED(status))
                p->exit_code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                p->exit_code = 128 + WTERMSIG(status);
            p->status = PANEL_EXITED;
            p->pid = 0;
        }
    }

    /* restore terminal */
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

    /* compute worst exit code */
    int worst = 0;
    for (int i = 0; i < db->panel_count; i++) {
        if (db->panels[i].exit_code > worst)
            worst = db->panels[i].exit_code;
    }

    return worst;
}

void ph_dashboard_destroy(ph_dashboard_t *db) {
    if (!db) return;

    for (int i = 0; i < db->panel_count; i++) {
        ringbuf_destroy(&db->panels[i].ring);
        if (db->panels[i].win) delwin(db->panels[i].win);
        if (db->panels[i].stdout_fd >= 0) close(db->panels[i].stdout_fd);
        if (db->panels[i].stderr_fd >= 0) close(db->panels[i].stderr_fd);
    }

    if (db->info_win) delwin(db->info_win);

    ph_free(db);
}

#endif /* PHOSPHOR_HAS_NCURSES */

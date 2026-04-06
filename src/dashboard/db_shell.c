#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#ifdef PHOSPHOR_HAS_CJSON
#include "phosphor/json.h"
#include <cJSON.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* ---- PTY helpers ---- */

static void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static pid_t shell_spawn_command(const char *cmd, int *out_master_fd,
                                  int pty_rows, int pty_cols) {
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) return -1;
    if (grantpt(master) < 0)  { close(master); return -1; }
    if (unlockpt(master) < 0) { close(master); return -1; }

    const char *slave_name = ptsname(master);
    if (!slave_name) { close(master); return -1; }

    pid_t pid = fork();
    if (pid < 0) { close(master); return -1; }

    if (pid == 0) {
        /* child */
        close(master);
        setsid();

        int slave = open(slave_name, O_RDWR);
        if (slave < 0) _exit(127);

        struct winsize ws;
        memset(&ws, 0, sizeof(ws));
        ws.ws_row = (unsigned short)pty_rows;
        ws.ws_col = (unsigned short)pty_cols;
        ioctl(slave, TIOCSWINSZ, &ws);

        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO) close(slave);

        const char *sh = getenv("SHELL");
        if (!sh || !*sh) sh = "/bin/sh";
        execlp(sh, sh, "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* parent */
    set_nonblock(master);
    *out_master_fd = master;
    setpgid(pid, pid);
    return pid;
}

/* ---- lifecycle ---- */

/* Ctrl+P: create a new view (opens shell if not open, up to 4 views) */
void shell_new_view(ph_dashboard_t *db) {
    if (!db->shell_open) {
        db->shell_open = true;
        db->shell_height = DB_SHELL_DEFAULT_H;
        db->shell_view_count = 1;
        db->shell_active_view = 0;
        memset(&db->shell_views[0], 0, sizeof(db_shell_view_t));
        db->shell_views[0].active_screen = -1;
        db->mode = DB_MODE_SHELL;
        layout_panels(db);
    } else if (db->shell_view_count < DB_SHELL_MAX_VIEWS) {
        int idx = db->shell_view_count++;
        memset(&db->shell_views[idx], 0, sizeof(db_shell_view_t));
        db->shell_views[idx].active_screen = -1;
        db->shell_active_view = idx;
        db->mode = DB_MODE_SHELL;
    } else {
        set_cmd_msg(db, "shell: max views reached", 20);
    }
}

/* Ctrl+I: focus back to active shell view */
void shell_focus(ph_dashboard_t *db) {
    if (db->shell_open && db->shell_view_count > 0)
        db->mode = DB_MODE_SHELL;
}

/* Ctrl+B / Ctrl+Right: cycle to next shell view tab */
void shell_next_view(ph_dashboard_t *db) {
    if (!db->shell_open || db->shell_view_count <= 1) return;
    db->shell_active_view = (db->shell_active_view + 1) % db->shell_view_count;
    db->mode = DB_MODE_SHELL;
}

/* Ctrl+Left: cycle to previous shell view tab */
void shell_prev_view(ph_dashboard_t *db) {
    if (!db->shell_open || db->shell_view_count <= 1) return;
    db->shell_active_view = (db->shell_active_view + db->shell_view_count - 1)
                          % db->shell_view_count;
    db->mode = DB_MODE_SHELL;
}

void shell_close_all(ph_dashboard_t *db) {
    for (int v = 0; v < db->shell_view_count; v++) {
        db_shell_view_t *view = &db->shell_views[v];
        for (int s = 0; s < view->screen_count; s++) {
            db_shell_screen_t *scr = &view->screens[s];
            if (scr->pid > 0) {
                kill(-(scr->pid), SIGTERM);
                waitpid(scr->pid, NULL, 0);
                scr->pid = 0;
            }
            if (scr->pty_master_fd >= 0) {
                close(scr->pty_master_fd);
                scr->pty_master_fd = -1;
            }
            ringbuf_destroy(&scr->ring);
            if (scr->win) { delwin(scr->win); scr->win = NULL; }
        }
    }
    db->shell_open = false;
    db->shell_view_count = 0;
    db->shell_active_view = 0;
    if (db->shell_win) { delwin(db->shell_win); db->shell_win = NULL; }
    if (db->mode == DB_MODE_SHELL) db->mode = DB_MODE_NORMAL;
    layout_panels(db);
}

static void shell_execute_command(ph_dashboard_t *db, db_shell_view_t *view) {
    if (view->screen_count >= DB_SHELL_MAX_SCREENS) {
        set_cmd_msg(db, "shell: max screens reached", 30);
        return;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int scr_h = rows * 80 / 100;
    int scr_w = cols * 80 / 100;
    if (scr_h < 10) scr_h = 10;
    if (scr_w < 40) scr_w = 40;

    int idx = view->screen_count;
    db_shell_screen_t *scr = &view->screens[idx];
    memset(scr, 0, sizeof(*scr));

    snprintf(scr->title, sizeof(scr->title), "%.*s",
             (int)sizeof(scr->title) - 1, view->input);
    scr->pty_master_fd = -1;
    scr->pid = shell_spawn_command(view->input, &scr->pty_master_fd,
                                    scr_h - 2, scr_w - 2);
    if (scr->pid < 0) {
        set_cmd_msg(db, "shell: spawn failed", 30);
        return;
    }

    scr->status = DB_SCREEN_RUNNING;
    scr->scroll = 0;
    scr->minimized = false;
    scr->win = NULL;

    view->screen_count++;
    view->active_screen = idx;
    view->busy = true;
}

static void shell_goto_screen(db_shell_view_t *view, int idx) {
    if (idx >= 0 && idx < view->screen_count) {
        view->active_screen = idx;
        view->screens[idx].minimized = false;
    }
}

void shell_close_screen(ph_dashboard_t *db, db_shell_view_t *view,
                        int idx) {
    (void)db;
    if (idx < 0 || idx >= view->screen_count) return;

    db_shell_screen_t *scr = &view->screens[idx];
    if (scr->pid > 0) {
        kill(-(scr->pid), SIGTERM);
        waitpid(scr->pid, NULL, 0);
    }
    if (scr->pty_master_fd >= 0) close(scr->pty_master_fd);
    ringbuf_destroy(&scr->ring);
    if (scr->win) { delwin(scr->win); scr->win = NULL; }

    /* compact array */
    for (int i = idx; i < view->screen_count - 1; i++)
        view->screens[i] = view->screens[i + 1];
    view->screen_count--;

    if (view->screen_count == 0)
        view->active_screen = -1;
    else if (view->active_screen >= view->screen_count)
        view->active_screen = view->screen_count - 1;
}

static void shell_save_screen(ph_dashboard_t *db, db_shell_screen_t *scr) {
#ifndef PHOSPHOR_HAS_CJSON
    (void)scr;
    set_cmd_msg(db, "shell save: cJSON not available", 30);
#else
    /* resolve log directory: <log_dir>/shell/ */
    const char *logdir = (db->serve_cfg && db->serve_cfg->ns.log_directory)
                       ? db->serve_cfg->ns.log_directory : ".";
    char shelldir[256];
    snprintf(shelldir, sizeof(shelldir), "%s/shell", logdir);
    mkdir(logdir, 0755);
    mkdir(shelldir, 0755);

    /* build filename: <log_dir>/shell/DD.MM.YYYY.shell.json */
    char date[16];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(date, sizeof(date), "%02d.%02d.%04d",
             tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);

    char fname[256];
    snprintf(fname, sizeof(fname), "%s/%s.shell.json", shelldir, date);

    /* read existing file or create new root */
    cJSON *root = NULL;
    {
        ph_json_t *jr = ph_json_parse_file(fname);
        if (jr) {
            root = cJSON_Duplicate((cJSON *)jr, 1);
            ph_json_destroy(jr);
        }
    }
    if (!root) root = cJSON_CreateObject();
    if (!root) {
        set_cmd_msg(db, "shell save: alloc failed", 30);
        return;
    }

    /* find next slot number */
    int slot_idx = 0;
    {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, root) {
            if (item->string) {
                int v = atoi(item->string);
                if (v >= slot_idx) slot_idx = v + 1;
            }
        }
    }

    char slot_key[16];
    snprintf(slot_key, sizeof(slot_key), "%d", slot_idx);

    /* build slot: { "cmd": "...", "exit": N, "lines": { "0": "...", ... } } */
    cJSON *slot = cJSON_CreateObject();
    if (!slot) { cJSON_Delete(root); set_cmd_msg(db, "shell save: alloc failed", 30); return; }
    cJSON_AddItemToObject(root, slot_key, slot);

    cJSON_AddStringToObject(slot, "cmd", scr->title);
    cJSON_AddNumberToObject(slot, "exit", scr->exit_code);

    cJSON *lines = cJSON_CreateObject();
    if (lines) {
        cJSON_AddItemToObject(slot, "lines", lines);
        char stripped[MAX_LINE_LEN];
        char idx_str[16];
        int count = scr->ring.count;
        int saved = 0;
        for (int i = 0; i < count; i++) {
            db_line_t *line = ringbuf_get(&scr->ring, i);
            if (!line || !line->text) continue;
            int slen = strip_ansi(stripped, line->text, line->len);
            stripped[slen] = '\0';
            snprintf(idx_str, sizeof(idx_str), "%d", saved);
            cJSON_AddStringToObject(lines, idx_str, stripped);
            saved++;
        }
    }

    /* write */
    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    if (out) {
        FILE *f = fopen(fname, "w");
        if (f) {
            fwrite(out, 1, strlen(out), f);
            fclose(f);
        }
        free(out);
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "saved [%d] to %s", slot_idx, fname);
    set_cmd_msg(db, msg, 30);
#endif /* PHOSPHOR_HAS_CJSON */
}

/* ---- shell data handlers ---- */

void handle_shell_data(ph_dashboard_t *db, const db_evt_shell_t *evt) {
    if (evt->view_idx < 0 || evt->view_idx >= db->shell_view_count) return;
    db_shell_view_t *view = &db->shell_views[evt->view_idx];
    if (evt->screen_idx < 0 || evt->screen_idx >= view->screen_count) return;

    db_shell_screen_t *scr = &view->screens[evt->screen_idx];
    int old_count = scr->ring.count;
    feed_accum(&scr->accum, &scr->ring, evt->buf, evt->len, false);

    /* freeze viewport when scrolled */
    if (scr->scroll > 0) {
        int added = scr->ring.count - old_count;
        if (added > 0) scr->scroll += added;
    }
}

void handle_shell_eof(ph_dashboard_t *db, const db_evt_shell_t *evt) {
    if (evt->view_idx < 0 || evt->view_idx >= db->shell_view_count) return;
    db_shell_view_t *view = &db->shell_views[evt->view_idx];
    if (evt->screen_idx < 0 || evt->screen_idx >= view->screen_count) return;

    db_shell_screen_t *scr = &view->screens[evt->screen_idx];
    if (scr->pty_master_fd >= 0) {
        close(scr->pty_master_fd);
        scr->pty_master_fd = -1;
    }
}

/* ---- key handler ---- */

void handle_key_shell(ph_dashboard_t *db, int ch) {
    if (db->shell_view_count == 0) return;
    db_shell_view_t *view = &db->shell_views[db->shell_active_view];

    /* Esc: minimize all screens, return to normal mode */
    if (ch == 27) {
        for (int s = 0; s < view->screen_count; s++)
            view->screens[s].minimized = true;
        db->mode = DB_MODE_NORMAL;
        touchwin(stdscr);
        return;
    }

    /* Ctrl+D: enter command mode from shell */
    if (ch == 0x04) {
        db->mode = DB_MODE_COMMAND;
        db->cmd_len = 0;
        db->cmd_buf[0] = '\0';
        return;
    }

    /* check if a non-minimized screen is visible */
    bool screen_visible = false;
    db_shell_screen_t *active_scr = NULL;
    if (view->active_screen >= 0 && view->active_screen < view->screen_count) {
        active_scr = &view->screens[view->active_screen];
        if (!active_scr->minimized)
            screen_visible = true;
    }

    /* Ctrl+N (0x0E): cycle to next screen */
    if (ch == 0x0E && view->screen_count > 0) {
        int next = (view->active_screen + 1) % view->screen_count;
        shell_goto_screen(view, next);
        return;
    }

    if (screen_visible && active_scr) {
        switch (ch) {
        case 0x18: /* Ctrl+X: minimize + deactivate screen (keep in list) */
            active_scr->minimized = true;
            if (active_scr->win) { delwin(active_scr->win); active_scr->win = NULL; }
            view->active_screen = -1;
            clearok(stdscr, TRUE);
            return;
        case 0x17: /* Ctrl+W: save screen output */
            shell_save_screen(db, active_scr);
            return;
        case KEY_UP: case 'k':
            if (active_scr->scroll < active_scr->ring.count)
                active_scr->scroll++;
            return;
        case KEY_DOWN: case 'j':
            if (active_scr->scroll > 0)
                active_scr->scroll--;
            return;
        case KEY_PPAGE:
            active_scr->scroll += 20;
            if (active_scr->scroll > active_scr->ring.count)
                active_scr->scroll = active_scr->ring.count;
            return;
        case KEY_NPAGE:
            active_scr->scroll -= 20;
            if (active_scr->scroll < 0)
                active_scr->scroll = 0;
            return;
        default:
            break;
        }
    }

    /* input line editing */
    switch (ch) {
    case '\n':
    case '\r':
    case KEY_ENTER:
        if (view->input_len > 0 && !view->busy) {
            view->input[view->input_len] = '\0';
            shell_execute_command(db, view);
            view->input_len = 0;
            view->input_cursor = 0;
        }
        break;

    case KEY_BACKSPACE:
    case 127:
    case '\b':
        if (view->input_cursor > 0) {
            memmove(view->input + view->input_cursor - 1,
                    view->input + view->input_cursor,
                    (size_t)(view->input_len - view->input_cursor));
            view->input_len--;
            view->input_cursor--;
        }
        break;

    case KEY_LEFT:
        if (view->input_cursor > 0) view->input_cursor--;
        break;
    case KEY_RIGHT:
        if (view->input_cursor < view->input_len) view->input_cursor++;
        break;
    case KEY_HOME:
        view->input_cursor = 0;
        break;
    case KEY_END:
        view->input_cursor = view->input_len;
        break;

    default:
        if (ch >= 0x20 && ch < 0x7f
            && view->input_len < DB_SHELL_INPUT_LEN - 1
            && !view->busy) {
            memmove(view->input + view->input_cursor + 1,
                    view->input + view->input_cursor,
                    (size_t)(view->input_len - view->input_cursor));
            view->input[view->input_cursor] = (char)ch;
            view->input_len++;
            view->input_cursor++;
        }
        break;
    }
}

/* ---- drawing: shell panel ---- */

void draw_shell(ph_dashboard_t *db) {
    if (!db->shell_open || !db->shell_win) return;

    WINDOW *w = db->shell_win;
    int rows, cols;
    getmaxyx(w, rows, cols);
    werase(w);

    /* border */
    int bcp = (db->mode == DB_MODE_SHELL) ? CP_SHELL_BORDER : CP_BORDER_NORMAL;
    wattron(w, COLOR_PAIR(bcp));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(bcp));

    /* title */
    wattron(w, COLOR_PAIR(CP_SHELL_PROMPT) | A_BOLD);
    mvwprintw(w, 0, 2, " shell ");
    wattroff(w, COLOR_PAIR(CP_SHELL_PROMPT) | A_BOLD);

    /* view tabs */
    int tx = 10;
    for (int v = 0; v < db->shell_view_count && tx < cols - 10; v++) {
        if (v > 0) {
            wattron(w, COLOR_PAIR(CP_SHELL_TAB_INACTIVE) | A_DIM);
            mvwprintw(w, 0, tx, "|");
            wattroff(w, COLOR_PAIR(CP_SHELL_TAB_INACTIVE) | A_DIM);
            tx++;
        }
        int cp = (v == db->shell_active_view)
               ? CP_SHELL_TAB_ACTIVE : CP_SHELL_TAB_INACTIVE;
        int attr = (v == db->shell_active_view) ? A_BOLD : A_DIM;
        wattron(w, COLOR_PAIR(cp) | attr);
        mvwprintw(w, 0, tx, " %d ", v + 1);
        wattroff(w, COLOR_PAIR(cp) | attr);
        tx += 4;
    }

    if (db->shell_view_count == 0) {
        wnoutrefresh(w);
        return;
    }

    db_shell_view_t *view = &db->shell_views[db->shell_active_view];

    /* screen list (rows 1..rows-3) */
    int list_end = rows - 2;
    int sy = 1;
    for (int s = 0; s < view->screen_count && sy < list_end; s++) {
        db_shell_screen_t *scr = &view->screens[s];
        char marker;
        if (scr->minimized) marker = '-';
        else if (s == view->active_screen) marker = '>';
        else marker = ' ';

        int scp = (scr->status == DB_SCREEN_RUNNING)
                 ? CP_STATUS_RUN : CP_STATUS_EXIT;
        wattron(w, COLOR_PAIR(scp));
        mvwprintw(w, sy, 1, "%c[%d] %.*s", marker, s + 1,
                  cols - 8, scr->title);
        wattroff(w, COLOR_PAIR(scp));
        sy++;
    }

    /* input line (second to last row) */
    int input_y = rows - 2;
    wattron(w, COLOR_PAIR(CP_SHELL_PROMPT) | A_BOLD);
    mvwprintw(w, input_y, 1, "$ ");
    wattroff(w, COLOR_PAIR(CP_SHELL_PROMPT) | A_BOLD);

    if (view->busy) {
        wattron(w, COLOR_PAIR(CP_STATUS_EXIT) | A_DIM);
        wprintw(w, "(busy)");
        wattroff(w, COLOR_PAIR(CP_STATUS_EXIT) | A_DIM);
    } else {
        wattron(w, COLOR_PAIR(CP_SHELL_INPUT));
        waddnstr(w, view->input, view->input_len);
        wattroff(w, COLOR_PAIR(CP_SHELL_INPUT));

        /* cursor */
        if (db->mode == DB_MODE_SHELL) {
            int cur_x = 3 + view->input_cursor;
            if (cur_x < cols - 1) {
                wattron(w, A_REVERSE);
                mvwaddch(w, input_y, cur_x,
                         view->input_cursor < view->input_len
                         ? (chtype)(unsigned char)view->input[view->input_cursor]
                         : ' ');
                wattroff(w, A_REVERSE);
            }
        }
    }

    wnoutrefresh(w);
}

/* ---- drawing: screen overlay ---- */

void draw_shell_screens(ph_dashboard_t *db) {
    if (!db->shell_open || db->shell_view_count == 0) return;

    db_shell_view_t *view = &db->shell_views[db->shell_active_view];
    if (view->active_screen < 0 || view->active_screen >= view->screen_count)
        return;

    db_shell_screen_t *scr = &view->screens[view->active_screen];
    if (scr->minimized) return;

    /* compute overlay geometry -- centered, 80% of terminal */
    int srows, scols;
    getmaxyx(stdscr, srows, scols);
    int h = srows * 80 / 100;
    int w = scols * 80 / 100;
    if (h < 10) h = 10;
    if (w < 40) w = 40;
    if (h > srows - 2) h = srows - 2;
    if (w > scols - 4) w = scols - 4;
    int y = (srows - h) / 2;
    int x = (scols - w) / 2;

    if (scr->win) delwin(scr->win);
    scr->win = newwin(h, w, y, x);

    WINDOW *win = scr->win;
    if (!win) return;
    werase(win);

    /* border */
    wattron(win, COLOR_PAIR(CP_SCREEN_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(CP_SCREEN_BORDER));

    /* title */
    wattron(win, COLOR_PAIR(CP_SCREEN_TITLE) | A_BOLD);
    mvwprintw(win, 0, 2, " %.*s ", w - 20, scr->title);
    wattroff(win, COLOR_PAIR(CP_SCREEN_TITLE) | A_BOLD);

    /* status */
    if (scr->status == DB_SCREEN_RUNNING) {
        wattron(win, COLOR_PAIR(CP_STATUS_RUN) | A_BOLD);
        mvwprintw(win, 0, w - 12, " running ");
        wattroff(win, COLOR_PAIR(CP_STATUS_RUN) | A_BOLD);
    } else {
        wattron(win, COLOR_PAIR(CP_STATUS_EXIT) | A_BOLD);
        mvwprintw(win, 0, w - 14, " exit: %d ", scr->exit_code);
        wattroff(win, COLOR_PAIR(CP_STATUS_EXIT) | A_BOLD);
    }

    /* output content */
    int content_h = h - 2;
    int content_w = w - 2;
    int total = scr->ring.count;
    int start;
    if (scr->scroll == 0) {
        start = total - content_h;
        if (start < 0) start = 0;
    } else {
        start = total - content_h - scr->scroll;
        if (start < 0) start = 0;
    }

    for (int r = 0; r < content_h && (start + r) < total; r++) {
        db_line_t *line = ringbuf_get(&scr->ring, start + r);
        if (line && line->text)
            render_line(win, r + 1, 1, line->text, line->len,
                        content_w, line->is_stderr);
    }

    /* footer */
    char fbuf[80];
    snprintf(fbuf, sizeof(fbuf), " %d/%d  Ctrl+N:next  Ctrl+X:close  Esc:back ",
             view->active_screen + 1, view->screen_count);
    wattron(win, COLOR_PAIR(CP_SCREEN_BORDER) | A_DIM);
    mvwprintw(win, h - 1, 2, "%s", fbuf);
    wattroff(win, COLOR_PAIR(CP_SCREEN_BORDER) | A_DIM);

    wnoutrefresh(win);
}

#endif /* PHOSPHOR_HAS_NCURSES */

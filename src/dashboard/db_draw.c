#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"

#include <stdio.h>
#include <string.h>

/* ---- ANSI color rendering ---- */

void render_line(WINDOW *win, int y, int x,
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
                /* UTF-8 multi-byte */
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

/* ---- helpers ---- */

static int title_color(ph_dashboard_panel_id_t id) {
    switch (id) {
    case PH_PANEL_NEONSIGNAL: return CP_TITLE_NS;
    case PH_PANEL_REDIRECT:   return CP_TITLE_REDIR;
    case PH_PANEL_WATCHER:    return CP_TITLE_WATCH;
    }
    return CP_TITLE_NS;
}

/* ---- info box ---- */

void draw_info_box(ph_dashboard_t *db) {
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
    wattron(db->info_win, COLOR_PAIR(CP_TITLE_SERVE) | A_BOLD);
    mvwprintw(db->info_win, 0, 2, " phosphor serve ");
    wattroff(db->info_win, COLOR_PAIR(CP_TITLE_SERVE) | A_BOLD);

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

/* ---- panel ---- */

void draw_panel(ph_dashboard_t *db, int idx) {
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

    /* tab indicators */
    if (p->tab_count > 0) {
        int tx = 2 + (int)strlen(p->name) + 2 + 1;
        for (int t = 0; t < p->tab_count && tx < cols - 16; t++) {
            if (t > 0) {
                wattron(p->win, COLOR_PAIR(CP_TAB_INACTIVE) | A_DIM);
                mvwprintw(p->win, 0, tx, "|");
                wattroff(p->win, COLOR_PAIR(CP_TAB_INACTIVE) | A_DIM);
                tx++;
            }
            if (t == p->active_tab) {
                wattron(p->win, COLOR_PAIR(CP_TAB_ACTIVE) | A_BOLD);
                mvwprintw(p->win, 0, tx, " %s ", p->tabs[t].name);
                wattroff(p->win, COLOR_PAIR(CP_TAB_ACTIVE) | A_BOLD);
            } else {
                wattron(p->win, COLOR_PAIR(CP_TAB_INACTIVE) | A_DIM);
                mvwprintw(p->win, 0, tx, " %s ", p->tabs[t].name);
                wattroff(p->win, COLOR_PAIR(CP_TAB_INACTIVE) | A_DIM);
            }
            tx += (int)strlen(p->tabs[t].name) + 2;
        }
    }

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

    /* visible range from ring buffer (via tab accessors) */
    db_ringbuf_t *ring = panel_ring(p);
    int total = ring->count;
    int scr = *panel_scroll(p);
    int cur = *panel_cursor(p);
    int sel = *panel_sel_anchor(p);
    int fold_idx = *panel_json_fold_idx(p);
    char *fold_text = *panel_json_fold_text(p);

    int start;
    if (scr == 0) {
        start = total - content_h;
        if (start < 0) start = 0;
    } else {
        start = total - content_h - scr;
        if (start < 0) start = 0;
    }

    /* bottom status: PID (left) + line count (right) on bottom border */
    int brow = rows - 1;
    if (p->status == PANEL_RUNNING && p->pid > 0) {
        wattron(p->win, COLOR_PAIR(CP_STATUS_RUN) | A_DIM);
        mvwprintw(p->win, brow, 2, " pid:%d ", (int)p->pid);
        wattroff(p->win, COLOR_PAIR(CP_STATUS_RUN) | A_DIM);
    }
    {
        char lbuf[32];
        int llen = snprintf(lbuf, sizeof(lbuf), " %d lines ", total);
        int lx = cols - llen - 1;
        if (lx > 2) {
            wattron(p->win, COLOR_PAIR(border_cp) | A_DIM);
            mvwprintw(p->win, brow, lx, "%s", lbuf);
            wattroff(p->win, COLOR_PAIR(border_cp) | A_DIM);
        }
    }

    int r = 0;
    int li = start;
    while (r < content_h && li < total) {
        /* JSON fold: expanded lines replace original */
        if (li == fold_idx && fold_text) {
            int avail = content_h - r;
            int used = render_json_fold(p->win, r + 1, 1,
                                         fold_text, content_w, avail);
            /* cursor highlight on first fold row */
            if (cur == li)
                mvwchgat(p->win, r + 1, 1, content_w,
                         A_REVERSE, CP_CURSOR_LINE, NULL);
            r += used;
            li++;
            continue;
        }

        db_line_t *line = ringbuf_get(ring, li);
        if (line && line->text) {
            render_line(p->win, r + 1, 1, line->text, line->len,
                        content_w, line->is_stderr);

            /* search highlight */
            if (db->search_active && db->search_pat[0] != '\0'
                && strstr(line->text, db->search_pat))
                mvwchgat(p->win, r + 1, 1, content_w,
                         A_REVERSE, CP_SEARCH_MATCH, NULL);

            /* selection highlight (overwrites cursor) */
            if (sel >= 0 && cur >= 0) {
                int sel_lo = sel < cur ? sel : cur;
                int sel_hi = sel > cur ? sel : cur;
                if (li >= sel_lo && li <= sel_hi)
                    mvwchgat(p->win, r + 1, 1, content_w,
                             A_BOLD, CP_SELECTED_LINE, NULL);
            } else if (cur == li) {
                mvwchgat(p->win, r + 1, 1, content_w,
                         A_REVERSE, CP_CURSOR_LINE, NULL);
            }
        }
        r++;
        li++;
    }

    wnoutrefresh(p->win);
}

/* ---- buttons ---- */

/* " Start " (7) + " " (1) + " Stop " (6) = 14 chars + 1 margin = 15 */
#define BTN_TOTAL_W 15

static void draw_buttons(ph_dashboard_t *db, int row, int cols) {
    bool running = any_child_running(db);
    bool can_start = !running && db->serve_cfg != NULL;
    bool can_stop  = running;

    int stop_x  = cols - 7;      /* " Stop " right-aligned with 1-char margin */
    int start_x = stop_x - 8;    /* " Start " + 1 gap */
    if (start_x < 0) return;

    /* Start button */
    int start_cp;
    if (!can_start)
        start_cp = CP_BTN_DISABLED;
    else if (db->btn_selected == DB_BTN_START)
        start_cp = CP_BTN_SELECTED;
    else
        start_cp = CP_BTN_START_ACTIVE;

    attron(COLOR_PAIR(start_cp) | A_BOLD);
    mvprintw(row, start_x, " Start ");
    attroff(COLOR_PAIR(start_cp) | A_BOLD);

    /* Stop button */
    int stop_cp;
    if (!can_stop)
        stop_cp = CP_BTN_DISABLED;
    else if (db->btn_selected == DB_BTN_STOP)
        stop_cp = CP_BTN_SELECTED;
    else
        stop_cp = CP_BTN_STOP_ACTIVE;

    attron(COLOR_PAIR(stop_cp) | A_BOLD);
    mvprintw(row, stop_x, " Stop ");
    attroff(COLOR_PAIR(stop_cp) | A_BOLD);
}

/* ---- status bar ---- */

void draw_status_bar(ph_dashboard_t *db) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* fill entire status bar with background color first */
    attron(COLOR_PAIR(CP_STATUSBAR));
    move(rows - 1, 0);
    for (int c = 0; c < cols; c++)
        addch(' ');

    /* phosphor brand badge: [space][symbol][space] all green-on-black */
    attroff(COLOR_PAIR(CP_STATUSBAR));
    attron(COLOR_PAIR(CP_SYMBOL));
    mvprintw(rows - 1, 0, " \xF0\x9F\x9C\xBD ");  /* U+1F73D padded */
    attroff(COLOR_PAIR(CP_SYMBOL));
    attron(COLOR_PAIR(CP_STATUSBAR));

    /* sym_end = first column after badge (1 pad + 2 symbol + 1 pad) */
    int sym_end = 4;

    if (db->mode == DB_MODE_COMMAND) {
        /* command entry after symbol: ":" + buffer text */
        mvprintw(rows - 1, sym_end, ":%.*s", db->cmd_len, db->cmd_buf);

        /* block cursor at the insertion point */
        int cur_col = sym_end + 1 + db->cmd_len;
        if (cur_col < cols) {
            attron(A_REVERSE);
            mvaddch(rows - 1, cur_col, ' ');
            attroff(A_REVERSE);
        }

        const char *hint = "Esc:cancel  Enter:execute";
        int hlen = (int)strlen(hint);
        if (cols - hlen - 2 > 0)
            mvprintw(rows - 1, cols - hlen - 1, "%s", hint);
    } else if (db->mode == DB_MODE_FUZZY) {
        const char *hint = db->fuzzy_picking
            ? "fuzzy-log: file picker"
            : "fuzzy-log: line search";
        mvprintw(rows - 1, sym_end + 1, "%s", hint);
    } else if (db->mode == DB_MODE_SEARCH) {
        /* search input after symbol: "/" + buffer text */
        mvprintw(rows - 1, sym_end, "/%.*s", db->search_len, db->search_buf);

        /* block cursor at the insertion point */
        int cur_col = sym_end + 1 + db->search_len;
        if (cur_col < cols) {
            attron(A_REVERSE);
            mvaddch(rows - 1, cur_col, ' ');
            attroff(A_REVERSE);
        }

        const char *hint = "Esc:cancel  Enter:search";
        int hlen = (int)strlen(hint);
        if (cols - hlen - 2 > 0)
            mvprintw(rows - 1, cols - hlen - 1, "%s", hint);
    } else if (db->mode == DB_MODE_POPUP
               && db->popup == DB_POPUP_JSON_VIEWER) {
        mvprintw(rows - 1, sym_end + 1, "%s", db->jv_title);

        const char *hint = "z:fold  l/h:open/close  j/k:navigate  Esc:back  q:close";
        int hlen = (int)strlen(hint);
        if (cols - hlen - 2 > 0)
            mvprintw(rows - 1, cols - hlen - 1, "%s", hint);
    } else {

        /* command message after symbol (when active) */
        if (db->cmd_msg_frames > 0)
            mvprintw(rows - 1, sym_end + 1, "%s", db->cmd_msg);

        /* keybindings hint (truncated for buttons space) */
        int btn_space = db->serve_cfg ? BTN_TOTAL_W + 2 : 0;
        const char *keys;
        char keys_buf[128];
        db_panel_t *fp = &db->panels[db->focused];
        int fp_sel = *panel_sel_anchor(fp);
        if (fp_sel >= 0)
            keys = "V:export  Esc:cancel  j/k:extend  ?:help";
        else if (db->zoomed && db->search_active)
            keys = "v:select  f:unzoom  n/N:search  ?:help  :cmd";
        else if (db->zoomed)
            keys = "v:select  f:unzoom  /:search  ?:help  :cmd";
        else if (db->search_active)
            keys = "v:select  n/N:search  ?:help  :cmd";
        else if (fp->tab_count > 0) {
            snprintf(keys_buf, sizeof(keys_buf),
                     "q:quit  v:select  1-%d:tab  g:fuzzy-log  ?:help  :cmd",
                     fp->tab_count);
            keys = keys_buf;
        } else
            keys = "q:quit  v:select  g:fuzzy-log  ?:help  :cmd";
        int klen = (int)strlen(keys);
        int key_x = cols - klen - btn_space - 1;
        if (key_x > 0)
            mvprintw(rows - 1, key_x, "%s", keys);

        /* buttons (only in normal mode, only if serve_cfg present) */
        if (db->mode == DB_MODE_NORMAL && db->serve_cfg)
            draw_buttons(db, rows - 1, cols);
    }

    attroff(COLOR_PAIR(CP_STATUSBAR));
    wnoutrefresh(stdscr);
}

/* ---- draw all ---- */

void draw_all(ph_dashboard_t *db) {
    if (!db->zoomed)
        draw_info_box(db);
    for (int i = 0; i < db->panel_count; i++) {
        if (!db->panels[i].win) continue;
        draw_panel(db, i);
    }

    if (db->shell_open)
        draw_shell(db);

    draw_status_bar(db);

    if (db->shell_open)
        draw_shell_screens(db);

    if (db->mode == DB_MODE_FUZZY)
        draw_fuzzy_popup(db);

    if (db->mode == DB_MODE_POPUP)
        draw_popup(db);

    doupdate();
}

#endif /* PHOSPHOR_HAS_NCURSES */

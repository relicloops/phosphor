#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"

#include <curses.h>
#include <stdio.h>
#include <string.h>

/* ---- cursor helpers ---- */

static int panel_content_h(ph_dashboard_t *db) {
    if (!db->panels[db->focused].win) return 1;
    int rows, cols;
    getmaxyx(db->panels[db->focused].win, rows, cols);
    (void)cols;
    int h = rows - 2;
    return h > 0 ? h : 1;
}

static void ensure_cursor_visible(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];
    int *cur = panel_cursor(p);
    int *scr = panel_scroll(p);
    if (*cur < 0) return;

    int total = panel_ring(p)->count;
    int ch = panel_content_h(db);

    /* compute view_top from scroll */
    int view_top;
    if (*scr == 0)
        view_top = total - ch;
    else
        view_top = total - ch - *scr;
    if (view_top < 0) view_top = 0;

    if (*cur < view_top) {
        *scr = total - ch - *cur;
    } else if (*cur >= view_top + ch) {
        *scr = total - ch - *cur;
    }

    if (*scr < 0) *scr = 0;
    if (*scr > total) *scr = total;
}

/* ---- command dispatch ---- */

static void dispatch_cmd(ph_dashboard_t *db) {
    const char *s = db->cmd_buf;
    while (*s == ' ') s++;

    if (*s == '\0') return;

    if (strncmp(s, "start", 5) == 0 && (s[5] == '\0' || s[5] == ' ')) {
        if (any_child_running(db))
            set_cmd_msg(db, "start: server already running", 30);
        else
            action_start(db);
    } else if (strncmp(s, "stop", 4) == 0 && (s[4] == '\0' || s[4] == ' ')) {
        if (!any_child_running(db))
            set_cmd_msg(db, "stop: server already stopped", 30);
        else
            action_stop(db);
    } else if (strncmp(s, "clear", 5) == 0 && (s[5] == '\0' || s[5] == ' ')) {
        action_clear(db);
    } else if (strncmp(s, "saveall", 7) == 0 && (s[7] == '\0' || s[7] == ' ')) {
        action_saveall(db);
    } else if (strncmp(s, "save", 4) == 0 && (s[4] == '\0' || s[4] == ' ')) {
        const char *p = s + 4;
        while (*p == ' ') p++;
        if (*p == '\0')
            set_cmd_msg(db, "Usage: :save <path>", 30);
        else
            action_save(db, p);
    } else if (strncmp(s, "shell", 5) == 0 && (s[5] == '\0' || s[5] == ' ')) {
        shell_new_view(db);
    } else if (strncmp(s, "shellclose", 10) == 0 && (s[10] == '\0' || s[10] == ' ')) {
        shell_close_all(db);
    } else if (strncmp(s, "filament", 8) == 0 && (s[8] == '\0' || s[8] == ' ')) {
        set_cmd_msg(db, "filament: not yet implemented", 40);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "E492: Not an editor command: %s", s);
        set_cmd_msg(db, msg, 30);
    }
}

/* ---- popup mode ---- */

static void handle_key_popup(ph_dashboard_t *db, int ch) {
    if (db->popup == DB_POPUP_JSON_VIEWER) {
        handle_json_viewer_key(db, ch);
        return;
    }

    switch (ch) {
    case 27:  /* Esc */
    case '\n':
    case '\r':
    case KEY_ENTER:
    case 'q':
        close_popup(db);
        break;
    case 'c':
        if (db->popup == DB_POPUP_HELP)
            open_popup(db, DB_POPUP_COMMANDS);
        break;
    case 'h':
        if (db->popup == DB_POPUP_HELP)
            open_popup(db, DB_POPUP_PH_HELP);
        break;
    case '?':
        if (db->popup == DB_POPUP_COMMANDS || db->popup == DB_POPUP_PH_HELP)
            open_popup(db, DB_POPUP_HELP);
        break;
    default:
        break;
    }
}

/* ---- command mode ---- */

static void handle_key_command(ph_dashboard_t *db, int ch) {
    if (ch == 27) { /* Esc */
        db->mode = DB_MODE_NORMAL;
        db->cmd_len = 0;
        db->cmd_buf[0] = '\0';
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        db->cmd_buf[db->cmd_len] = '\0';
        dispatch_cmd(db);
        /* don't clobber mode if dispatch changed it (e.g. :shell) */
        if (db->mode == DB_MODE_COMMAND)
            db->mode = DB_MODE_NORMAL;
        db->cmd_len = 0;
        db->cmd_buf[0] = '\0';
    } else if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
               && db->cmd_len > 0) {
        db->cmd_buf[--db->cmd_len] = '\0';
    } else if (ch >= 0x20 && ch < 0x7f && db->cmd_len < 254) {
        db->cmd_buf[db->cmd_len++] = (char)ch;
        db->cmd_buf[db->cmd_len]   = '\0';
    }
}

/* ---- search mode ---- */

static void handle_key_search(ph_dashboard_t *db, int ch) {
    if (ch == 27) { /* Esc -- cancel, keep old pattern */
        db->mode = DB_MODE_NORMAL;
        db->search_len = 0;
        db->search_buf[0] = '\0';
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        db->search_buf[db->search_len] = '\0';
        if (db->search_len > 0) {
            memcpy(db->search_pat, db->search_buf, (size_t)db->search_len + 1);
            db->search_active = true;
        } else {
            db->search_active = false;
            db->search_pat[0] = '\0';
        }
        db->mode = DB_MODE_NORMAL;
        db->search_len = 0;
        db->search_buf[0] = '\0';
    } else if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
               && db->search_len > 0) {
        db->search_buf[--db->search_len] = '\0';
    } else if (ch >= 0x20 && ch < 0x7f && db->search_len < 254) {
        db->search_buf[db->search_len++] = (char)ch;
        db->search_buf[db->search_len]   = '\0';
    }
}

static void search_jump_next(ph_dashboard_t *db) {
    if (!db->search_active || db->search_pat[0] == '\0') return;

    db_panel_t *p = &db->panels[db->focused];
    db_ringbuf_t *ring = panel_ring(p);
    int *scr = panel_scroll(p);
    int total = ring->count;
    if (total == 0) { set_cmd_msg(db, "Pattern not found", 20); return; }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    int info_h = (db->info_count > 0) ? db->info_count + 2 : 0;
    int content_h = rows - info_h - 1 - 2;
    if (content_h < 1) content_h = 1;

    int view_top;
    if (*scr == 0)
        view_top = total - content_h;
    else
        view_top = total - content_h - *scr;
    if (view_top < 0) view_top = 0;

    for (int i = view_top - 1; i >= 0; i--) {
        db_line_t *line = ringbuf_get(ring, i);
        if (line && line->text && strstr(line->text, db->search_pat)) {
            *scr = total - content_h - i;
            if (*scr < 0) *scr = 0;
            if (*scr > total) *scr = total;
            return;
        }
    }
    set_cmd_msg(db, "Pattern not found", 20);
}

static void search_jump_prev(ph_dashboard_t *db) {
    if (!db->search_active || db->search_pat[0] == '\0') return;

    db_panel_t *p = &db->panels[db->focused];
    db_ringbuf_t *ring = panel_ring(p);
    int *scr = panel_scroll(p);
    int total = ring->count;
    if (total == 0) { set_cmd_msg(db, "Pattern not found", 20); return; }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    int info_h = (db->info_count > 0) ? db->info_count + 2 : 0;
    int content_h = rows - info_h - 1 - 2;
    if (content_h < 1) content_h = 1;

    int view_top;
    if (*scr == 0)
        view_top = total - content_h;
    else
        view_top = total - content_h - *scr;
    if (view_top < 0) view_top = 0;

    int start = view_top + content_h;
    if (start >= total) start = total - 1;
    for (int i = start; i < total; i++) {
        db_line_t *line = ringbuf_get(ring, i);
        if (line && line->text && strstr(line->text, db->search_pat)) {
            *scr = total - content_h - i;
            if (*scr < 0) *scr = 0;
            if (*scr > total) *scr = total;
            return;
        }
    }
    set_cmd_msg(db, "Pattern not found", 20);
}

/* ---- select-mode cursor helpers ---- */

static int last_visible_line(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];
    int total = panel_ring(p)->count;
    int scr = *panel_scroll(p);
    int ch = panel_content_h(db);
    int view_top = (scr == 0) ? total - ch : total - ch - scr;
    if (view_top < 0) view_top = 0;
    int last = view_top + ch - 1;
    if (last >= total) last = total - 1;
    return last >= 0 ? last : 0;
}

static void select_cursor_up(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];
    int *cur = panel_cursor(p);
    if (*cur > 0) (*cur)--;
    ensure_cursor_visible(db);
}

static void select_cursor_down(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];
    int *cur = panel_cursor(p);
    if (*cur < panel_ring(p)->count - 1) (*cur)++;
    ensure_cursor_visible(db);
}

static void select_cursor_pgup(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];
    int *cur = panel_cursor(p);
    *cur -= panel_content_h(db);
    if (*cur < 0) *cur = 0;
    ensure_cursor_visible(db);
}

static void select_cursor_pgdn(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];
    int *cur = panel_cursor(p);
    int total = panel_ring(p)->count;
    *cur += panel_content_h(db);
    if (*cur >= total) *cur = total - 1;
    if (*cur < 0) *cur = 0;
    ensure_cursor_visible(db);
}

/* ---- normal mode ---- */

static void handle_key_normal(ph_dashboard_t *db, int ch) {
    db_panel_t *fp = &db->panels[db->focused];
    int *cur = panel_cursor(fp);
    int *scr = panel_scroll(fp);
    int *sel = panel_sel_anchor(fp);

    /* select mode: arrows move cursor instead of scrolling */
    if (*sel >= 0) {
        switch (ch) {
        case KEY_UP:    case 'k':    select_cursor_up(db);   return;
        case KEY_DOWN:  case 'j':    select_cursor_down(db); return;
        case KEY_PPAGE:              select_cursor_pgup(db); return;
        case KEY_NPAGE:              select_cursor_pgdn(db); return;
        case KEY_HOME:
            *cur = 0;
            ensure_cursor_visible(db);
            return;
        default:
            break;  /* fall through to main switch */
        }
    }

    switch (ch) {
    case ':':
        db->mode = DB_MODE_COMMAND;
        db->cmd_len = 0;
        db->cmd_buf[0] = '\0';
        break;

    case 'q':
    case 'Q':
        db->quit = true;
        break;

    case '\t':
        db->focused = (db->focused + 1) % db->panel_count;
        if (db->zoomed) layout_panels(db);
        break;

    case KEY_UP:
    case 'k': {
        if (*cur < 0)
            *cur = last_visible_line(db);
        if (*cur > 0) (*cur)--;
        ensure_cursor_visible(db);
        break;
    }

    case KEY_DOWN:
    case 'j': {
        if (*cur < 0)
            *cur = last_visible_line(db);
        if (*cur < panel_ring(fp)->count - 1) (*cur)++;
        ensure_cursor_visible(db);
        break;
    }

    case KEY_PPAGE: {
        if (*cur < 0)
            *cur = last_visible_line(db);
        *cur -= panel_content_h(db);
        if (*cur < 0) *cur = 0;
        ensure_cursor_visible(db);
        break;
    }

    case KEY_NPAGE: {
        int total = panel_ring(fp)->count;
        if (*cur < 0)
            *cur = last_visible_line(db);
        *cur += panel_content_h(db);
        if (*cur >= total) *cur = total - 1;
        if (*cur < 0) *cur = 0;
        ensure_cursor_visible(db);
        break;
    }

    case KEY_HOME:
        *cur = 0;
        ensure_cursor_visible(db);
        break;

    case KEY_END:
        *scr = 0;
        *cur = -1;
        *sel = -1;
        break;

    case 'v': { /* enter/exit visual select mode */
        if (*sel >= 0) {
            *sel = -1;
        } else {
            if (*cur < 0)
                *cur = last_visible_line(db);
            *sel = *cur;
        }
        break;
    }

    case KEY_SR: /* Shift+Up */
        if (*cur < 0)
            *cur = last_visible_line(db);
        if (*sel < 0) *sel = *cur;
        select_cursor_up(db);
        break;

    case KEY_SF: /* Shift+Down */
        if (*cur < 0)
            *cur = last_visible_line(db);
        if (*sel < 0) *sel = *cur;
        select_cursor_down(db);
        break;

    case 'V':
        action_export_selection(db);
        break;

    case 'g':
        if (!fuzzy_scan_json_files(db)) {
            set_cmd_msg(db, "no .json files in cwd", 30);
            break;
        }
        db->fuzzy_saved_scroll = *scr;
        db->fuzzy_saved_cursor = *cur;
        db->fuzzy_picking = true;
        db->mode = DB_MODE_FUZZY;
        db->fuzzy_len = 0;
        db->fuzzy_buf[0] = '\0';
        fuzzy_recompute(db);
        break;

    case 'z':
        action_toggle_json_fold(db);
        break;

    case '1': case '2': case '3': case '4': {
        int tab_idx = ch - '1';
        if (fp->tab_count > 0 && tab_idx < fp->tab_count)
            fp->active_tab = tab_idx;
        break;
    }

    case 'f':
        db->zoomed = !db->zoomed;
        layout_panels(db);
        break;

    case 'c':
        action_clear(db);
        break;

    case '/':
        db->mode = DB_MODE_SEARCH;
        db->search_len = 0;
        db->search_buf[0] = '\0';
        break;

    case 'n':
        search_jump_next(db);
        break;

    case 'N':
        search_jump_prev(db);
        break;

    case '?':
        open_popup(db, DB_POPUP_HELP);
        break;

    case 'a':
        open_popup(db, DB_POPUP_ABOUT);
        break;

    case 0x13: /* Ctrl-S */
        db->btn_selected = DB_BTN_START;
        break;

    case 0x14: /* Ctrl-T */
        db->btn_selected = DB_BTN_STOP;
        break;

    case '\n':
    case '\r':
    case KEY_ENTER:
        if (db->btn_selected != DB_BTN_NONE)
            activate_button(db);
        break;

    case 27: /* Esc */
        db->btn_selected = DB_BTN_NONE;
        db->search_active = false;
        db->search_pat[0] = '\0';
        *cur = -1;
        *sel = -1;
        break;
    }
}

/* ---- mode dispatch ---- */

void handle_key(ph_dashboard_t *db, int ch) {
    /* global shell keybindings (work in any mode except popup) */
    if (db->mode != DB_MODE_POPUP) {
        if (ch == 0x10) { /* Ctrl+P: new shell view */
            shell_new_view(db);
            return;
        }
        if (ch == 0x07 && db->shell_open) { /* Ctrl+G: focus shell */
            shell_focus(db);
            return;
        }
        if (ch == 0x02 && db->shell_open) {
            shell_next_view(db);    /* Ctrl+B: next view */
            return;
        }
        if (ch == 0x12 && db->shell_open) {
            shell_prev_view(db);    /* Ctrl+R: prev view */
            return;
        }
        if (ch == 0x11) { /* Ctrl+Q: close shell */
            shell_close_all(db);
            return;
        }
    }

    switch (db->mode) {
    case DB_MODE_POPUP:   handle_key_popup(db, ch);   break;
    case DB_MODE_COMMAND: handle_key_command(db, ch);  break;
    case DB_MODE_SEARCH:  handle_key_search(db, ch);   break;
    case DB_MODE_FUZZY:   handle_key_fuzzy(db, ch);    break;
    case DB_MODE_SHELL:   handle_key_shell(db, ch);    break;
    case DB_MODE_NORMAL:  handle_key_normal(db, ch);   break;
    }
}

#endif /* PHOSPHOR_HAS_NCURSES */

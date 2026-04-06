#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/alloc.h"

#include <ctype.h>
#include <curses.h>
#include <stdio.h>
#include <string.h>

#ifdef PHOSPHOR_HAS_CJSON
#include "phosphor/json.h"
#include <cJSON.h>
#endif

/* ---- find JSON in a line ---- */

static bool find_json_span(const char *text, int len,
                            int *out_start, int *out_end) {
    for (int i = 0; i < len; i++) {
        if (text[i] != '{' && text[i] != '[') continue;

        int depth = 0;
        bool in_str = false;
        bool esc = false;

        for (int j = i; j < len; j++) {
            if (esc) { esc = false; continue; }
            if (text[j] == '\\' && in_str) { esc = true; continue; }
            if (text[j] == '"') { in_str = !in_str; continue; }
            if (in_str) continue;

            if (text[j] == '{' || text[j] == '[') depth++;
            else if (text[j] == '}' || text[j] == ']') {
                depth--;
                if (depth == 0) {
                    *out_start = i;
                    *out_end = j + 1;
                    return true;
                }
            }
        }
        return false; /* unmatched open bracket */
    }
    return false;
}

/* ---- count lines in text ---- */

static int count_lines(const char *text) {
    int n = 1;
    for (const char *p = text; *p; p++)
        if (*p == '\n') n++;
    return n;
}

/* ---- toggle fold on cursor line ---- */

void action_toggle_json_fold(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];
    int cur = *panel_cursor(p);
    int *fidx = panel_json_fold_idx(p);
    char **ftxt = panel_json_fold_text(p);
    int *flines = panel_json_fold_lines(p);

    if (cur < 0) {
        set_cmd_msg(db, "no cursor line", 20);
        return;
    }

    /* collapse if already expanded on this line */
    if (*fidx == cur) {
        json_fold_cleanup(p);
        return;
    }

    /* collapse any previous fold */
    json_fold_cleanup(p);

#ifndef PHOSPHOR_HAS_CJSON
    set_cmd_msg(db, "fold: cJSON not available", 30);
#else
    db_line_t *line = ringbuf_get(panel_ring(p), cur);
    if (!line || !line->text) {
        set_cmd_msg(db, "empty line", 20);
        return;
    }

    /* strip ANSI for clean JSON extraction */
    char clean[MAX_LINE_LEN];
    int clen = strip_ansi(clean, line->text, line->len);
    clean[clen] = '\0';

    int js, je;
    if (!find_json_span(clean, clen, &js, &je)) {
        set_cmd_msg(db, "no JSON on this line", 20);
        return;
    }

    /* extract JSON substring */
    char json_str[MAX_LINE_LEN];
    int jlen = je - js;
    if (jlen >= (int)sizeof(json_str)) jlen = (int)sizeof(json_str) - 1;
    memcpy(json_str, clean + js, (size_t)jlen);
    json_str[jlen] = '\0';

    /* parse and pretty-print */
    ph_json_t *root = ph_json_parse(json_str);
    if (!root) {
        set_cmd_msg(db, "invalid JSON", 20);
        return;
    }

    char *pretty = ph_json_print(root);
    ph_json_destroy(root);

    if (!pretty) {
        set_cmd_msg(db, "fold: format failed", 20);
        return;
    }

    *ftxt = pretty;
    *fidx = cur;
    *flines = count_lines(pretty);
    if (*flines > 200) *flines = 200;

    char msg[64];
    snprintf(msg, sizeof(msg), "JSON expanded: %d lines", *flines);
    set_cmd_msg(db, msg, 20);
#endif /* PHOSPHOR_HAS_CJSON */
}

/* ---- cleanup fold state ---- */

void json_fold_cleanup(db_panel_t *p) {
    char **ftxt = panel_json_fold_text(p);
    if (*ftxt) {
#ifdef PHOSPHOR_HAS_CJSON
        ph_json_free_string(*ftxt);
#endif
        *ftxt = NULL;
    }
    *panel_json_fold_idx(p) = -1;
    *panel_json_fold_lines(p) = 0;
}

/* ---- JSON syntax-highlighted line rendering ---- */

static void render_json_line(WINDOW *win, int y, int x,
                              const char *text, int len, int max_cols) {
    wmove(win, y, x);
    int col = 0;
    bool in_string = false;
    bool is_key = false;

    /* pre-scan: find colon to distinguish keys from values */
    const char *colon_pos = NULL;
    {
        bool qs = false;
        for (int i = 0; i < len; i++) {
            if (text[i] == '"') qs = !qs;
            if (!qs && text[i] == ':') { colon_pos = &text[i]; break; }
        }
    }

    for (int i = 0; i < len && col < max_cols; i++) {
        char c = text[i];

        /* inside a string: render until closing quote */
        if (in_string) {
            waddch(win, (chtype)c);
            col++;
            if (c == '"' && i > 0 && text[i-1] != '\\') {
                wattrset(win, A_NORMAL);
                in_string = false;
            }
            continue;
        }

        /* opening quote: determine key vs value */
        if (c == '"') {
            in_string = true;
            is_key = (colon_pos && &text[i] < colon_pos);
            wattron(win, COLOR_PAIR(is_key ? CP_JSON_KEY : CP_JSON_STRING));
            waddch(win, (chtype)c);
            col++;
            continue;
        }

        if (c == '{' || c == '}' || c == '[' || c == ']') {
            wattron(win, A_BOLD);
            waddch(win, (chtype)c);
            wattroff(win, A_BOLD);
            col++;
        } else if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
            wattron(win, COLOR_PAIR(CP_JSON_NUMBER));
            waddch(win, (chtype)c);
            wattroff(win, COLOR_PAIR(CP_JSON_NUMBER));
            col++;
        } else if (i + 4 <= len && strncmp(&text[i], "true", 4) == 0
                   && (i + 4 >= len || !isalpha((unsigned char)text[i+4]))) {
            wattron(win, COLOR_PAIR(CP_JSON_BOOL));
            waddnstr(win, "true", 4);
            wattroff(win, COLOR_PAIR(CP_JSON_BOOL));
            i += 3; col += 4;
        } else if (i + 5 <= len && strncmp(&text[i], "false", 5) == 0
                   && (i + 5 >= len || !isalpha((unsigned char)text[i+5]))) {
            wattron(win, COLOR_PAIR(CP_JSON_BOOL));
            waddnstr(win, "false", 5);
            wattroff(win, COLOR_PAIR(CP_JSON_BOOL));
            i += 4; col += 5;
        } else if (i + 4 <= len && strncmp(&text[i], "null", 4) == 0
                   && (i + 4 >= len || !isalpha((unsigned char)text[i+4]))) {
            wattron(win, COLOR_PAIR(CP_JSON_BOOL));
            waddnstr(win, "null", 4);
            wattroff(win, COLOR_PAIR(CP_JSON_BOOL));
            i += 3; col += 4;
        } else {
            waddch(win, (chtype)c);
            col++;
        }
    }
    wattrset(win, A_NORMAL);
}

/* ---- render expanded fold (multiple rows) ---- */

int render_json_fold(WINDOW *win, int start_row, int x,
                     const char *json_text, int max_w, int max_rows) {
    int row = 0;
    const char *line = json_text;

    while (*line && row < max_rows) {
        const char *nl = strchr(line, '\n');
        int llen = nl ? (int)(nl - line) : (int)strlen(line);

        render_json_line(win, start_row + row, x, line, llen, max_w);
        row++;

        if (!nl) break;
        line = nl + 1;
    }

    return row;
}

/* ==================================================================
 *  JSON VIEWER -- popup tree browser with fold/unfold
 * ================================================================== */

#define JV_MAX_NODES 10000

/* ---- node builder (cJSON -> flat node array) ---- */

#ifdef PHOSPHOR_HAS_CJSON

typedef struct {
    db_json_node_t *buf;
    int             cap;
    int             cnt;
} jv_builder_t;

static char *jv_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = ph_alloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

static void jv_emit(jv_builder_t *b, db_jn_type_t type, int depth,
                     const char *key, const char *value,
                     int child_count, bool is_last) {
    if (b->cnt >= b->cap) return;
    db_json_node_t *n = &b->buf[b->cnt++];
    n->type = type;
    n->depth = depth;
    n->key = jv_strdup(key);
    n->value = jv_strdup(value);
    n->child_count = child_count;
    n->subtree_end = 0;
    n->folded = (type == JN_OBJECT || type == JN_ARRAY);
    n->is_last = is_last;
}

static void jv_build(jv_builder_t *b, cJSON *item, int depth, bool is_last) {
    if (!item || b->cnt >= b->cap) return;

    if (cJSON_IsObject(item)) {
        int cc = cJSON_GetArraySize(item);
        int idx = b->cnt;
        jv_emit(b, JN_OBJECT, depth, item->string, NULL, cc, is_last);
        cJSON *ch = item->child;
        while (ch) {
            jv_build(b, ch, depth + 1, ch->next == NULL);
            ch = ch->next;
        }
        jv_emit(b, JN_CLOSE, depth, NULL, "}", 0, is_last);
        if (idx < b->cnt) b->buf[idx].subtree_end = b->cnt;

    } else if (cJSON_IsArray(item)) {
        int cc = cJSON_GetArraySize(item);
        int idx = b->cnt;
        jv_emit(b, JN_ARRAY, depth, item->string, NULL, cc, is_last);
        cJSON *ch = item->child;
        while (ch) {
            jv_build(b, ch, depth + 1, ch->next == NULL);
            ch = ch->next;
        }
        jv_emit(b, JN_CLOSE, depth, NULL, "]", 0, is_last);
        if (idx < b->cnt) b->buf[idx].subtree_end = b->cnt;

    } else if (cJSON_IsString(item)) {
        jv_emit(b, JN_STRING, depth, item->string,
                item->valuestring, 0, is_last);
    } else if (cJSON_IsNumber(item)) {
        char buf[64];
        if (item->valuedouble == (double)item->valueint)
            snprintf(buf, sizeof(buf), "%d", item->valueint);
        else
            snprintf(buf, sizeof(buf), "%g", item->valuedouble);
        jv_emit(b, JN_NUMBER, depth, item->string, buf, 0, is_last);
    } else if (cJSON_IsBool(item)) {
        jv_emit(b, JN_BOOL, depth, item->string,
                cJSON_IsTrue(item) ? "true" : "false", 0, is_last);
    } else {
        jv_emit(b, JN_NULL, depth, item->string, "null", 0, is_last);
    }
}

#endif /* PHOSPHOR_HAS_CJSON */

/* ---- visible node helpers ---- */

static int jv_count_visible(db_json_node_t *nodes, int count) {
    int nvis = 0;
    int i = 0;
    while (i < count) {
        nvis++;
        if ((nodes[i].type == JN_OBJECT || nodes[i].type == JN_ARRAY)
            && nodes[i].folded)
            i = nodes[i].subtree_end;
        else
            i++;
    }
    return nvis;
}

static int jv_vis_to_raw(db_json_node_t *nodes, int count, int vis_idx) {
    int vi = 0;
    int i = 0;
    while (i < count) {
        if (vi == vis_idx) return i;
        vi++;
        if ((nodes[i].type == JN_OBJECT || nodes[i].type == JN_ARRAY)
            && nodes[i].folded)
            i = nodes[i].subtree_end;
        else
            i++;
    }
    return -1;
}

/* ---- free viewer nodes ---- */

static void jv_free_nodes(ph_dashboard_t *db) {
    if (!db->jv_nodes) return;
    for (int i = 0; i < db->jv_node_count; i++) {
        if (db->jv_nodes[i].key) ph_free(db->jv_nodes[i].key);
        if (db->jv_nodes[i].value) ph_free(db->jv_nodes[i].value);
    }
    ph_free(db->jv_nodes);
    db->jv_nodes = NULL;
    db->jv_node_count = 0;
    db->jv_cursor = 0;
    db->jv_scroll = 0;
}

/* ---- open viewer ---- */

void open_json_viewer(ph_dashboard_t *db, const char *path) {
#ifndef PHOSPHOR_HAS_CJSON
    (void)path;
    set_cmd_msg(db, "viewer: cJSON not available", 30);
#else
    ph_json_t *root = ph_json_parse_file(path);
    if (!root) {
        set_cmd_msg(db, "failed to parse JSON", 30);
        return;
    }

    jv_builder_t bld;
    bld.cap = JV_MAX_NODES;
    bld.cnt = 0;
    bld.buf = ph_alloc((size_t)bld.cap * sizeof(db_json_node_t));
    if (!bld.buf) {
        ph_json_destroy(root);
        set_cmd_msg(db, "out of memory", 30);
        return;
    }
    memset(bld.buf, 0, (size_t)bld.cap * sizeof(db_json_node_t));

    jv_build(&bld, (cJSON *)root, 0, true);
    ph_json_destroy(root);

    db->jv_nodes = bld.buf;
    db->jv_node_count = bld.cnt;
    db->jv_cursor = 0;
    db->jv_scroll = 0;
    snprintf(db->jv_title, sizeof(db->jv_title), "%s", path);

    db->popup = DB_POPUP_JSON_VIEWER;
    db->mode = DB_MODE_POPUP;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int h = rows - 2; if (h < 10) h = 10;
    int w = cols - 4;  if (w < 40) w = 40;
    if (db->popup_win) delwin(db->popup_win);
    db->popup_win = newwin(h, w, (rows - h) / 2, (cols - w) / 2);
#endif
}

/* ---- close viewer (back to fuzzy file picker) ---- */

void close_json_viewer(ph_dashboard_t *db) {
    jv_free_nodes(db);

    if (db->popup_win) {
        delwin(db->popup_win);
        db->popup_win = NULL;
    }
    db->popup = DB_POPUP_NONE;

    db->mode = DB_MODE_FUZZY;
    db->fuzzy_len = 0;
    db->fuzzy_buf[0] = '\0';
    fuzzy_recompute(db);
    touchwin(stdscr);
}

/* ---- render single node line ---- */

static void render_jv_node(WINDOW *win, int y, int x,
                            db_json_node_t *node, int max_w) {
    wmove(win, y, x);
    int indent = node->depth * 2;
    for (int i = 0; i < indent && i < max_w; i++)
        waddch(win, ' ');

    bool is_cont = (node->type == JN_OBJECT || node->type == JN_ARRAY);

    /* close marker */
    if (node->type == JN_CLOSE) {
        wattron(win, A_BOLD);
        if (node->value) waddnstr(win, node->value, 1);
        wattroff(win, A_BOLD);
        if (!node->is_last) waddch(win, ',');
        wattrset(win, A_NORMAL);
        return;
    }

    /* fold indicator for containers */
    if (is_cont) {
        wattron(win, COLOR_PAIR(CP_FUZZY_PROMPT));
        waddnstr(win, node->folded ? "[+] " : "[-] ", 4);
        wattroff(win, COLOR_PAIR(CP_FUZZY_PROMPT));
    }

    /* key */
    if (node->key) {
        wattron(win, COLOR_PAIR(CP_JSON_KEY));
        wprintw(win, "\"%s\"", node->key);
        wattroff(win, COLOR_PAIR(CP_JSON_KEY));
        waddnstr(win, ": ", 2);
    }

    /* value */
    if (is_cont && node->folded) {
        char br = (node->type == JN_OBJECT) ? '{' : '[';
        char cl = (node->type == JN_OBJECT) ? '}' : ']';
        wattron(win, A_BOLD);
        waddch(win, (chtype)br);
        wattroff(win, A_BOLD);
        wattron(win, A_DIM);
        waddnstr(win, " ... ", 5);
        wattroff(win, A_DIM);
        wattron(win, A_BOLD);
        waddch(win, (chtype)cl);
        wattroff(win, A_BOLD);
        char cbuf[32];
        snprintf(cbuf, sizeof(cbuf), "  (%d)", node->child_count);
        wattron(win, A_DIM);
        waddnstr(win, cbuf, (int)strlen(cbuf));
        wattroff(win, A_DIM);
        if (!node->is_last) waddch(win, ',');
    } else if (is_cont) {
        wattron(win, A_BOLD);
        waddch(win, (chtype)(node->type == JN_OBJECT ? '{' : '['));
        wattroff(win, A_BOLD);
    } else {
        switch (node->type) {
        case JN_STRING:
            wattron(win, COLOR_PAIR(CP_JSON_STRING));
            wprintw(win, "\"%s\"", node->value ? node->value : "");
            wattroff(win, COLOR_PAIR(CP_JSON_STRING));
            break;
        case JN_NUMBER:
            wattron(win, COLOR_PAIR(CP_JSON_NUMBER));
            if (node->value)
                waddnstr(win, node->value, (int)strlen(node->value));
            wattroff(win, COLOR_PAIR(CP_JSON_NUMBER));
            break;
        case JN_BOOL:
        case JN_NULL:
            wattron(win, COLOR_PAIR(CP_JSON_BOOL));
            if (node->value)
                waddnstr(win, node->value, (int)strlen(node->value));
            wattroff(win, COLOR_PAIR(CP_JSON_BOOL));
            break;
        default:
            break;
        }
        if (!node->is_last) waddch(win, ',');
    }

    wattrset(win, A_NORMAL);
}

/* ---- draw viewer popup ---- */

void draw_json_viewer(ph_dashboard_t *db) {
    if (!db->jv_nodes) return;

    /* recreate window for current terminal size */
    int srows, scols;
    getmaxyx(stdscr, srows, scols);
    int h = srows - 2; if (h < 10) h = 10;
    int w = scols - 4;  if (w < 40) w = 40;
    if (db->popup_win) delwin(db->popup_win);
    db->popup_win = newwin(h, w, (srows - h) / 2, (scols - w) / 2);

    WINDOW *win = db->popup_win;
    if (!win) return;
    werase(win);

    /* border */
    wattron(win, COLOR_PAIR(CP_POPUP_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(CP_POPUP_BORDER));

    /* title */
    wattron(win, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);
    mvwprintw(win, 0, 2, " %s ", db->jv_title);
    wattroff(win, COLOR_PAIR(CP_POPUP_KEY) | A_BOLD);

    int content_h = h - 2;
    int content_w = w - 2;
    if (content_h <= 0 || content_w <= 0) {
        wnoutrefresh(win);
        return;
    }

    int nvis = jv_count_visible(db->jv_nodes, db->jv_node_count);

    /* clamp cursor */
    if (db->jv_cursor >= nvis) db->jv_cursor = nvis - 1;
    if (db->jv_cursor < 0) db->jv_cursor = 0;

    /* adjust scroll */
    if (db->jv_cursor < db->jv_scroll)
        db->jv_scroll = db->jv_cursor;
    if (db->jv_cursor >= db->jv_scroll + content_h)
        db->jv_scroll = db->jv_cursor - content_h + 1;
    if (db->jv_scroll < 0) db->jv_scroll = 0;

    /* iterate visible nodes */
    int vi = 0;   /* visible index */
    int ni = 0;   /* raw node index */
    while (ni < db->jv_node_count) {
        if (vi >= db->jv_scroll && vi < db->jv_scroll + content_h) {
            int r = vi - db->jv_scroll;
            render_jv_node(win, r + 1, 1,
                           &db->jv_nodes[ni], content_w);
            if (vi == db->jv_cursor)
                mvwchgat(win, r + 1, 1, content_w,
                         A_REVERSE, CP_CURSOR_LINE, NULL);
        }
        vi++;
        if ((db->jv_nodes[ni].type == JN_OBJECT
             || db->jv_nodes[ni].type == JN_ARRAY)
            && db->jv_nodes[ni].folded)
            ni = db->jv_nodes[ni].subtree_end;
        else
            ni++;

        if (vi >= db->jv_scroll + content_h) break;
    }

    /* footer */
    char fbuf[64];
    snprintf(fbuf, sizeof(fbuf), " %d/%d ", db->jv_cursor + 1, nvis);
    int fx = w - (int)strlen(fbuf) - 1;
    if (fx > 2) {
        wattron(win, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
        mvwprintw(win, h - 1, fx, "%s", fbuf);
        wattroff(win, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
    }

    wattron(win, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);
    mvwprintw(win, h - 1, 2, "z:fold  j/k:navigate  Esc:back  q:close");
    wattroff(win, COLOR_PAIR(CP_POPUP_KEY) | A_DIM);

    wnoutrefresh(win);
}

/* ---- key handler ---- */

void handle_json_viewer_key(ph_dashboard_t *db, int ch) {
    int nvis = jv_count_visible(db->jv_nodes, db->jv_node_count);
    if (nvis == 0) {
        if (ch == 27 || ch == 'q') close_json_viewer(db);
        return;
    }

    switch (ch) {
    case 27: /* Esc -- back to fuzzy file picker */
        close_json_viewer(db);
        break;

    case 'q': { /* close everything, back to normal */
        jv_free_nodes(db);
        if (db->popup_win) { delwin(db->popup_win); db->popup_win = NULL; }
        db->popup = DB_POPUP_NONE;
        db->mode = DB_MODE_NORMAL;
        db->panels[db->focused].scroll = db->fuzzy_saved_scroll;
        db->panels[db->focused].cursor = db->fuzzy_saved_cursor;
        db->fuzzy_picking = false;
        fuzzy_unload_disk(db);
        touchwin(stdscr);
        break;
    }

    case 'z':
    case '\n':
    case '\r':
    case KEY_ENTER: {
        int raw = jv_vis_to_raw(db->jv_nodes, db->jv_node_count,
                                 db->jv_cursor);
        if (raw >= 0) {
            db_json_node_t *n = &db->jv_nodes[raw];
            if (n->type == JN_OBJECT || n->type == JN_ARRAY)
                n->folded = !n->folded;
        }
        break;
    }

    case KEY_RIGHT:
    case 'l': {
        int raw = jv_vis_to_raw(db->jv_nodes, db->jv_node_count,
                                 db->jv_cursor);
        if (raw >= 0) {
            db_json_node_t *n = &db->jv_nodes[raw];
            if ((n->type == JN_OBJECT || n->type == JN_ARRAY) && n->folded)
                n->folded = false;
        }
        break;
    }

    case KEY_LEFT:
    case 'h': {
        int raw = jv_vis_to_raw(db->jv_nodes, db->jv_node_count,
                                 db->jv_cursor);
        if (raw >= 0) {
            db_json_node_t *n = &db->jv_nodes[raw];
            if ((n->type == JN_OBJECT || n->type == JN_ARRAY) && !n->folded)
                n->folded = true;
        }
        break;
    }

    case KEY_UP:
    case 'k':
        if (db->jv_cursor > 0) db->jv_cursor--;
        break;

    case KEY_DOWN:
    case 'j':
        if (db->jv_cursor < nvis - 1) db->jv_cursor++;
        break;

    case KEY_PPAGE: {
        int h = 10;
        if (db->popup_win) {
            int r, c;
            getmaxyx(db->popup_win, r, c);
            (void)c;
            h = r - 2;
        }
        db->jv_cursor -= h;
        if (db->jv_cursor < 0) db->jv_cursor = 0;
        break;
    }

    case KEY_NPAGE: {
        int h = 10;
        if (db->popup_win) {
            int r, c;
            getmaxyx(db->popup_win, r, c);
            (void)c;
            h = r - 2;
        }
        db->jv_cursor += h;
        if (db->jv_cursor >= nvis) db->jv_cursor = nvis - 1;
        break;
    }

    case KEY_HOME:
        db->jv_cursor = 0;
        db->jv_scroll = 0;
        break;

    case KEY_END:
        db->jv_cursor = nvis - 1;
        break;

    default:
        break;
    }
}

#endif /* PHOSPHOR_HAS_NCURSES */

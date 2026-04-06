#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/alloc.h"

#ifdef PHOSPHOR_HAS_CJSON
#include "phosphor/json.h"
#include <cJSON.h>
#endif

#include <ctype.h>
#include <curses.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* ---- fuzzy scoring ---- */

int fuzzy_score(const char *text, const char *pattern, int patlen) {
    if (patlen == 0) return 0;

    int score = 0;
    int consecutive = 0;
    const char *t = text;

    for (int i = 0; i < patlen; i++) {
        char pc = (char)tolower((unsigned char)pattern[i]);
        bool found = false;
        while (*t) {
            if ((char)tolower((unsigned char)*t) == pc) {
                score++;
                if (consecutive > 0) score += consecutive * 2;
                consecutive++;
                if (t == text || t[-1] == ' ' || t[-1] == '/'
                    || t[-1] == ':' || t[-1] == '.')
                    score += 5;
                t++;
                found = true;
                break;
            }
            consecutive = 0;
            t++;
        }
        if (!found) return -1;
    }
    return score;
}

/* ---- recursive scan for *.json files ---- */

/* capacity management for the file list */
#define FUZZY_INITIAL_CAP 64

static int fuzzy_cap;

static bool fuzzy_grow(ph_dashboard_t *db) {
    int new_cap = fuzzy_cap * 2;
    char **grown = ph_realloc(db->fuzzy_files,
                              (size_t)new_cap * sizeof(char *));
    if (!grown) return false;
    db->fuzzy_files = grown;
    fuzzy_cap = new_cap;
    return true;
}

static void fuzzy_add_file(ph_dashboard_t *db, const char *path) {
    if (db->fuzzy_file_count >= fuzzy_cap && !fuzzy_grow(db))
        return;
    size_t len = strlen(path);
    char *copy = ph_alloc(len + 1);
    if (!copy) return;
    memcpy(copy, path, len + 1);
    db->fuzzy_files[db->fuzzy_file_count++] = copy;
}

static bool fuzzy_is_excluded(ph_dashboard_t *db, const char *path) {
    /* never exclude the log directory tree */
    if (db->serve_cfg && db->serve_cfg->ns.log_directory) {
        const char *ld = db->serve_cfg->ns.log_directory;
        size_t ldlen = strlen(ld);
        if (strncmp(path, ld, ldlen) == 0
            && (path[ldlen] == '/' || path[ldlen] == '\0'))
            return false;
    }

    for (int i = 0; i < db->fuzzy_exclude_count; i++) {
        const char *pat = db->fuzzy_excludes[i];
        if (!pat) continue;
        /* match: exact dir name, or path starts with pattern/ */
        if (strcmp(path, pat) == 0) return true;
        size_t plen = strlen(pat);
        if (strncmp(path, pat, plen) == 0 && path[plen] == '/')
            return true;
        /* also match basename against pattern (e.g. "node_modules") */
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        if (strcmp(base, pat) == 0) return true;
    }
    return false;
}

/* built-in dirs that are never useful for the fuzzy finder */
static const char *builtin_excludes[] = {
    "node_modules", "build", "public", ".venv", "__pycache__",
    "subprojects", "certs", ".cache", ".git",
};
#define BUILTIN_EXCLUDE_COUNT \
    ((int)(sizeof(builtin_excludes) / sizeof(builtin_excludes[0])))

static bool fuzzy_builtin_excluded(const char *name) {
    for (int i = 0; i < BUILTIN_EXCLUDE_COUNT; i++) {
        if (strcmp(name, builtin_excludes[i]) == 0) return true;
    }
    return false;
}

static void fuzzy_scan_dir(ph_dashboard_t *db, const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        if (name[0] == '.') continue;  /* skip hidden + . + .. */

        char fullpath[512];
        if (dirpath[0] == '.' && dirpath[1] == '\0')
            snprintf(fullpath, sizeof(fullpath), "%s", name);
        else
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, name);

        /* never exclude the log directory tree */
        bool is_log_path = false;
        if (db->serve_cfg && db->serve_cfg->ns.log_directory) {
            const char *ld = db->serve_cfg->ns.log_directory;
            size_t ldlen = strlen(ld);
            if (strncmp(fullpath, ld, ldlen) == 0
                && (fullpath[ldlen] == '/' || fullpath[ldlen] == '\0'))
                is_log_path = true;
        }

        /* check excludes (built-in + user-configured) -- skip for log paths */
        if (!is_log_path && fuzzy_builtin_excluded(name)) continue;
        if (!is_log_path && fuzzy_is_excluded(db, fullpath)) continue;

        /* check if it's a directory -- recurse */
        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            fuzzy_scan_dir(db, fullpath);
            continue;
        }

        /* check .json extension */
        size_t nlen = strlen(name);
        if (nlen > 5 && strcmp(name + nlen - 5, ".json") == 0)
            fuzzy_add_file(db, fullpath);
    }

    closedir(dir);
}

bool fuzzy_scan_json_files(ph_dashboard_t *db) {
    /* free any previous file list */
    if (db->fuzzy_files) {
        for (int i = 0; i < db->fuzzy_file_count; i++)
            ph_free(db->fuzzy_files[i]);
        ph_free(db->fuzzy_files);
        db->fuzzy_files = NULL;
        db->fuzzy_file_count = 0;
    }

    fuzzy_cap = FUZZY_INITIAL_CAP;
    db->fuzzy_files = ph_alloc((size_t)fuzzy_cap * sizeof(char *));
    if (!db->fuzzy_files) return false;

    /* scan cwd + subdirectories (log/, log/debug/, shell/, etc.) */
    fuzzy_scan_dir(db, ".");

    return db->fuzzy_file_count > 0;
}

/* ---- load a specific JSON file ---- */

bool fuzzy_load_file(ph_dashboard_t *db, const char *path) {
#ifndef PHOSPHOR_HAS_CJSON
    (void)db; (void)path;
    return false;
#else
    /* free any previously loaded lines */
    if (db->fuzzy_disk_lines) {
        for (int i = 0; i < db->fuzzy_disk_count; i++)
            ph_free(db->fuzzy_disk_lines[i]);
        ph_free(db->fuzzy_disk_lines);
        db->fuzzy_disk_lines = NULL;
        db->fuzzy_disk_count = 0;
    }

    snprintf(db->fuzzy_fname, sizeof(db->fuzzy_fname), "%s", path);

    ph_json_t *root = ph_json_parse_file(path);
    if (!root) return false;

    /* first pass: count total lines */
    int total = 0;
    cJSON *date_obj = NULL;
    cJSON_ArrayForEach(date_obj, (cJSON *)root) {
        cJSON *line_obj = NULL;
        cJSON_ArrayForEach(line_obj, date_obj) {
            if (cJSON_IsString(line_obj)) total++;
        }
    }

    if (total == 0) {
        ph_json_destroy(root);
        return false;
    }

    db->fuzzy_disk_lines = ph_alloc((size_t)total * sizeof(char *));
    if (!db->fuzzy_disk_lines) {
        ph_json_destroy(root);
        return false;
    }
    db->fuzzy_disk_count = 0;

    /* second pass: load lines with date prefix */
    cJSON_ArrayForEach(date_obj, (cJSON *)root) {
        const char *date = date_obj->string ? date_obj->string : "?";
        cJSON *line_obj = NULL;
        cJSON_ArrayForEach(line_obj, date_obj) {
            if (!cJSON_IsString(line_obj) || !line_obj->valuestring)
                continue;
            const char *text = line_obj->valuestring;
            int len = snprintf(NULL, 0, "[%s] %s", date, text);
            char *buf = ph_alloc((size_t)len + 1);
            if (!buf) continue;
            snprintf(buf, (size_t)len + 1, "[%s] %s", date, text);
            db->fuzzy_disk_lines[db->fuzzy_disk_count++] = buf;
        }
    }

    ph_json_destroy(root);
    return db->fuzzy_disk_count > 0;
#endif /* PHOSPHOR_HAS_CJSON */
}

/* ---- free loaded data ---- */

void fuzzy_unload_disk(ph_dashboard_t *db) {
    if (db->fuzzy_disk_lines) {
        for (int i = 0; i < db->fuzzy_disk_count; i++)
            ph_free(db->fuzzy_disk_lines[i]);
        ph_free(db->fuzzy_disk_lines);
        db->fuzzy_disk_lines = NULL;
    }
    db->fuzzy_disk_count = 0;

    if (db->fuzzy_files) {
        for (int i = 0; i < db->fuzzy_file_count; i++)
            ph_free(db->fuzzy_files[i]);
        ph_free(db->fuzzy_files);
        db->fuzzy_files = NULL;
    }
    db->fuzzy_file_count = 0;
    db->fuzzy_fname[0] = '\0';
}

/* ---- recompute results ---- */

void fuzzy_recompute(ph_dashboard_t *db) {
    db->fuzzy_result_count = 0;
    db->fuzzy_selected = 0;

    if (db->fuzzy_picking) {
        /* phase 0: match against file names */
        for (int i = 0; i < db->fuzzy_file_count; i++) {
            const char *name = db->fuzzy_files[i];
            if (!name) continue;

            if (db->fuzzy_len == 0
                || fuzzy_score(name, db->fuzzy_buf, db->fuzzy_len) >= 0) {
                if (db->fuzzy_result_count < MAX_LINES)
                    db->fuzzy_results[db->fuzzy_result_count++] = i;
            }
        }
    } else {
        /* phase 1: match against loaded lines */
        for (int i = 0; i < db->fuzzy_disk_count; i++) {
            const char *line = db->fuzzy_disk_lines[i];
            if (!line) continue;

            if (db->fuzzy_len == 0
                || fuzzy_score(line, db->fuzzy_buf, db->fuzzy_len) >= 0) {
                if (db->fuzzy_result_count < MAX_LINES)
                    db->fuzzy_results[db->fuzzy_result_count++] = i;
            }
        }
    }
}

/* ---- compute match positions for highlight ---- */

static void fuzzy_match_positions(const char *text, int textlen,
                                   const char *pattern, int patlen,
                                   bool *highlight) {
    memset(highlight, 0, (size_t)textlen * sizeof(bool));
    if (patlen == 0) return;

    int ti = 0;
    for (int pi = 0; pi < patlen && ti < textlen; pi++) {
        char pc = (char)tolower((unsigned char)pattern[pi]);
        while (ti < textlen) {
            if ((char)tolower((unsigned char)text[ti]) == pc) {
                highlight[ti] = true;
                ti++;
                break;
            }
            ti++;
        }
    }
}

/* ---- render a fuzzy-highlighted line ---- */

static void render_fuzzy_line(WINDOW *win, int y, int x,
                               const char *text, int len,
                               int max_cols,
                               const char *pattern, int patlen,
                               bool dim_date_prefix) {
    bool highlight[MAX_LINE_LEN];
    int tlen = len < MAX_LINE_LEN ? len : MAX_LINE_LEN - 1;
    fuzzy_match_positions(text, tlen, pattern, patlen, highlight);

    wmove(win, y, x);
    int col = 0;

    for (int i = 0; i < tlen && col < max_cols;) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x20 && c != '\t') { i++; continue; }
        int slen = (c >= 0x80) ? utf8_seq_len(c) : 1;
        if (slen < 1) slen = 1;
        if (i + slen > tlen) break;

        /* dim date prefix [DDMMYYYY] */
        if (col == 0 && c == '[' && dim_date_prefix) {
            int end = 0;
            for (int k = 0; k < 16 && i + k < tlen; k++) {
                if (text[i + k] == ']') { end = k + 1; break; }
            }
            if (end > 0) {
                wattron(win, COLOR_PAIR(CP_INFO_DIM) | A_DIM);
                waddnstr(win, text + i, end);
                wattroff(win, COLOR_PAIR(CP_INFO_DIM) | A_DIM);
                i += end;
                col += end;
                continue;
            }
        }

        if (highlight[i]) {
            wattron(win, COLOR_PAIR(CP_FUZZY_MATCH) | A_BOLD | A_UNDERLINE);
            waddnstr(win, text + i, slen);
            wattroff(win, COLOR_PAIR(CP_FUZZY_MATCH) | A_BOLD | A_UNDERLINE);
        } else {
            waddnstr(win, text + i, slen);
        }

        i += slen;
        col++;
    }
}

/* ---- draw fuzzy popup (overlay) ---- */

void draw_fuzzy_popup(ph_dashboard_t *db) {
    int srows, scols;
    getmaxyx(stdscr, srows, scols);
    int h = srows - 4; if (h < 10) h = 10;
    int w = scols - 8;  if (w < 40) w = 40;
    if (db->popup_win) delwin(db->popup_win);
    db->popup_win = newwin(h, w, (srows - h) / 2, (scols - w) / 2);

    WINDOW *win = db->popup_win;
    if (!win) return;
    werase(win);

    /* border */
    wattron(win, COLOR_PAIR(CP_POPUP_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(CP_POPUP_BORDER));

    /* title: fuzzy prompt with cursor */
    wattron(win, COLOR_PAIR(CP_FUZZY_PROMPT) | A_BOLD);
    if (db->fuzzy_picking)
        mvwprintw(win, 0, 2, " open> %.*s", db->fuzzy_len, db->fuzzy_buf);
    else
        mvwprintw(win, 0, 2, " > %.*s", db->fuzzy_len, db->fuzzy_buf);
    wattroff(win, COLOR_PAIR(CP_FUZZY_PROMPT) | A_BOLD);

    {
        int prefix = db->fuzzy_picking ? 8 : 5;
        int cur_x = prefix + db->fuzzy_len;
        if (cur_x < w - 2) {
            wattron(win, A_REVERSE);
            mvwaddch(win, 0, cur_x, ' ');
            wattroff(win, A_REVERSE);
        }
    }

    /* info on right */
    char cbuf[128];
    int total = db->fuzzy_picking ? db->fuzzy_file_count
                                  : db->fuzzy_disk_count;
    if (db->fuzzy_picking) {
        snprintf(cbuf, sizeof(cbuf), " %d/%d .json files ",
                 db->fuzzy_result_count, total);
    } else {
        snprintf(cbuf, sizeof(cbuf), " %d/%d  %s ",
                 db->fuzzy_result_count, total, db->fuzzy_fname);
    }
    int cx = w - (int)strlen(cbuf) - 1;
    if (cx > 0) {
        wattron(win, COLOR_PAIR(CP_FUZZY_PROMPT) | A_DIM);
        mvwprintw(win, 0, cx, "%s", cbuf);
        wattroff(win, COLOR_PAIR(CP_FUZZY_PROMPT) | A_DIM);
    }

    int content_h = h - 2;
    int content_w = w - 2;
    if (content_h <= 0 || content_w <= 0) {
        wnoutrefresh(win);
        return;
    }

    /* visible window centered on selected result */
    int start = db->fuzzy_selected - content_h / 2;
    if (start < 0) start = 0;
    if (start + content_h > db->fuzzy_result_count)
        start = db->fuzzy_result_count - content_h;
    if (start < 0) start = 0;

    for (int r = 0; r < content_h; r++) {
        int ri = start + r;
        if (ri >= db->fuzzy_result_count) break;

        int di = db->fuzzy_results[ri];
        const char *text = NULL;

        if (db->fuzzy_picking) {
            if (di >= 0 && di < db->fuzzy_file_count)
                text = db->fuzzy_files[di];
        } else {
            if (di >= 0 && di < db->fuzzy_disk_count)
                text = db->fuzzy_disk_lines[di];
        }
        if (!text) continue;

        render_fuzzy_line(win, r + 1, 1, text, (int)strlen(text),
                          content_w, db->fuzzy_buf, db->fuzzy_len,
                          !db->fuzzy_picking);

        /* selected row highlight */
        if (ri == db->fuzzy_selected)
            mvwchgat(win, r + 1, 1, content_w,
                     A_REVERSE, CP_CURSOR_LINE, NULL);
    }

    /* footer hints */
    wattron(win, COLOR_PAIR(CP_FUZZY_PROMPT) | A_DIM);
    const char *fhint = db->fuzzy_picking
        ? "Enter:open  Esc:cancel  Ctrl-U:clear"
        : "Enter:select  Esc:back  Ctrl-U:clear";
    mvwprintw(win, h - 1, 2, "%s", fhint);
    wattroff(win, COLOR_PAIR(CP_FUZZY_PROMPT) | A_DIM);

    wnoutrefresh(win);
}

/* ---- key handler ---- */

static void fuzzy_exit(ph_dashboard_t *db) {
    db->mode = DB_MODE_NORMAL;
    *panel_scroll(&db->panels[db->focused]) = db->fuzzy_saved_scroll;
    *panel_cursor(&db->panels[db->focused]) = db->fuzzy_saved_cursor;
    db->fuzzy_len = 0;
    db->fuzzy_buf[0] = '\0';
    db->fuzzy_result_count = 0;
    db->fuzzy_picking = false;
    fuzzy_unload_disk(db);
    if (db->popup_win) {
        delwin(db->popup_win);
        db->popup_win = NULL;
    }
    touchwin(stdscr);
}

void handle_key_fuzzy(ph_dashboard_t *db, int ch) {
    if (ch == 27) { /* Esc */
        if (!db->fuzzy_picking && db->fuzzy_disk_lines) {
            /* back to file picker */
            for (int i = 0; i < db->fuzzy_disk_count; i++)
                ph_free(db->fuzzy_disk_lines[i]);
            ph_free(db->fuzzy_disk_lines);
            db->fuzzy_disk_lines = NULL;
            db->fuzzy_disk_count = 0;
            db->fuzzy_fname[0] = '\0';
            db->fuzzy_picking = true;
            db->fuzzy_len = 0;
            db->fuzzy_buf[0] = '\0';
            fuzzy_recompute(db);
        } else {
            fuzzy_exit(db);
        }
        return;
    }

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
        if (db->fuzzy_result_count == 0) return;

        int di = db->fuzzy_results[db->fuzzy_selected];

        if (db->fuzzy_picking) {
            /* pick file -> open in JSON viewer popup */
            if (di >= 0 && di < db->fuzzy_file_count
                && db->fuzzy_files[di]) {
                open_json_viewer(db, db->fuzzy_files[di]);
            }
        } else {
            /* legacy line search: show in command message */
            if (di >= 0 && di < db->fuzzy_disk_count
                && db->fuzzy_disk_lines[di]) {
                char msg[256];
                snprintf(msg, sizeof(msg), "%.250s",
                         db->fuzzy_disk_lines[di]);
                set_cmd_msg(db, msg, 60);
            }
            fuzzy_exit(db);
        }
        return;
    }

    if (ch == KEY_UP) {
        if (db->fuzzy_selected > 0) db->fuzzy_selected--;
        return;
    }

    if (ch == KEY_DOWN) {
        if (db->fuzzy_selected < db->fuzzy_result_count - 1)
            db->fuzzy_selected++;
        return;
    }

    if (ch == KEY_PPAGE) {
        db->fuzzy_selected -= 10;
        if (db->fuzzy_selected < 0) db->fuzzy_selected = 0;
        return;
    }

    if (ch == KEY_NPAGE) {
        db->fuzzy_selected += 10;
        if (db->fuzzy_selected >= db->fuzzy_result_count)
            db->fuzzy_selected = db->fuzzy_result_count - 1;
        if (db->fuzzy_selected < 0) db->fuzzy_selected = 0;
        return;
    }

    if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
        && db->fuzzy_len > 0) {
        db->fuzzy_buf[--db->fuzzy_len] = '\0';
        fuzzy_recompute(db);
        return;
    }

    if (ch == 0x15) { /* ctrl-u: clear input */
        db->fuzzy_len = 0;
        db->fuzzy_buf[0] = '\0';
        fuzzy_recompute(db);
        return;
    }

    if (ch >= 0x20 && ch < 0x7f && db->fuzzy_len < 254) {
        db->fuzzy_buf[db->fuzzy_len++] = (char)ch;
        db->fuzzy_buf[db->fuzzy_len] = '\0';
        fuzzy_recompute(db);
    }
}

#endif /* PHOSPHOR_HAS_NCURSES */

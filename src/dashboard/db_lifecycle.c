#ifdef PHOSPHOR_HAS_NCURSES

#include "db_types.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"
#include "phosphor/serve.h"

#ifdef PHOSPHOR_HAS_CJSON
#include "phosphor/json.h"
#include <cJSON.h>
#include <stdlib.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

/* ---- helpers ---- */

void set_cmd_msg(ph_dashboard_t *db, const char *msg, int frames) {
    snprintf(db->cmd_msg, sizeof(db->cmd_msg), "%s", msg);
    db->cmd_msg_frames = frames;
}

bool any_child_running(ph_dashboard_t *db) {
    for (int i = 0; i < db->panel_count; i++) {
        if (db->panels[i].status == PANEL_RUNNING)
            return true;
    }
    return false;
}

bool any_fd_open(ph_dashboard_t *db) {
    for (int i = 0; i < db->panel_count; i++) {
        if (db->panels[i].stdout_fd >= 0 ||
            db->panels[i].stderr_fd >= 0)
            return true;
    }
    return false;
}

int compute_worst_exit(ph_dashboard_t *db) {
    int worst = 0;
    for (int i = 0; i < db->panel_count; i++) {
        if (db->panels[i].exit_code > worst)
            worst = db->panels[i].exit_code;
    }
    return worst;
}

/* ---- shutdown ---- */

void shutdown_children(ph_dashboard_t *db) {
    /* send SIGTERM to any still-running children */
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->pid > 0 && p->status == PANEL_RUNNING)
            kill(-(p->pid), SIGTERM);
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

    /* kill shell screen processes */
    if (db->shell_open) {
        for (int v = 0; v < db->shell_view_count; v++) {
            db_shell_view_t *view = &db->shell_views[v];
            for (int s = 0; s < view->screen_count; s++) {
                db_shell_screen_t *scr = &view->screens[s];
                if (scr->pid > 0 && scr->status == DB_SCREEN_RUNNING)
                    kill(-(scr->pid), SIGTERM);
            }
        }
    }
}

/* ---- clear ---- */

void action_clear(ph_dashboard_t *db) {
    db_panel_t *p = &db->panels[db->focused];

    if (p->tab_count > 0) {
        db_tab_t *tab = &p->tabs[p->active_tab];
        ringbuf_destroy(&tab->ring);
        memset(&tab->ring, 0, sizeof(db_ringbuf_t));
        tab->scroll = 0;
        tab->cursor = -1;
        tab->sel_anchor = -1;
        if (tab->json_fold_text) { ph_free(tab->json_fold_text); tab->json_fold_text = NULL; }
        tab->json_fold_idx = -1;
        tab->json_fold_lines = 0;
    } else {
        ringbuf_destroy(&p->ring);
        memset(&p->ring, 0, sizeof(db_ringbuf_t));
        p->scroll = 0;
        p->cursor = -1;
        p->sel_anchor = -1;
        json_fold_cleanup(p);
    }
    memset(&p->out_acc, 0, sizeof(db_accum_t));
    memset(&p->err_acc, 0, sizeof(db_accum_t));

    char msg[128];
    if (p->tab_count > 0)
        snprintf(msg, sizeof(msg), "cleared: %s [%s]",
                 p->name, p->tabs[p->active_tab].name);
    else
        snprintf(msg, sizeof(msg), "cleared: %s", p->name);
    set_cmd_msg(db, msg, 20);
}

/* ---- save helpers ---- */

/* return the configured log directory or "." as fallback */
static const char *log_base_dir(ph_dashboard_t *db) {
    if (db->serve_cfg && db->serve_cfg->ns.log_directory)
        return db->serve_cfg->ns.log_directory;
    return ".";
}

#ifdef PHOSPHOR_HAS_CJSON

/* build today's date prefix "DD.MM.YYYY" */
static void save_date_prefix(char *buf, size_t sz) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(buf, sz, "%02d.%02d.%04d",
             tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
}

/* find the next numeric key in a cJSON object (0, 1, 2, ...) */
static int save_next_key(cJSON *obj) {
    int next = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, obj) {
        if (item->string) {
            int v = atoi(item->string);
            if (v >= next) next = v + 1;
        }
    }
    return next;
}

/* dump ring buffer lines into a cJSON object with "0","1",... keys */
static int save_ringbuf_to_json(db_ringbuf_t *ring, cJSON *slot) {
    char stripped[MAX_LINE_LEN];
    char idx_str[16];
    int count = ring->count;
    int saved = 0;
    for (int i = 0; i < count; i++) {
        db_line_t *line = ringbuf_get(ring, i);
        if (!line || !line->text) continue;
        int slen = strip_ansi(stripped, line->text, line->len);
        stripped[slen] = '\0';
        snprintf(idx_str, sizeof(idx_str), "%d", saved);
        cJSON_AddStringToObject(slot, idx_str, stripped);
        saved++;
    }
    return saved;
}

/* clear a single panel's rings + accumulators (all tabs or legacy) */
static void save_clear_panel(db_panel_t *p) {
    if (p->tab_count > 0) {
        for (int t = 0; t < p->tab_count; t++) {
            db_tab_t *tab = &p->tabs[t];
            ringbuf_destroy(&tab->ring);
            memset(&tab->ring, 0, sizeof(db_ringbuf_t));
            tab->scroll = 0;
            tab->cursor = -1;
            tab->sel_anchor = -1;
            if (tab->json_fold_text) { ph_free(tab->json_fold_text); tab->json_fold_text = NULL; }
            tab->json_fold_idx = -1;
            tab->json_fold_lines = 0;
        }
    } else {
        ringbuf_destroy(&p->ring);
        memset(&p->ring, 0, sizeof(db_ringbuf_t));
        p->scroll = 0;
        p->cursor = -1;
        p->sel_anchor = -1;
        json_fold_cleanup(p);
    }
    memset(&p->out_acc, 0, sizeof(db_accum_t));
    memset(&p->err_acc, 0, sizeof(db_accum_t));
}

/* write cJSON root to file */
static bool save_write_json(cJSON *root, const char *fname) {
    char *out = cJSON_Print(root);
    if (!out) return false;
    FILE *f = fopen(fname, "w");
    if (!f) { free(out); return false; }
    fwrite(out, 1, strlen(out), f);
    fclose(f);
    free(out);
    return true;
}

#endif /* PHOSPHOR_HAS_CJSON */

/* ---- :save <path> ---- */

void action_save(ph_dashboard_t *db, const char *path) {
#ifndef PHOSPHOR_HAS_CJSON
    (void)path;
    set_cmd_msg(db, "save: cJSON not available", 30);
#else
    db_panel_t *p = &db->panels[db->focused];
    db_ringbuf_t *ring = panel_ring(p);

    if (ring->count == 0) {
        set_cmd_msg(db, "save: panel is empty", 20);
        return;
    }

    /* build filename: DD.MM.YYYY.<panel>[.<tab>].<path>.json */
    char date[16];
    save_date_prefix(date, sizeof(date));

    const char *logdir = log_base_dir(db);
    mkdir(logdir, 0755);

    char fname[512];
    if (p->tab_count > 0)
        snprintf(fname, sizeof(fname), "%s/%s.%s.%s.%s.json",
                 logdir, date, p->name ? p->name : "panel",
                 p->tabs[p->active_tab].name, path);
    else
        snprintf(fname, sizeof(fname), "%s/%s.%s.%s.json",
                 logdir, date, p->name ? p->name : "panel", path);
    for (char *c = fname; *c; c++) {
        if (*c == ' ') *c = '_';
    }

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
        set_cmd_msg(db, "save: alloc failed", 30);
        return;
    }

    /* add next numbered save slot */
    int slot_idx = save_next_key(root);
    char slot_key[16];
    snprintf(slot_key, sizeof(slot_key), "%d", slot_idx);

    cJSON *slot = cJSON_CreateObject();
    if (!slot) {
        cJSON_Delete(root);
        set_cmd_msg(db, "save: alloc failed", 30);
        return;
    }
    cJSON_AddItemToObject(root, slot_key, slot);

    int saved = save_ringbuf_to_json(ring, slot);

    /* write */
    if (!save_write_json(root, fname)) {
        cJSON_Delete(root);
        char msg[256];
        snprintf(msg, sizeof(msg), "save: cannot write %s", fname);
        set_cmd_msg(db, msg, 30);
        return;
    }
    cJSON_Delete(root);

    /* clear focused panel */
    save_clear_panel(p);

    char msg[256];
    snprintf(msg, sizeof(msg), "saved %d lines [%d] to %s", saved, slot_idx, fname);
    set_cmd_msg(db, msg, 30);
#endif /* PHOSPHOR_HAS_CJSON */
}

/* ---- :saveall ---- */

void action_saveall(ph_dashboard_t *db) {
#ifndef PHOSPHOR_HAS_CJSON
    set_cmd_msg(db, "saveall: cJSON not available", 30);
#else
    /* check if any panel/tab has data */
    bool has_data = false;
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *pp = &db->panels[i];
        if (pp->tab_count > 0) {
            for (int t = 0; t < pp->tab_count; t++)
                if (pp->tabs[t].ring.count > 0) { has_data = true; break; }
        } else {
            if (pp->ring.count > 0) has_data = true;
        }
        if (has_data) break;
    }
    if (!has_data) {
        set_cmd_msg(db, "saveall: all panels empty", 20);
        return;
    }

    /* build filename: <logdir>/DD.MM.YYYY.all.json */
    char date[16];
    save_date_prefix(date, sizeof(date));

    const char *logdir = log_base_dir(db);
    mkdir(logdir, 0755);

    char fname[256];
    snprintf(fname, sizeof(fname), "%s/%s.all.json", logdir, date);

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
        set_cmd_msg(db, "saveall: alloc failed", 30);
        return;
    }

    /* add next numbered save slot */
    int slot_idx = save_next_key(root);
    char slot_key[16];
    snprintf(slot_key, sizeof(slot_key), "%d", slot_idx);

    cJSON *slot = cJSON_CreateObject();
    if (!slot) {
        cJSON_Delete(root);
        set_cmd_msg(db, "saveall: alloc failed", 30);
        return;
    }
    cJSON_AddItemToObject(root, slot_key, slot);

    /* dump each panel into the slot keyed by panel name */
    int total_saved = 0;
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        const char *name = p->name ? p->name : "panel";

        if (p->tab_count > 0) {
            /* tabbed panel: each tab is a sub-object */
            cJSON *panel_obj = cJSON_CreateObject();
            if (!panel_obj) continue;
            cJSON_AddItemToObject(slot, name, panel_obj);
            for (int t = 0; t < p->tab_count; t++) {
                cJSON *tab_obj = cJSON_CreateObject();
                if (!tab_obj) continue;
                cJSON_AddItemToObject(panel_obj, p->tabs[t].name, tab_obj);
                total_saved += save_ringbuf_to_json(&p->tabs[t].ring, tab_obj);
            }
        } else {
            cJSON *panel_obj = cJSON_CreateObject();
            if (!panel_obj) continue;
            cJSON_AddItemToObject(slot, name, panel_obj);
            total_saved += save_ringbuf_to_json(&p->ring, panel_obj);
        }
    }

    /* write */
    if (!save_write_json(root, fname)) {
        cJSON_Delete(root);
        char msg[256];
        snprintf(msg, sizeof(msg), "saveall: cannot write %s", fname);
        set_cmd_msg(db, msg, 30);
        return;
    }
    cJSON_Delete(root);

    /* clear all panels */
    for (int i = 0; i < db->panel_count; i++)
        save_clear_panel(&db->panels[i]);

    char msg[256];
    snprintf(msg, sizeof(msg), "saved %d lines [%d] to %s",
             total_saved, slot_idx, fname);
    set_cmd_msg(db, msg, 30);
#endif /* PHOSPHOR_HAS_CJSON */
}

/* ---- start / stop ---- */

void action_stop(ph_dashboard_t *db) {
    if (!any_child_running(db)) {
        set_cmd_msg(db, "No running processes", 30);
        return;
    }

    /* send SIGTERM to all running children's process groups */
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        if (p->pid > 0 && p->status == PANEL_RUNNING)
            kill(-(p->pid), SIGTERM);
    }

    set_cmd_msg(db, "Stopping...", 20);
}

void rewire_panels(ph_dashboard_t *db, ph_serve_session_t *s) {
    /* re-populate pid/stdout_fd/stderr_fd from new session */
    for (int i = 0; i < db->panel_count; i++) {
        db_panel_t *p = &db->panels[i];
        switch (p->id) {
        case PH_PANEL_NEONSIGNAL:
            p->pid = ph_serve_ns_pid(s);
            p->stdout_fd = ph_serve_ns_stdout_fd(s);
            p->stderr_fd = ph_serve_ns_stderr_fd(s);
            break;
        case PH_PANEL_REDIRECT:
            p->pid = ph_serve_redir_pid(s);
            p->stdout_fd = ph_serve_redir_stdout_fd(s);
            p->stderr_fd = ph_serve_redir_stderr_fd(s);
            break;
        case PH_PANEL_WATCHER:
            p->pid = ph_serve_watch_pid(s);
            p->stdout_fd = ph_serve_watch_stdout_fd(s);
            p->stderr_fd = ph_serve_watch_stderr_fd(s);
            break;
        }
        p->status = (p->pid > 0) ? PANEL_RUNNING : PANEL_EXITED;
        p->exit_code = 0;
        /* ring buffers preserved (log history retained across restart) */
        memset(&p->out_acc, 0, sizeof(db_accum_t));
        memset(&p->err_acc, 0, sizeof(db_accum_t));
        if (p->pid > 0) db->alive++;
    }
}

void action_start(ph_dashboard_t *db) {
    if (!db->serve_cfg || !db->session_ptr) {
        set_cmd_msg(db, "No serve config available", 30);
        return;
    }

    if (any_child_running(db)) {
        set_cmd_msg(db, "Server already running", 30);
        return;
    }

    /* destroy old session */
    if (*db->session_ptr) {
        ph_serve_destroy(*db->session_ptr);
        *db->session_ptr = NULL;
    }

    /* reset alive counter */
    db->alive = 0;

    /* start new session */
    ph_error_t *err = NULL;
    if (ph_serve_start(db->serve_cfg, db->session_ptr, &err) != PH_OK) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Start failed: %s",
                 err ? err->message : "unknown");
        set_cmd_msg(db, msg, 40);
        ph_error_destroy(err);
        return;
    }

    rewire_panels(db, *db->session_ptr);

    /* one-shot full repaint -- terminal size unchanged, no re-layout needed */
    clearok(stdscr, TRUE);

    set_cmd_msg(db, "Started", 20);
}

/* ---- button activation ---- */

void activate_button(ph_dashboard_t *db) {
    switch (db->btn_selected) {
    case DB_BTN_START:
        action_start(db);
        break;
    case DB_BTN_STOP:
        action_stop(db);
        break;
    case DB_BTN_NONE:
        break;
    }
    db->btn_selected = DB_BTN_NONE;
    db->btn_flash = 3;
}

/* ---- export selection to JSON ---- */

void action_export_selection(ph_dashboard_t *db) {
#ifndef PHOSPHOR_HAS_CJSON
    set_cmd_msg(db, "export: cJSON not available", 30);
    return;
#else
    db_panel_t *p = &db->panels[db->focused];
    int p_sel = *panel_sel_anchor(p);
    int p_cur = *panel_cursor(p);

    if (p_sel < 0 || p_cur < 0) {
        set_cmd_msg(db, "no selection", 20);
        return;
    }

    int sel_lo = p_sel < p_cur ? p_sel : p_cur;
    int sel_hi = p_sel > p_cur ? p_sel : p_cur;

    /* build filename: <logdir>/phosphor.<name>.json (sanitize spaces) */
    const char *logdir = log_base_dir(db);
    mkdir(logdir, 0755);

    char fname[256];
    snprintf(fname, sizeof(fname), "%s/phosphor.%s.json",
             logdir, p->name ? p->name : "panel");
    for (char *c = fname; *c; c++) {
        if (*c == ' ') *c = '_';
    }

    /* read existing file or create new root */
    ph_json_t *root = ph_json_parse_file(fname);
    if (!root) root = ph_json_create_object();
    if (!root) {
        set_cmd_msg(db, "export: alloc failed", 30);
        return;
    }

    /* today's date key: DDMMYYYY */
    char datekey[16];
    {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        snprintf(datekey, sizeof(datekey), "%02d%02d%04d",
                 tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
    }

    /* get or create today's object */
    ph_json_t *day = ph_json_get_object(root, datekey);
    if (!day) day = ph_json_add_object(root, datekey);
    if (!day) {
        ph_json_destroy(root);
        set_cmd_msg(db, "export: alloc failed", 30);
        return;
    }

    /* find highest existing numeric key in today's object */
    int next_idx = 0;
    {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, (cJSON *)day) {
            if (item->string) {
                int v = atoi(item->string);
                if (v >= next_idx) next_idx = v + 1;
            }
        }
    }

    /* export selected lines */
    char stripped[MAX_LINE_LEN];
    char idx_str[16];
    int exported = 0;
    db_ringbuf_t *ering = panel_ring(p);
    for (int i = sel_lo; i <= sel_hi; i++) {
        db_line_t *line = ringbuf_get(ering, i);
        if (!line || !line->text) continue;
        int slen = strip_ansi(stripped, line->text, line->len);
        stripped[slen] = '\0';
        snprintf(idx_str, sizeof(idx_str), "%d", next_idx++);
        ph_json_add_string(day, idx_str, stripped);
        exported++;
    }

    /* write file */
    char *out = ph_json_print(root);
    ph_json_destroy(root);

    if (out) {
        FILE *f = fopen(fname, "w");
        if (f) {
            fwrite(out, 1, strlen(out), f);
            fclose(f);
        }
        ph_json_free_string(out);
    }

    /* clear selection */
    *panel_sel_anchor(p) = -1;

    char msg[256];
    snprintf(msg, sizeof(msg), "exported %d lines to %s", exported, fname);
    set_cmd_msg(db, msg, 30);
#endif /* PHOSPHOR_HAS_CJSON */
}

#endif /* PHOSPHOR_HAS_NCURSES */

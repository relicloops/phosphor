# Plan: Dashboard Embedded Shell Panel

## Context

The dashboard needs an embedded interactive terminal. Users want to run shell
commands without leaving the TUI -- inspecting files, running curl, grepping
logs, etc. The shell sits below the process panels and above the status bar.

### Terminology

- **shell** -- resizable panel region at the bottom, contains views (tabs)
- **view** -- a tab inside the shell with its own input line and screen list
- **screen** -- popup overlay showing one command's output (scrollable, saveable)

### Keybindings

| Key | Action |
|-----|--------|
| Alt+F11 | Toggle shell open / open new view |
| Alt+F12 | Close shell entirely (kill all) |
| Ctrl+Shift+Up | Increase shell height |
| Ctrl+Shift+Down | Decrease shell height |
| Ctrl+[1-9] | Jump to screen by number |
| Ctrl+X | Close focused screen |
| Ctrl+S | Save screen output to `cwd/shell/[date].command.txt` |
| Ctrl+M | Minimize/restore focused screen |
| Esc | Remove focus from shell, minimize all screens (shell stays open) |

### Behavior

- Each view has an input line (`$ ...`). Command typed there, Enter executes.
- Output appears in a **screen** popup overlay (not inline in the view).
- Blocking commands disable the view's input until the process exits.
- Each command spawns via PTY using `$SHELL -c "command"`.
- Multiple screens per view; Ctrl+[1-9] jumps to screen N, Ctrl+X closes, Ctrl+M minimizes.
- Minimized screens keep running but hide their overlay.

---

## Current State (from `db_types.h`)

- Modes: `DB_MODE_NORMAL, COMMAND, POPUP, SEARCH, FUZZY`
- Layout: info_box (top) -> panels (middle) -> status_bar (bottom row)
- `layout_panels()`: `avail_rows = rows - info_h - 1` (1 for status bar)
- No PTY support -- only pipe-based spawning exists
- `MAX_FDS = PH_DASHBOARD_MAX_PANELS * 2 + 2` = 8
- Color pairs: up to CP_TAB_INACTIVE = 49

---

## Data Model (all in `db_types.h`)

### New Constants

```c
#define DB_SHELL_MAX_VIEWS    4
#define DB_SHELL_MAX_SCREENS  16
#define DB_SHELL_INPUT_LEN    1024
#define DB_SHELL_MIN_HEIGHT   3
#define DB_SHELL_MAX_HEIGHT   40
#define DB_SHELL_DEFAULT_H    8
```

### MAX_FDS Update

```c
#define MAX_SHELL_FDS  DB_SHELL_MAX_SCREENS  /* practical: one pty fd per screen */
#define MAX_FDS        (PH_DASHBOARD_MAX_PANELS * 2 + 2 + MAX_SHELL_FDS)
```

### New Enums

```c
/* add to db_mode_t */
DB_MODE_SHELL,

/* add to db_evt_type_t */
DB_EVT_SHELL_DATA,
DB_EVT_SHELL_EOF,
```

### New Color Pairs

```c
CP_SHELL_BORDER       = 50,
CP_SHELL_INPUT        = 51,
CP_SHELL_PROMPT       = 52,
CP_SHELL_TAB_ACTIVE   = 53,
CP_SHELL_TAB_INACTIVE = 54,
CP_SCREEN_BORDER      = 55,
CP_SCREEN_TITLE       = 56,
```

### Screen Struct

```c
typedef enum { DB_SCREEN_RUNNING, DB_SCREEN_EXITED } db_screen_status_t;

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
```

### View Struct

```c
typedef struct {
    char                  input[DB_SHELL_INPUT_LEN];
    int                   input_len;
    int                   input_cursor;
    db_shell_screen_t     screens[DB_SHELL_MAX_SCREENS];
    int                   screen_count;
    int                   active_screen;   /* -1 = none focused */
    bool                  busy;            /* blocking command running */
} db_shell_view_t;
```

### Shell Event Data

```c
typedef struct {
    int   view_idx;
    int   screen_idx;
    char  buf[4096];
    int   len;
} db_evt_shell_t;
```

Add `db_evt_shell_t shell;` to `db_event_t` union.

### Shell State on `struct ph_dashboard`

```c
/* shell */
bool              shell_open;
int               shell_height;
db_shell_view_t   shell_views[DB_SHELL_MAX_VIEWS];
int               shell_view_count;
int               shell_active_view;
WINDOW           *shell_win;
```

### Key Constants

```c
#define KEY_ALT_F11       (KEY_MAX - 1)
#define KEY_ALT_F12       (KEY_MAX - 2)
#define KEY_CTRL_SHIFT_UP (KEY_MAX - 3)
#define KEY_CTRL_SHIFT_DN (KEY_MAX - 4)
```

---

## Phases

### Phase 1: Types + constants (`db_types.h`)

Add all new structs, enums, constants, color pairs, key defines listed above.
Add `DB_MODE_SHELL` to mode enum.
Add `DB_EVT_SHELL_DATA`/`DB_EVT_SHELL_EOF` to event enum.
Add shell event to event union.
Add shell state to `struct ph_dashboard`.
Increase `MAX_FDS`.
Add cross-file declarations for new functions.

### Phase 2: PTY spawning + shell lifecycle (`db_shell.c` -- NEW)

**PTY allocation:** `posix_openpt()` + `grantpt()` + `unlockpt()` + `ptsname()`.
Available under `_POSIX_C_SOURCE=200809L`. Child uses `setsid()` + `open(slave)` +
`dup2()` (avoids `login_tty()` BSD dependency).

**Functions:**
- `shell_spawn_command(cmd, *out_master_fd, rows, cols)` -- fork+pty, exec `$SHELL -c cmd`
- `shell_toggle_or_new_view(db)` -- open shell or add view tab
- `shell_close_all(db)` -- kill all, free all, close shell
- `shell_resize(db, delta)` -- adjust shell_height, call layout_panels
- `shell_execute_command(db, view)` -- create screen, spawn command
- `shell_goto_screen(view, idx)` -- Ctrl+[1-9]: jump to screen by number
- `shell_close_screen(db, view, idx)` -- Ctrl+X: kill, free, compact array
- `shell_save_screen(db, scr)` -- Ctrl+S: write to `cwd/shell/[date].command.txt`
- `handle_shell_data(db, evt)` -- feed PTY output into screen ring buffer
- `handle_shell_eof(db, evt)` -- close PTY fd
- `draw_shell(db)` -- render shell panel: border, view tabs, screen list, input line
- `draw_shell_screens(db)` -- render active non-minimized screen as overlay popup

### Phase 3: Shell key handler (`db_shell.c` -- same file)

**`handle_key_shell(db, ch)`:**
- When screen visible + non-minimized: Ctrl+[1-9] jump to screen N, Ctrl+X (0x18) close,
  Ctrl+S (0x13) save, Ctrl+M/Enter (0x0D) minimize, arrows scroll screen
- When no visible screen or input focused: character input, Enter executes,
  Backspace/arrows edit
- Esc: minimizes all screens in the active view, returns to DB_MODE_NORMAL
  (shell stays open, processes keep running)

**Ctrl+M vs Enter conflict:** 0x0D is both. Disambiguation: if a non-minimized
screen overlay is visible, 0x0D = minimize. Otherwise 0x0D = submit input.

### Phase 4: Key registration + global dispatch (`dashboard.c`, `db_evt_key.c`)

**In `ph_dashboard_create()`:**
```c
define_key("\033\033[23~", KEY_ALT_F11);
define_key("\033[23;3~",   KEY_ALT_F11);
define_key("\033\033[24~", KEY_ALT_F12);
define_key("\033[24;3~",   KEY_ALT_F12);
define_key("\033[1;6A",    KEY_CTRL_SHIFT_UP);
define_key("\033[1;6B",    KEY_CTRL_SHIFT_DN);
```

**In `handle_key()` (before mode switch):**
```c
if (ch == KEY_ALT_F11)       { shell_toggle_or_new_view(db); return; }
if (ch == KEY_ALT_F12)       { shell_close_all(db); return; }
if (ch == KEY_CTRL_SHIFT_UP  && db->shell_open) { shell_resize(db, +1); return; }
if (ch == KEY_CTRL_SHIFT_DN  && db->shell_open) { shell_resize(db, -1); return; }
```

Add `case DB_MODE_SHELL: handle_key_shell(db, ch); break;` to mode switch.

### Phase 5: Layout changes (`db_layout.c`)

When `shell_open`, subtract `shell_height` from `avail_rows`:
```c
int shell_h = db->shell_open ? db->shell_height : 0;
int avail_rows = rows - info_h - 1 - shell_h;
```

Position shell window above status bar:
```c
int shell_y = rows - 1 - shell_h;
db->shell_win = newwin(shell_h, cols, shell_y, 0);
```

### Phase 6: Event loop (`db_event.c`)

In `collect_events()`: add all active screen `pty_master_fd` values to the pollfd
array (after panel fds, before stdin). Track `view_idx`/`screen_idx` per fd.
On POLLIN: read into `DB_EVT_SHELL_DATA`. On EOF: `DB_EVT_SHELL_EOF`.

In `handle_event()`: dispatch `DB_EVT_SHELL_DATA` -> `handle_shell_data()`,
`DB_EVT_SHELL_EOF` -> `handle_shell_eof()`.

Extend `reap_children()`: also waitpid on all shell screen PIDs, mark
`DB_SCREEN_EXITED`, set `view->busy = false`.

### Phase 7: Drawing integration (`db_draw.c`)

In `draw_all()`, after panel drawing and before status bar:
```c
if (db->shell_open) draw_shell(db);
```

After status bar, before popups:
```c
if (db->shell_open) draw_shell_screens(db);
```

### Phase 8: Init + destroy (`dashboard.c`)

- Init: zero shell fields in `ph_dashboard_create()`, add color pair inits
- Destroy: call `shell_close_all(db)` in `ph_dashboard_destroy()`
- Shutdown: kill shell screen processes in `shutdown_children()`

### Phase 9: Build (`meson.build`)

Add `src/dashboard/db_shell.c` to `phosphor_sources`.

---

## Files Modified

| File | Changes |
|------|---------|
| `src/dashboard/db_types.h` | All new types, constants, enums, key defines, shell state on dashboard, cross-file declarations |
| `src/dashboard/db_shell.c` | NEW: PTY spawn, lifecycle, key handler, drawing, data handlers |
| `src/dashboard/dashboard.c` | Init shell state, define_key calls, color pairs, destroy cleanup |
| `src/dashboard/db_layout.c` | Subtract shell_height, create shell window |
| `src/dashboard/db_event.c` | Poll shell PTY fds, dispatch shell events |
| `src/dashboard/db_evt_key.c` | Global Alt+F11/F12/Ctrl+Shift+arrows intercept, DB_MODE_SHELL dispatch |
| `src/dashboard/db_evt_child.c` | Reap shell screen children |
| `src/dashboard/db_draw.c` | Call draw_shell/draw_shell_screens in draw_all |
| `src/dashboard/db_lifecycle.c` | Kill shell processes in shutdown_children |
| `meson.build` | Add db_shell.c to sources |

---

## Key Conflicts

| Key | Current Use | Shell Use | Conflict? |
|-----|------------|-----------|-----------|
| Ctrl+[1-9] | Unused | Jump to screen N | No |
| Ctrl+X (0x18) | Unused | Close screen | No |
| Ctrl+S (0x13) | Start button (normal mode) | Save screen (shell mode) | No -- different modes |
| Ctrl+M (0x0D) | Enter/CR | Minimize screen | Yes -- disambiguate by context |
| Alt+F11 | Unused | Toggle shell | No |
| Alt+F12 | Unused | Close shell | No |

**Ctrl+M resolution:** When active non-minimized screen visible -> minimize.
Otherwise -> submit input (Enter).

---

## Verification

1. `meson setup build --wipe && ninja -C build` -- zero warnings
2. `./build/phosphor version` -- smoke test
3. `phosphor serve` in a project:
   - Alt+F11: shell panel appears below panels, above status bar
   - Type `ls` + Enter: screen popup shows directory listing
   - Ctrl+Shift+Up/Down: shell panel resizes
   - Alt+F11 again: second view tab appears
   - Execute another command: second screen in the view
   - Ctrl+2: jumps to screen 2
   - Ctrl+M: minimizes/restores screen overlay
   - Ctrl+S: saves output to `shell/[date].ls.txt`
   - Ctrl+X: closes focused screen
   - Alt+F12: everything closes, processes killed
   - Esc: all screens minimize, focus returns to panels (shell stays open)

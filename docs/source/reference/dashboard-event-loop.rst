.. meta::
   :title: dashboard event loop architecture
   :tags: #dashboard, #ncurses, #phosphor, #event-loop
   :status: active
   :updated: 2026-04-04

.. index::
   single: dashboard
   single: event loop
   single: ncurses TUI
   pair: architecture; event-driven

dashboard event loop architecture
=================================

the ``phosphor serve`` dashboard is an ncurses TUI that monitors child
processes (neonsignal, neonsignal_redirect, file watcher) in real time.
it was refactored from a monolithic 960-line file into 13 files with
maximum separation of concerns: one file per subsystem, one file per
event type.

this chapter documents the internal architecture: how events are
collected, dispatched, and rendered, and how the UI state machine
controls keyboard behavior.

headers: ``phosphor/dashboard.h``, ``src/dashboard/db_types.h``

file layout
-----------

every ``db_*.c`` file has a single responsibility. all share the internal
types header ``db_types.h``.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - File
     - Responsibility
   * - ``db_types.h``
     - all internal types, enums, structs, cross-file declarations
   * - ``db_ring.c``
     - ring buffer, line accumulator, line cleaner, UTF-8 helper, ANSI stripper
   * - ``db_layout.c``
     - ``layout_panels()`` -- window geometry calculation
   * - ``db_draw.c``
     - all rendering: info box, panels, status bar, buttons, ANSI SGR
   * - ``db_popup.c``
     - popup open/close/draw, help/about/commands/phosphor-help content
   * - ``db_evt_pipe.c``
     - ``handle_pipe_data()``, ``handle_pipe_eof()``
   * - ``db_evt_signal.c``
     - ``handle_signal()`` (SIGINT/SIGTERM), ``handle_winch()`` (resize)
   * - ``db_evt_child.c``
     - ``handle_child_exit()``, ``reap_children()``
   * - ``db_evt_key.c``
     - ``handle_key()`` mode dispatch, normal/command/popup/search handlers
   * - ``db_evt_tick.c``
     - ``handle_tick()`` -- message frame countdown, button flash
   * - ``db_event.c``
     - ``collect_events()``, ``handle_event()`` switch dispatch
   * - ``db_fuzzy.c``
     - fuzzy log finder: file scanning, fuzzy scoring, popup drawing,
       two-phase UI (file picker, line search)
   * - ``db_json_fold.c``
     - inline JSON fold toggle, JSON viewer popup with tree folding,
       node builder, syntax-highlighted rendering, fold/unfold navigation
   * - ``db_lifecycle.c``
     - ``action_start()``, ``action_stop()``, ``action_clear()``,
       ``action_save()``, ``action_saveall()``, ``action_export_selection()``,
       ``rewire_panels()``, ``shutdown_children()``
   * - ``db_shell.c``
     - embedded shell panel: PTY spawning, view/screen lifecycle,
       key handler, drawing (shell panel + screen overlays)
   * - ``dashboard.c``
     - thin glue: ``ph_dashboard_create()``, ``ph_dashboard_run()``,
       ``ph_dashboard_destroy()``

collect / dispatch / render cycle
---------------------------------

the event loop in ``ph_dashboard_run()`` follows a three-phase cycle::

    while (!db->quit) {
        1. collect_events()   -- poll + signals + keyboard + reap
        2. handle_event()     -- dispatch each event to its handler
        3. draw_all()         -- re-render everything to the screen
    }

this is a purely single-threaded design. all I/O is non-blocking. the
``poll()`` call is the only blocking point, with a 100ms timeout that
doubles as the tick rate for animated elements (message countdowns,
button flash).

event types
-----------

events are represented as a tagged union allocated on the stack::

    typedef struct {
        db_evt_type_t type;
        union {
            db_evt_pipe_t  pipe;   -- panel_idx, is_stderr, buf[], len
            db_evt_child_t child;  -- panel_idx, exit_code
            db_evt_key_t   key;    -- ch (keypress code)
        } d;
    } db_event_t;

the event types and their sources:

.. list-table::
   :header-rows: 1
   :widths: 25 40 35

   * - Event
     - Source
     - Handler file
   * - ``DB_EVT_PIPE_DATA``
     - ``poll()`` POLLIN on panel stdout/stderr fd
     - ``db_evt_pipe.c``
   * - ``DB_EVT_PIPE_EOF``
     - ``read()`` returns 0 on panel fd
     - ``db_evt_pipe.c``
   * - ``DB_EVT_CHILD_EXIT``
     - ``waitpid()`` WNOHANG reap after poll
     - ``db_evt_child.c``
   * - ``DB_EVT_KEY``
     - ``getch()`` after poll (ncurses nodelay mode)
     - ``db_evt_key.c``
   * - ``DB_EVT_SIGNAL``
     - ``ph_signal_interrupted()`` flag (SIGINT/SIGTERM)
     - ``db_evt_signal.c``
   * - ``DB_EVT_WINCH``
     - ``ph_signal_winch_pending()`` flag (SIGWINCH)
     - ``db_evt_signal.c``
   * - ``DB_EVT_TICK``
     - emitted when no other events occurred in poll cycle
     - ``db_evt_tick.c``

the event queue is a fixed-size ``db_event_t events[MAX_EVENTS]`` array on
the stack. ``collect_events()`` fills it; no heap allocation occurs in the
hot path.

event collection
~~~~~~~~~~~~~~~~

``collect_events()`` in ``db_event.c`` follows this sequence:

1. build ``pollfd`` array from all open panel pipe fds + signal pipe
2. ``poll(fds, nfds, POLL_TIMEOUT_MS)``
3. drain signal pipe on POLLIN or EINTR (non-blocking read loop)
4. check ``ph_signal_interrupted()`` -> push ``DB_EVT_SIGNAL``
5. check ``ph_signal_winch_pending()`` -> push ``DB_EVT_WINCH``
6. read each readable pipe fd -> push ``DB_EVT_PIPE_DATA`` or ``DB_EVT_PIPE_EOF``
7. ``reap_children()`` -> push ``DB_EVT_CHILD_EXIT`` for each reaped child
8. ``getch()`` loop -> push ``DB_EVT_KEY`` for each keypress
9. if nevents == 0 -> push ``DB_EVT_TICK``

event dispatch
~~~~~~~~~~~~~~

``handle_event()`` is a simple switch::

    void handle_event(ph_dashboard_t *db, const db_event_t *evt) {
        switch (evt->type) {
        case DB_EVT_SIGNAL:     handle_signal(db);                    break;
        case DB_EVT_WINCH:      handle_winch(db);                     break;
        case DB_EVT_PIPE_DATA:  handle_pipe_data(db, &evt->d.pipe);   break;
        case DB_EVT_PIPE_EOF:   handle_pipe_eof(db, &evt->d.pipe);    break;
        case DB_EVT_CHILD_EXIT: handle_child_exit(db, &evt->d.child); break;
        case DB_EVT_KEY:        handle_key(db, evt->d.key.ch);        break;
        case DB_EVT_TICK:       handle_tick(db);                       break;
        case DB_EVT_NONE:       break;
        }
    }

UI mode state machine
---------------------

the dashboard has six input modes that control how keyboard events
are interpreted::

    DB_MODE_NORMAL  <--Esc--  DB_MODE_COMMAND  (':'  enters)
         |                         |
         |  '?' or 'a'            Enter (execute)
         v                         |
    DB_MODE_POPUP  --Esc/q/Enter--> DB_MODE_NORMAL
         ^                              |
         |  Enter (open file)           |  'g' (fuzzy)
         |                              v
    DB_MODE_FUZZY  <--Esc-----------+
         |                              |
         DB_MODE_SEARCH  <--'/'----+    |
              |                         |
              +--Enter/Esc-----------> (back)

- **NORMAL**: navigation, scrolling, zoom (``f``), clear (``c``),
  search jumps (``n``/``N``), button selection (Ctrl-S/T),
  popup triggers (``?``, ``a``), select mode (``v``), fuzzy log
  finder (``g``), JSON fold (``z``), quit (``q``).
- **COMMAND**: vi-style command line (``:start``, ``:stop``, ``:save``,
  ``:saveall``, ``:clear``). Esc cancels, Enter executes.
- **SEARCH**: pattern input for log search. ``/`` enters from NORMAL.
  Enter confirms (activates highlighting + n/N navigation). Esc cancels.
- **FUZZY**: popup overlay for fuzzy log file search. ``g`` enters from
  NORMAL. file picker phase lists ``.json`` files in cwd; Enter opens
  the selected file in the JSON viewer popup. Esc returns to NORMAL.
- **POPUP**: modal overlay. help popup navigates to commands (``c``) or
  phosphor help (``h``); sub-popups return to help with ``?``. JSON viewer
  popup has its own key handler with fold/unfold navigation. Esc closes
  popup back to previous mode (FUZZY for JSON viewer, NORMAL for others).
- **SHELL**: embedded shell panel has focus. input goes to the view's
  command line. screen overlays are navigable. ``Esc`` minimizes all
  screens and returns to NORMAL. ``Alt+F11`` enters from any mode.

mode transitions are handled in ``db_evt_key.c::handle_key()``::

    void handle_key(ph_dashboard_t *db, int ch) {
        switch (db->mode) {
        case DB_MODE_POPUP:   handle_key_popup(db, ch);   break;
        case DB_MODE_COMMAND: handle_key_command(db, ch);  break;
        case DB_MODE_SEARCH:  handle_key_search(db, ch);   break;
        case DB_MODE_FUZZY:   handle_key_fuzzy(db, ch);    break;
        case DB_MODE_NORMAL:  handle_key_normal(db, ch);   break;
        }
    }

keybindings
~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 40 20

   * - Key
     - Action
     - Mode
   * - ``q`` / ``Q``
     - Quit dashboard
     - NORMAL
   * - ``Tab``
     - Switch panel focus
     - NORMAL
   * - ``f``
     - Toggle fullscreen zoom on focused panel
     - NORMAL
   * - ``c``
     - Clear focused panel ring buffer
     - NORMAL
   * - ``/``
     - Enter search mode (pattern input)
     - NORMAL
   * - ``n``
     - Jump to next search match (upward/older)
     - NORMAL
   * - ``N``
     - Jump to previous match (downward/newer)
     - NORMAL
   * - ``Up`` / ``k``
     - Move cursor up (activates cursor on first press)
     - NORMAL
   * - ``Down`` / ``j``
     - Move cursor down (auto-follow when past end)
     - NORMAL
   * - ``PgUp``
     - Move cursor up one page
     - NORMAL
   * - ``PgDn``
     - Move cursor down one page
     - NORMAL
   * - ``Home``
     - Cursor to first line
     - NORMAL
   * - ``End``
     - Back to auto-follow (deactivates cursor)
     - NORMAL
   * - ``v``
     - Enter/exit visual select mode (anchor at cursor)
     - NORMAL
   * - ``V``
     - Export selection to JSON file
     - NORMAL
   * - ``g``
     - Open fuzzy log finder popup
     - NORMAL
   * - ``z``
     - Toggle JSON fold on cursor line
     - NORMAL
   * - ``:``
     - Enter command mode
     - NORMAL
   * - ``?``
     - Open help popup
     - NORMAL
   * - ``a``
     - Open about popup
     - NORMAL
   * - ``Ctrl-S``
     - Select Start button
     - NORMAL
   * - ``Ctrl-T``
     - Select Stop button
     - NORMAL
   * - ``Enter``
     - Activate selected button / confirm search
     - NORMAL/SEARCH
   * - ``Esc``
     - Clear selection + search / cancel input / close popup
     - ALL

start/stop buttons
~~~~~~~~~~~~~~~~~~

the dashboard renders Start and Stop buttons in the status bar when a
``serve_cfg`` is available (borrowed pointer from the caller):

- **Start**: green background, white text when actionable; grey when
  server is running (NOOP). ``Ctrl-S`` selects, ``Enter`` activates.
- **Stop**: red background, white text when actionable; grey when server
  is stopped (NOOP). ``Ctrl-T`` selects, ``Enter`` activates.

``Ctrl-S`` requires disabling terminal XON/XOFF flow control. this is
done in ``ph_dashboard_create()`` via ``tcsetattr()`` clearing ``IXON``
and ``IXOFF``. ``cbreak()`` (not ``raw()``) is used so that ``Ctrl-C``
still generates SIGINT for the signal handler.

process lifecycle
-----------------

the spawn/monitor/stop/restart flow:

1. **spawn**: ``ph_serve_start()`` forks children with ``setpgid()`` for
   process group isolation. pipes are created for stdout/stderr capture.
2. **monitor**: ``poll()`` watches all pipe fds. output flows into ring
   buffers via ``feed_accum()`` -> ``ringbuf_push()``.
3. **stop**: ``action_stop()`` sends ``kill(-(pid), SIGTERM)`` to each
   running child's process group. non-blocking -- reap happens
   asynchronously via ``DB_EVT_CHILD_EXIT``.
4. **reap**: ``reap_children()`` calls ``waitpid(WNOHANG)`` each cycle
   and pushes ``DB_EVT_CHILD_EXIT`` events.
5. **restart**: ``action_start()`` destroys the old session, calls
   ``ph_serve_start()`` to create a new one, then ``rewire_panels()``
   updates pids and fds without clearing ring buffers (log history is
   preserved across restarts).

auto-quit behavior: when ``serve_cfg`` is present, the dashboard stays
open after all children exit so the user can restart. without
``serve_cfg``, it auto-quits when all children exit and all fds close.

signal handling
---------------

three signals are handled:

- **SIGINT / SIGTERM**: set ``g_interrupted`` flag via ``signal_handler()``.
  the dashboard checks this each poll cycle and sets ``db->quit = true``.
- **SIGWINCH**: set ``g_winch`` flag via ``winch_handler()``. the dashboard
  calls ``endwin(); refresh(); layout_panels()`` to recalculate geometry.
  popup windows are recreated by ``draw_popup()`` on the next render.

the self-pipe pattern wakes ``poll()`` from signal handlers. both ends
of the pipe are non-blocking (``O_NONBLOCK``) to prevent the drain loop
from stalling the event loop. ``SA_RESTART`` is set on the SIGWINCH
handler to avoid EINTR on system calls other than ``poll()``.

concurrency model
-----------------

the dashboard is fully single-threaded. there are no mutexes, no
atomics (beyond ``sig_atomic_t`` in the signal handler), and no race
conditions. correctness relies on:

- ``poll()`` as the single blocking synchronization point
- non-blocking pipe reads (``O_NONBLOCK`` on signal pipe)
- ``WNOHANG`` on ``waitpid()`` for non-blocking child reaping
- signal flags checked unconditionally after ``poll()`` returns (even
  on EINTR) to avoid missing signals

the ring buffer (2000 lines per panel) is only accessed from the main
thread. the ANSI SGR color passthrough in ``render_line()`` and the UTF-8
preservation in ``clean_line()`` operate on stored line data, never on
raw pipe input.

zoom (fullscreen)
-----------------

pressing ``f`` toggles the focused panel to fill the entire panel area.
in zoom mode, only the focused panel's ``WINDOW`` is created; all others
are set to ``NULL`` and skipped by ``draw_all()``. the info box is also
hidden while zoomed.

``Tab`` while zoomed switches which panel is displayed fullscreen by
changing ``db->focused`` and calling ``layout_panels()``. pressing ``f``
again restores the normal side-by-side or stacked layout.

resize (SIGWINCH) while zoomed works naturally -- ``layout_panels()``
checks the ``zoomed`` flag on every call.

search
------

``/`` enters search mode (``DB_MODE_SEARCH``). the user types a plain-text
pattern; Enter confirms, Esc cancels.

once confirmed, ``search_active`` is set to ``true`` and the pattern is
stored in ``search_pat``. every visible line in ``draw_panel()`` is
checked with ``strstr()``; matching lines get a yellow highlight via
``mvwchgat()`` with ``CP_SEARCH_MATCH``.

``n`` jumps to the next match upward (older lines), ``N`` jumps downward
(newer lines). both adjust the panel's ``scroll`` offset. if no match is
found, "Pattern not found" is shown in the command message area.

``Esc`` in normal mode clears the active search (deactivates highlights
and resets ``search_pat``).

line cursor
-----------

arrow keys activate a visible cursor line in the focused panel. the cursor
starts invisible (``cursor = -1``, auto-follow mode). pressing ``Up/k``
activates it at the last visible line; subsequent presses move it upward.
``Down/j`` moves it forward; when it reaches the end of the ring buffer,
the cursor deactivates and the panel returns to auto-follow.

``PgUp``/``PgDn`` move the cursor by one page (content height). ``Home``
jumps to the first line; ``End`` returns to auto-follow (``cursor = -1``,
``scroll = 0``).

the cursor is rendered as a reverse-video bar (``CP_CURSOR_LINE``) via
``mvwchgat()``. it is only visible when no selection is active.

``Esc`` in normal mode clears the cursor, selection, and search.

visual select mode (v)
----------------------

pressing ``v`` enters vim-style visual select mode. the cursor position
becomes the ``sel_anchor``. subsequent ``j``/``k``/arrow movement extends
the selection as a contiguous range from anchor to cursor:
``min(sel_anchor, cursor)..max(sel_anchor, cursor)``.

if no cursor is active when ``v`` is pressed, the cursor activates at the
last visible line first. pressing ``v`` again exits select mode (clears
``sel_anchor``). ``Esc`` also cancels.

selected lines are highlighted in blue (``CP_SELECTED_LINE``) via
``mvwchgat()`` with ``A_BOLD``. the selection highlight overwrites the
cursor highlight for the selected range.

in select mode, arrow keys and ``j``/``k`` move the cursor (extending the
selection) rather than scrolling the viewport. ``PgUp``/``PgDn``/``Home``
also move the cursor.

viewport freeze
~~~~~~~~~~~~~~~

when the user has scrolled away from the bottom (``scroll > 0``), new
output arriving via ``handle_pipe_data()`` does not shift the viewport.
instead, the scroll offset is incremented by the number of new lines
added, keeping the visible content stable. this prevents the jarring
effect of new output jumping the view while reading older lines.

the cursor model uses ``cursor = -1`` for auto-follow mode and
``cursor >= 0`` for a specific ring buffer index. ``End`` returns to
auto-follow by resetting cursor, sel_anchor, and scroll to their
default states.

JSON export (V)
---------------

pressing ``V`` (Shift+V) exports the current selection to a JSON file
named ``phosphor.<panel_name>.json`` in the working directory.

the JSON file uses a daily-indexed structure::

    {
      "04042026": {
        "0": "first exported line",
        "1": "second line"
      }
    }

- date key format: ``DDMMYYYY`` (from ``localtime()``)
- line keys: sequential integers as strings, continuing from the highest
  existing key in today's entry (append semantics)
- the file is read-modify-write: existing content is parsed, new lines are
  merged into today's object, and the file is rewritten
- ANSI escape sequences are stripped from exported lines via ``strip_ansi()``
- if the existing file contains malformed JSON, it is treated as empty

the export requires ``PHOSPHOR_HAS_CJSON`` (cJSON vendored subproject).
without it, ``V`` shows a "cJSON not available" message.

after export, the selection is cleared and a status message shows the
number of lines exported and the filename.

save and clear
--------------

``:save <path>`` and ``:saveall`` use incremental JSON export with panel
clearing -- the same structured format as the ``V`` export.

``:save <path>`` (focused panel)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

saves the focused panel's ring buffer to a JSON file and clears the panel.

filename: ``DD.MM.YYYY.<panel_name>.<path>.json`` (e.g.
``04.04.2026.neonsignal.audit.txt.json`` for ``:save audit.txt``).

the file uses numbered save slots. each ``:save`` to the same path appends
a new slot::

    {
      "0": {
        "0": "[2026-04-04 08:59:25] first line",
        "1": "[2026-04-04 08:59:30] second line"
      },
      "1": {
        "0": "[2026-04-04 09:15:00] line after clear"
      }
    }

- slot keys: incrementing integers as strings (``"0"``, ``"1"``, ...)
- line keys: incrementing integers within each slot
- ANSI stripped via ``strip_ansi()``
- file is read-modify-write (existing slots preserved)
- the focused panel is cleared after saving (ring buffer destroyed,
  accumulators reset)

``:saveall`` (all panels)
~~~~~~~~~~~~~~~~~~~~~~~~~

saves all panels to a single JSON file and clears all panels.

filename: ``DD.MM.YYYY.all.json`` (e.g. ``04.04.2026.all.json``).

each save slot contains an object per panel::

    {
      "0": {
        "neonsignal": { "0": "line...", "1": "line..." },
        "redirect":   { "0": "line..." },
        "watcher":    {}
      }
    }

- same incrementing slot structure as ``:save``
- panels with no output since last clear get an empty object
- all panels are cleared after saving

``:clear``
~~~~~~~~~~

``:clear`` and the ``c`` keybinding destroy the focused panel's ring
buffer and reset accumulators. log output from running processes
continues flowing normally.

commands
--------

available dashboard commands (entered via ``:`` in command mode):

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Command
     - Description
   * - ``:start``
     - Start the server (NOOP if already running)
   * - ``:stop``
     - Stop the server (NOOP if already stopped)
   * - ``:clear``
     - Clear focused panel ring buffer
   * - ``:save <path>``
     - Save panel to JSON with incremental slots, clear panel
   * - ``:saveall``
     - Save all panels to JSON with incremental slots, clear all
   * - ``:filament``
     - (not yet implemented)

popups
------

popups are centered ncurses overlay windows rendered on top of all
panels. six types:

- **Help** (``?``): shows all keybindings in a 56x34 box. footer
  offers navigation: ``c`` opens commands popup, ``h`` opens phosphor
  help popup.
- **About** (``a``): shows phosphor version and description in a 46x10 box.
- **Commands** (``c`` from help): lists available dashboard ``:cmd``
  commands in a 50x15 box. ``?`` returns to help.
- **Phosphor Help** (``h`` from help): lists all ``phosphor`` CLI commands
  in a 56x20 box. ``?`` returns to help.
- **Fuzzy Log Finder** (``g``): popup overlay for searching ``.json`` log
  files on disk. file picker phase, then JSON viewer on Enter.
- **JSON Viewer** (Enter from fuzzy): full-screen popup with tree folding.

popup windows are recreated each draw call to respect terminal resize.
``close_popup()`` deletes the window and calls ``touchwin(stdscr)``
to force a full repaint that removes the popup shadow.

fuzzy log finder (g)
--------------------

pressing ``g`` scans the current working directory for ``.json`` files and
opens a fuzzy finder popup overlay. the panel output continues rendering
behind the popup.

**file picker phase** (``fuzzy_picking = true``):

- lists all ``.json`` files found in cwd
- typing filters the list with fuzzy matching (characters must appear in
  order, case-insensitive, with consecutive/boundary bonuses)
- ``Up``/``Down`` navigate the result list
- ``Enter`` opens the selected file in the JSON viewer popup
- ``Esc`` closes the fuzzy finder and returns to NORMAL mode
- ``Ctrl-U`` clears the search input

the popup title shows ``open>`` followed by the typed filter. the right
side shows match count (e.g. ``3/7 .json files``).

implementation: ``db_fuzzy.c`` -- ``fuzzy_scan_json_files()`` uses
``opendir``/``readdir`` to find files, ``fuzzy_score()`` computes match
quality, ``fuzzy_recompute()`` rebuilds the result list on each keystroke,
``draw_fuzzy_popup()`` renders the overlay with match highlighting.

JSON viewer popup
-----------------

the JSON viewer is a full-screen popup (``DB_POPUP_JSON_VIEWER``) that
displays a parsed JSON file as a foldable tree. it opens from the fuzzy
log finder when a file is selected.

**tree model**: the JSON file is parsed with cJSON and converted to a
flat array of ``db_json_node_t`` nodes via pre-order traversal. each
container (object/array) gets an opening node and a ``JN_CLOSE`` marker.
the ``subtree_end`` field enables efficient fold skipping.

**fold state**: all nodes start folded on load. folded containers display
as a single line::

    [+] "key": { ... }  (3)

unfolded containers show their children with indentation::

    [-] "key": {
        "name": "hello",
        "count": 42
    }

**syntax highlighting**: keys (cyan, ``CP_JSON_KEY``), strings (green,
``CP_JSON_STRING``), numbers (yellow, ``CP_JSON_NUMBER``), bools/null
(magenta, ``CP_JSON_BOOL``), brackets (bold). fold indicators ``[+]``/
``[-]`` use ``CP_FUZZY_PROMPT``.

**navigation**:

- ``j``/``k``/``Up``/``Down``: move cursor (skips folded subtrees)
- ``z``/``Enter``: toggle fold on cursor node
- ``l``/``Right``: unfold (if container is folded)
- ``h``/``Left``: fold (if container is unfolded)
- ``PgUp``/``PgDn``: page movement
- ``Home``/``End``: top/bottom
- ``Esc``: back to fuzzy file picker
- ``q``: close everything, return to NORMAL mode

the footer shows the cursor position (e.g. ``3/47``) and key hints.

implementation: ``db_json_fold.c`` -- ``open_json_viewer()`` parses and
builds the node tree, ``draw_json_viewer()`` renders with scroll/cursor,
``handle_json_viewer_key()`` handles navigation and fold toggling,
``close_json_viewer()`` frees nodes and returns to fuzzy mode.

inline JSON fold (z)
--------------------

pressing ``z`` in NORMAL mode with a cursor active parses JSON from the
cursor line and expands it inline within the panel. if JSON is found
(bracket-matched extraction with string/escape awareness), it is
pretty-printed with cJSON and rendered with syntax highlighting in place
of the original line.

pressing ``z`` again on the same line collapses the fold. only one fold
per panel is active at a time.

this is distinct from the JSON viewer popup: inline fold works on
individual log lines in the panel, while the viewer opens entire JSON
files in a popup.

panel tabs
----------

panels can optionally have tabs that split stdout and stderr into
separate views. the neonsignal panel uses two tabs:

- **live-stream** (tab 1): receives ``stdout`` -- request/response access logs
- **debug-stream** (tab 2): receives ``stderr`` -- debug/protocol logs

tabs are configured at panel creation via ``ph_dashboard_tab_cfg_t``.
each tab owns its own ring buffer, scroll, cursor, selection, and JSON
fold state. the ``panel_ring()``, ``panel_scroll()``, ``panel_cursor()``
accessor inlines transparently resolve to the active tab's fields (or
the panel's inline fields when ``tab_count == 0``).

tab indicators are rendered in the panel title bar after the panel name.
the active tab is shown in bold; inactive tabs are dim. switching tabs:
``1`` through ``4`` in normal mode (when the focused panel has tabs).

panels without tabs (redirect, watcher) behave identically to before.

the pipe routing in ``handle_pipe_data()`` uses ``feed_accum_multi()``
to push completed lines to all tabs whose ``source`` matches the
``is_stderr`` flag.

embedded shell
--------------

the dashboard includes an embedded terminal panel between the process
panels and the status bar. it provides interactive command execution
without leaving the TUI.

terminology:

- **shell**: resizable panel region at the bottom, contains views (tabs)
- **view**: a tab inside the shell with its own input line and screen list
- **screen**: popup overlay showing one command's output

**keybindings**:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key
     - Action
   * - ``Ctrl+P``
     - Toggle shell open / open new view tab
   * - ``Ctrl+Q``
     - Close shell entirely (kill all processes)
   * - ``Ctrl+D``
     - Open phosphor command bar from shell mode
   * - ``Ctrl+X``
     - Minimize focused screen overlay
   * - ``Ctrl+S``
     - Save screen output to ``shell/[date].command.txt``
   * - ``1``-``9``
     - Open screen by number
   * - ``Esc``
     - Minimize all screens, return focus to panels

**behavior**: each view has a ``$`` input line. typing a command and
pressing Enter spawns the command via PTY using ``$SHELL -c "cmd"``.
output appears in a screen popup overlay. blocking commands disable the
view's input until the process exits. multiple screens per view are
navigable by number keys when a screen is visible.

screens can be minimized (hidden but still running) with ``Ctrl+M``.
``Ctrl+S`` saves the focused screen's output to a plain text file in
``cwd/shell/``.

**PTY spawning**: uses ``posix_openpt()`` / ``grantpt()`` /
``unlockpt()`` / ``ptsname()`` (all available under
``_POSIX_C_SOURCE=200809L``). child process uses ``setsid()`` +
``open(slave)`` + ``dup2()`` to attach the PTY. no ``login_tty()``
dependency.

**layout**: when the shell is open, ``layout_panels()`` subtracts
``shell_height`` from the available panel rows. the shell window sits
between the bottom panel and the status bar.

**event loop**: shell PTY master fds are added to the ``poll()`` array.
``DB_EVT_SHELL_DATA`` and ``DB_EVT_SHELL_EOF`` events feed into screen
ring buffers via the existing ``feed_accum()`` pipeline.

implementation: ``db_shell.c`` -- all shell lifecycle, PTY spawning,
key handling, and drawing in a single file.

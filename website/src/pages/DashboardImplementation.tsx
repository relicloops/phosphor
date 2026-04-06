import { css } from '@relicloops/cathode';

import { BackToTop } from '../components/BackToTop';
import { Footer } from '../components/Footer';
import { Header } from '../components/Header';
import { initScrollTracker } from '../scripts/scroll-tracker';

interface Section {
  id: string;
  title: string;
  body: () => HTMLElement;
}

function p( text: string ): HTMLElement {
  const el = document.createElement( 'p' );
  el.className = 'dash-arch__text';
  el.textContent = text;
  return el;
}

function pre( text: string ): HTMLElement {
  const el = document.createElement( 'pre' );
  el.className = 'dash-arch__pre';
  el.textContent = text;
  return el;
}

function table( headers: string[], rows: string[][] ): HTMLElement {
  const tbl = document.createElement( 'table' );
  tbl.className = 'dash-arch__table';

  const thead = document.createElement( 'thead' );
  const headRow = document.createElement( 'tr' );
  headers.forEach( h => {
    const th = document.createElement( 'th' );
    th.textContent = h;
    headRow.appendChild( th );
  } );
  thead.appendChild( headRow );
  tbl.appendChild( thead );

  const tbody = document.createElement( 'tbody' );
  rows.forEach( row => {
    const tr = document.createElement( 'tr' );
    row.forEach( ( cell, i ) => {
      const td = document.createElement( 'td' );
      if ( i === 0 ) {
        const code = document.createElement( 'code' );
        code.textContent = cell;
        td.appendChild( code );
      } else {
        td.textContent = cell;
      }
      tr.appendChild( td );
    } );
    tbody.appendChild( tr );
  } );
  tbl.appendChild( tbody );

  return tbl;
}

function diagram( text: string ): HTMLElement {
  const el = document.createElement( 'pre' );
  el.className = 'dash-arch__diagram';
  el.textContent = text;
  return el;
}

function frag( ...children: HTMLElement[] ): HTMLElement {
  const div = document.createElement( 'div' );
  children.forEach( c => div.appendChild( c ) );
  return div;
}

const sections: Section[] = [
  {
    id: 'overview',
    title: 'Overview',
    body: () => frag(
      p( 'The phosphor serve dashboard is an ncurses TUI that monitors child processes (neonsignal, neonsignal_redirect, file watcher) in real time. It was refactored from a monolithic 960-line file into 16 files with maximum separation of concerns: one file per subsystem, one file per event type.' ),
      p( 'This page documents the internal architecture: how events are collected, dispatched, and rendered, and how the UI state machine controls keyboard behavior.' ),
    ),
  },
  {
    id: 'file-layout',
    title: 'File Layout',
    body: () => frag(
      p( 'Every db_*.c file has a single responsibility. All share the internal types header db_types.h.' ),
      table(
        [ 'File', 'Responsibility' ],
        [
          [ 'db_types.h', 'All internal types, enums, structs, cross-file declarations' ],
          [ 'db_ring.c', 'Ring buffer, line accumulator, line cleaner, UTF-8 helper, ANSI stripper' ],
          [ 'db_layout.c', 'layout_panels() -- window geometry calculation' ],
          [ 'db_draw.c', 'All rendering: info box, panels, status bar, buttons, ANSI SGR' ],
          [ 'db_popup.c', 'Popup open/close/draw, help/about/commands/phosphor-help content' ],
          [ 'db_fuzzy.c', 'Fuzzy log finder: file scanning, scoring, popup rendering, file picker' ],
          [ 'db_json_fold.c', 'JSON viewer popup: tree builder, fold/unfold, syntax highlighting. Inline JSON fold' ],
          [ 'db_evt_pipe.c', 'handle_pipe_data(), handle_pipe_eof()' ],
          [ 'db_evt_signal.c', 'handle_signal() (SIGINT/SIGTERM), handle_winch() (resize)' ],
          [ 'db_evt_child.c', 'handle_child_exit(), reap_children()' ],
          [ 'db_evt_key.c', 'handle_key() mode dispatch, normal/command/popup/search/select/fuzzy handlers' ],
          [ 'db_evt_tick.c', 'handle_tick() -- message frame countdown, button flash' ],
          [ 'db_event.c', 'collect_events(), handle_event() switch dispatch' ],
          [ 'db_lifecycle.c', 'action_start(), action_stop(), action_clear(), action_save(), action_saveall(), action_export_selection(), rewire_panels(), shutdown_children()' ],
          [ 'db_shell.c', 'Embedded shell panel: PTY spawning, view/screen lifecycle, key handler, drawing (shell panel + screen overlays)' ],
          [ 'dashboard.c', 'Thin glue: ph_dashboard_create(), ph_dashboard_run(), ph_dashboard_destroy()' ],
        ],
      ),
    ),
  },
  {
    id: 'event-loop',
    title: 'Collect / Dispatch / Render Cycle',
    body: () => frag(
      p( 'The event loop in ph_dashboard_run() follows a three-phase cycle:' ),
      pre( 'while (!db->quit) {\n    1. collect_events()   -- poll + signals + keyboard + reap\n    2. handle_event()     -- dispatch each event to its handler\n    3. draw_all()         -- re-render everything to the screen\n}' ),
      p( 'This is a purely single-threaded design. All I/O is non-blocking. The poll() call is the only blocking point, with a 100ms timeout that doubles as the tick rate for animated elements (message countdowns, button flash).' ),
    ),
  },
  {
    id: 'event-types',
    title: 'Event Types',
    body: () => frag(
      p( 'Events are represented as a tagged union allocated on the stack:' ),
      pre( 'typedef struct {\n    db_evt_type_t type;\n    union {\n        db_evt_pipe_t  pipe;   -- panel_idx, is_stderr, buf[], len\n        db_evt_child_t child;  -- panel_idx, exit_code\n        db_evt_key_t   key;    -- ch (keypress code)\n    } d;\n} db_event_t;' ),
      table(
        [ 'Event', 'Source', 'Handler' ],
        [
          [ 'DB_EVT_PIPE_DATA', 'poll() POLLIN on panel fd', 'db_evt_pipe.c' ],
          [ 'DB_EVT_PIPE_EOF', 'read() returns 0', 'db_evt_pipe.c' ],
          [ 'DB_EVT_CHILD_EXIT', 'waitpid() WNOHANG reap', 'db_evt_child.c' ],
          [ 'DB_EVT_KEY', 'getch() nodelay mode', 'db_evt_key.c' ],
          [ 'DB_EVT_SIGNAL', 'ph_signal_interrupted() flag', 'db_evt_signal.c' ],
          [ 'DB_EVT_WINCH', 'ph_signal_winch_pending() flag', 'db_evt_signal.c' ],
          [ 'DB_EVT_TICK', 'No other events in poll cycle', 'db_evt_tick.c' ],
        ],
      ),
      p( 'The event queue is a fixed-size db_event_t events[MAX_EVENTS] array on the stack. collect_events() fills it; no heap allocation occurs in the hot path.' ),
    ),
  },
  {
    id: 'event-collection',
    title: 'Event Collection',
    body: () => frag(
      p( 'collect_events() in db_event.c follows this sequence:' ),
      pre( '1. Build pollfd array from all open panel pipe fds + signal pipe\n2. poll(fds, nfds, POLL_TIMEOUT_MS)\n3. Drain signal pipe on POLLIN or EINTR (non-blocking read loop)\n4. Check ph_signal_interrupted() -> push DB_EVT_SIGNAL\n5. Check ph_signal_winch_pending() -> push DB_EVT_WINCH\n6. Read each readable pipe fd -> push DB_EVT_PIPE_DATA or DB_EVT_PIPE_EOF\n7. reap_children() -> push DB_EVT_CHILD_EXIT for each reaped child\n8. getch() loop -> push DB_EVT_KEY for each keypress\n9. If nevents == 0 -> push DB_EVT_TICK' ),
    ),
  },
  {
    id: 'event-dispatch',
    title: 'Event Dispatch',
    body: () => frag(
      p( 'handle_event() is a simple switch:' ),
      pre( 'void handle_event(ph_dashboard_t *db, const db_event_t *evt) {\n    switch (evt->type) {\n    case DB_EVT_SIGNAL:     handle_signal(db);                    break;\n    case DB_EVT_WINCH:      handle_winch(db);                     break;\n    case DB_EVT_PIPE_DATA:  handle_pipe_data(db, &evt->d.pipe);   break;\n    case DB_EVT_PIPE_EOF:   handle_pipe_eof(db, &evt->d.pipe);    break;\n    case DB_EVT_CHILD_EXIT: handle_child_exit(db, &evt->d.child); break;\n    case DB_EVT_KEY:        handle_key(db, evt->d.key.ch);        break;\n    case DB_EVT_TICK:       handle_tick(db);                       break;\n    case DB_EVT_NONE:       break;\n    }\n}' ),
    ),
  },
  {
    id: 'ui-modes',
    title: 'UI Mode State Machine',
    body: () => frag(
      p( 'The dashboard has six input modes that control how keyboard events are interpreted:' ),
      diagram( '                     "g"                        ":"\nDB_MODE_NORMAL  ---------> DB_MODE_FUZZY    DB_MODE_COMMAND\n     |        <---Esc----     |          <--Esc--  |\n     |                        |  Enter             Enter\n     |  "?" or "a"            v                    |\nDB_MODE_POPUP  <---- DB_POPUP_JSON_VIEWER  --------+\n     |                                             |\n     +--Esc/q/Enter-------> DB_MODE_NORMAL <-------+\n                                |\n     DB_MODE_SEARCH  <--"/"--+  |\n          |                     |\n          +--Enter/Esc-------> (back)' ),
      p( 'NORMAL: navigation, scrolling, zoom ("f"), clear ("c"), select ("v"), search jumps ("n"/"N"), fold ("z"), fuzzy ("g"), button selection (Ctrl-S/T), popup triggers ("?", "a"), quit ("q").' ),
      p( 'COMMAND: vi-style command line (":start", ":stop", ":save", ":saveall", ":clear"). Esc cancels, Enter executes.' ),
      p( 'SEARCH: pattern input for log search. "/" enters from NORMAL. Enter confirms (activates highlighting + n/N navigation). Esc cancels.' ),
      p( 'POPUP: modal overlay. Help popup navigates to commands ("c") or phosphor help ("h"); sub-popups return to help with "?". Esc/Enter/q closes any popup. JSON viewer has its own key handler.' ),
      p( 'FUZZY: fuzzy log finder. Type to filter, Up/Down to navigate, Enter to open in JSON viewer, Esc/q to close.' ),
      p( 'SHELL: embedded terminal panel has focus. Input goes to view command line, screens are navigable. Esc minimizes all screens and returns to NORMAL. Alt+F11 enters from any mode.' ),
    ),
  },
  {
    id: 'process-lifecycle',
    title: 'Process Lifecycle',
    body: () => frag(
      p( 'The spawn/monitor/stop/restart flow:' ),
      pre( '1. Spawn:   ph_serve_start() forks children with setpgid()\n            for process group isolation. Pipes capture stdout/stderr.\n2. Monitor: poll() watches all pipe fds. Output flows into ring\n            buffers via feed_accum() -> ringbuf_push().\n3. Stop:    action_stop() sends kill(-(pid), SIGTERM) to each\n            running child\'s process group. Non-blocking.\n4. Reap:    reap_children() calls waitpid(WNOHANG) each cycle\n            and pushes DB_EVT_CHILD_EXIT events.\n5. Restart: action_start() destroys old session, starts new one,\n            rewire_panels() updates pids/fds. Ring buffers preserved.' ),
      p( 'Auto-quit behavior: when serve_cfg is present, the dashboard stays open after all children exit so the user can restart. Without serve_cfg, it auto-quits when all children exit and all fds close.' ),
    ),
  },
  {
    id: 'signals',
    title: 'Signal Handling',
    body: () => frag(
      p( 'Three signals are handled:' ),
      p( 'SIGINT / SIGTERM: set g_interrupted flag. The dashboard checks this each poll cycle and sets db->quit = true.' ),
      p( 'SIGWINCH: set g_winch flag. The dashboard calls endwin(); refresh(); layout_panels() to recalculate geometry. Popup windows are recreated on next render.' ),
      p( 'The self-pipe pattern wakes poll() from signal handlers. Both ends of the pipe are O_NONBLOCK to prevent the drain loop from stalling. SA_RESTART is set on SIGWINCH to avoid EINTR on system calls other than poll().' ),
    ),
  },
  {
    id: 'concurrency',
    title: 'Concurrency Model',
    body: () => frag(
      p( 'The dashboard is fully single-threaded. No mutexes, no atomics (beyond sig_atomic_t in signal handlers), no race conditions. Correctness relies on:' ),
      pre( '- poll() as the single blocking synchronization point\n- Non-blocking pipe reads (O_NONBLOCK on signal pipe)\n- WNOHANG on waitpid() for non-blocking child reaping\n- Signal flags checked unconditionally after poll()\n  (even on EINTR) to avoid missing signals' ),
      p( 'The ring buffer (2000 lines per panel) is only accessed from the main thread. ANSI SGR color passthrough and UTF-8 preservation operate on stored line data, never on raw pipe input.' ),
    ),
  },
  {
    id: 'ring-buffer',
    title: 'Ring Buffer',
    body: () => frag(
      p( 'Each panel has a ring buffer of 2000 lines. Lines are stored as db_line_t structs with text, length, and flags (is_stderr, color). When the buffer is full, the oldest line is overwritten.' ),
      p( 'The line accumulator (feed_accum) collects partial reads until a newline is found, then pushes complete lines. UTF-8 multibyte sequences are preserved across partial reads. ANSI SGR color codes are stored with the line and replayed during rendering.' ),
    ),
  },
  {
    id: 'json-fold-impl',
    title: 'JSON Fold Implementation',
    body: () => frag(
      p( 'The JSON viewer uses a flat db_json_node_t array built by pre-order traversal of a cJSON tree. Each node stores:' ),
      pre( 'typedef struct {\n    db_jn_type_t type;    /* object, array, string, number, bool, null */\n    char        *key;    /* key name (NULL for array elements) */\n    char        *value;  /* value string (NULL for containers) */\n    int          depth;  /* nesting depth for indentation */\n    int          subtree_end;  /* index past last child (for fold skip) */\n    bool         folded; /* true = children hidden */\n} db_json_node_t;' ),
      p( 'Visible node iteration uses jv_count_visible() and jv_vis_to_raw() -- both compute on-the-fly by scanning the flat array and skipping folded subtrees via subtree_end. No stack arrays or auxiliary data structures needed.' ),
      p( 'All containers start folded. Toggle operations flip the folded flag. The cursor tracks the visible node index; scroll tracks the viewport offset within visible nodes.' ),
    ),
  },
  {
    id: 'viewport-impl',
    title: 'Viewport Freeze Implementation',
    body: () => frag(
      p( 'When scroll > 0 (the user has scrolled away from the bottom), new pipe data triggers a scroll offset adjustment in handle_pipe_data():' ),
      pre( 'if (p->scroll > 0)\n    p->scroll++;  /* keep view stable while new lines arrive */' ),
      p( 'This increment offsets the new line, keeping the visible window at the same position in the ring buffer. When the user returns to auto-follow (End key, scroll = 0), the latest output becomes visible again.' ),
    ),
  },
];

export const DashboardImplementation = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/pages/dashboard-architecture.css' );

  if ( typeof document !== 'undefined' ) {
    setTimeout( () => {
      const contentEl = document.querySelector( '.dash-arch__content' );
      const tocList = document.querySelector( '.dash-arch__toc-list' );

      if ( ! contentEl || ! tocList ) {
        return;
      }

      /* render ToC */
      tocList.innerHTML = '';
      sections.forEach( section => {
        const li = document.createElement( 'li' );
        li.className = 'dash-arch__toc-item';
        const a = document.createElement( 'a' );
        a.className = 'dash-arch__toc-link';
        a.href = `#${section.id}`;
        a.textContent = section.title;
        li.appendChild( a );
        tocList.appendChild( li );
      } );

      /* render content sections */
      contentEl.innerHTML = '';
      sections.forEach( section => {
        const sectionEl = document.createElement( 'section' );
        sectionEl.className = 'dash-arch__section';
        sectionEl.id = section.id;

        const titleEl = document.createElement( 'h2' );
        titleEl.className = 'dash-arch__section-title';
        titleEl.textContent = section.title;
        sectionEl.appendChild( titleEl );

        const bodyEl = section.body();
        sectionEl.appendChild( bodyEl );

        contentEl.appendChild( sectionEl );
      } );

      initScrollTracker( {
        linkSelector: '.dash-arch__toc-link',
        sectionSelector: '.dash-arch__section',
        activeClass: 'dash-arch__toc-item--active',
      } );
    }, 0 );
  }

  return (
    <div>
      <Header />
      <main class="dash-arch">
        <aside class="dash-arch__toc">
          <h3 class="dash-arch__toc-title">Sections</h3>
          <ul class="dash-arch__toc-list"></ul>
        </aside>
        <div class="dash-arch__content">
          <h1 class="dash-arch__title">Dashboard Implementation</h1>
          <p class="dash-arch__text">Loading...</p>
        </div>
      </main>
      <BackToTop />
      <Footer />
    </div>
  );
};

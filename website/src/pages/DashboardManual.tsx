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
  el.className = 'dash-man__text';
  el.textContent = text;
  return el;
}

function pre( text: string ): HTMLElement {
  const el = document.createElement( 'pre' );
  el.className = 'dash-man__pre';
  el.textContent = text;
  return el;
}

function table( headers: string[], rows: string[][] ): HTMLElement {
  const tbl = document.createElement( 'table' );
  tbl.className = 'dash-man__table';

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
      p( 'The phosphor serve dashboard is an ncurses TUI that captures stdout and stderr from child processes (neonsignal, neonsignal_redirect, file watcher) into scrollable, color-coded panels. It runs inside phosphor serve and provides real-time log monitoring, search, selection, export, and process control.' ),
      p( 'Launch the dashboard with phosphor serve from a project directory. The dashboard starts automatically unless --no-dashboard is passed. All keybindings and commands are available immediately.' ),
    ),
  },
  {
    id: 'modes',
    title: 'Modes',
    body: () => frag(
      p( 'The dashboard has five input modes. The current mode controls how keyboard input is interpreted.' ),
      table(
        [ 'Mode', 'Enter', 'Exit', 'Purpose' ],
        [
          [ 'Normal', 'Default / Esc', '--', 'Navigation, scrolling, keybindings' ],
          [ 'Command', ':  (colon)', 'Enter / Esc', 'Type and execute dashboard commands' ],
          [ 'Search', '/  (slash)', 'Enter / Esc', 'Type a search pattern for log filtering' ],
          [ 'Popup', '? a or g->Enter', 'Esc / q', 'Modal overlay windows (help, about, etc.)' ],
          [ 'Fuzzy', 'g', 'Esc / q', 'Fuzzy search for JSON log files' ],
          [ 'Shell', 'Ctrl+P', 'Esc / Ctrl+Q', 'Embedded terminal input and screens' ],
        ],
      ),
      p( 'Esc always returns to Normal mode. In Select sub-mode (within Normal), arrow keys extend the selection instead of scrolling.' ),
    ),
  },
  {
    id: 'navigation',
    title: 'Navigation',
    body: () => frag(
      p( 'Panels display child process output. Tab cycles focus between panels. The focused panel has a highlighted border and receives all keyboard input.' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ 'Tab', 'Switch focus to the next panel' ],
          [ 'f', 'Toggle fullscreen zoom on focused panel' ],
          [ 'Up / k', 'Activate cursor, move up one line' ],
          [ 'Down / j', 'Move cursor down one line' ],
          [ 'PgUp', 'Move cursor up one page' ],
          [ 'PgDn', 'Move cursor down one page' ],
          [ 'Home', 'Jump cursor to first line in buffer' ],
          [ 'End', 'Return to auto-follow (deactivate cursor)' ],
        ],
      ),
      p( 'The cursor starts invisible (auto-follow mode). Pressing Up or Down activates it at the last visible line. When the cursor is active, new output no longer scrolls the view -- the viewport is frozen. Press End or Esc to return to auto-follow.' ),
    ),
  },
  {
    id: 'cursor',
    title: 'Cursor and Auto-Follow',
    body: () => frag(
      p( 'The cursor is a reverse-video highlighted line in the focused panel. It has two states:' ),
      p( 'Auto-follow (cursor = -1): the panel scrolls to show the newest output. This is the default state.' ),
      p( 'Active (cursor >= 0): the cursor is visible at a specific ring buffer index. The viewport freezes: new output increments the internal scroll offset so the view stays stable while data continues arriving.' ),
      p( 'Arrow keys, PgUp, PgDn, and Home activate the cursor on first press. End deactivates it (returns to auto-follow). Esc also deactivates cursor, clears selection, and clears search.' ),
    ),
  },
  {
    id: 'selection',
    title: 'Visual Selection',
    body: () => frag(
      p( 'Visual selection highlights a contiguous range of lines for export.' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ 'v', 'Toggle visual select mode (anchor at cursor position)' ],
          [ 'Shift+Up', 'Enter selection + extend upward' ],
          [ 'Shift+Down', 'Enter selection + extend downward' ],
          [ 'V', 'Export selected lines to JSON file' ],
          [ 'Esc', 'Cancel selection (and deactivate cursor)' ],
        ],
      ),
      p( 'Press v to start selecting. The anchor is set at the current cursor position (or last visible line if no cursor). Arrow keys then extend the selection range. The selection is always the range between anchor and cursor, highlighted in blue.' ),
      p( 'Shift+Up and Shift+Down also enter selection mode and immediately extend. These require terminal support for shift+arrow escape sequences.' ),
      p( 'V (capital V / Shift+V) exports the selection to phosphor.<panel>.json with a daily-indexed format. ANSI escape codes are stripped from exported text. After export, the selection clears.' ),
    ),
  },
  {
    id: 'search',
    title: 'Search',
    body: () => frag(
      p( 'Press / to enter search mode. Type a plain-text pattern and press Enter to confirm. Matching text is highlighted in yellow across all visible lines in the focused panel.' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ '/', 'Enter search mode' ],
          [ 'Enter', 'Confirm pattern (activate highlighting)' ],
          [ 'Esc', 'Cancel input (keep previous pattern) / Clear active search' ],
          [ 'n', 'Jump to next match (upward / older lines)' ],
          [ 'N', 'Jump to previous match (downward / newer lines)' ],
        ],
      ),
      p( 'After confirming a search, n and N jump between matches by adjusting the scroll offset. "Pattern not found" appears in the status bar if no match exists in the visible range.' ),
      p( 'Pressing Esc in normal mode clears the active search and removes all highlights.' ),
    ),
  },
  {
    id: 'commands',
    title: 'Commands',
    body: () => frag(
      p( 'Press : to enter command mode. A vi-style command line appears in the status bar. Type the command and press Enter to execute.' ),
      table(
        [ 'Command', 'Description' ],
        [
          [ ':start', 'Start the server processes. NOOP if already running.' ],
          [ ':stop', 'Stop all server processes (SIGTERM to process groups). NOOP if already stopped.' ],
          [ ':clear', 'Clear the focused panel ring buffer and reset cursor/selection.' ],
          [ ':save <path>', 'Save focused panel to DD.MM.YYYY.<panel>.<path>.json with incremental numbered slots. Clears the panel after saving.' ],
          [ ':saveall', 'Save all panels to DD.MM.YYYY.all.json with panel-keyed sub-objects per slot. Clears all panels after saving.' ],
          [ ':filament', 'Reserved for future filament package management.' ],
        ],
      ),
      p( 'Esc cancels the command without executing. Backspace deletes characters. Unknown commands show an error message.' ),
    ),
  },
  {
    id: 'save',
    title: 'Save and Export',
    body: () => frag(
      p( 'There are three ways to save log data from the dashboard:' ),
      p( 'V (selection export): Saves selected lines to phosphor.<panel>.json. Daily-indexed with a DDMMYYYY date key. Lines are numbered sequentially. Appends to existing file content. ANSI stripped.' ),
      p( ':save <path>: Saves the entire focused panel ring buffer. Filename format is DD.MM.YYYY.<panel>.<path>.json. The file uses incrementing numbered slots ("0", "1", "2", ...) where each slot contains the full panel contents as numbered line keys. Multiple saves to the same path add new slots. The panel is cleared after each save.' ),
      p( ':saveall: Saves all panels to DD.MM.YYYY.all.json. Same slot structure but each slot contains sub-objects keyed by panel name. All panels are cleared after saving.' ),
      pre( '// :save audit.txt -> 04.04.2026.neonsignal.audit.txt.json\n{\n  "0": {\n    "0": "first line from first save",\n    "1": "second line"\n  },\n  "1": {\n    "0": "first line from second save"\n  }\n}\n\n// :saveall -> 04.04.2026.all.json\n{\n  "0": {\n    "neonsignal": { "0": "line...", "1": "line..." },\n    "redirect":   { "0": "line..." },\n    "watcher":    { "0": "line..." }\n  }\n}' ),
    ),
  },
  {
    id: 'fuzzy',
    title: 'Fuzzy Log Finder',
    body: () => frag(
      p( 'Press g to open the fuzzy log finder. It scans the current working directory for .json files and presents a searchable file list as a popup overlay.' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ 'g', 'Open fuzzy finder (from Normal mode)' ],
          [ 'Type', 'Filter file list with fuzzy matching' ],
          [ 'Up / Down', 'Navigate file list' ],
          [ 'Enter', 'Open selected file in JSON viewer' ],
          [ 'Esc', 'Close fuzzy finder, return to Normal' ],
          [ 'q', 'Close fuzzy finder, return to Normal' ],
        ],
      ),
      p( 'Fuzzy matching scores characters that appear in order. Consecutive character matches and word boundary matches receive bonus scores. The best-matching files appear at the top of the list.' ),
      p( 'Selecting a file with Enter opens it in the JSON viewer popup.' ),
    ),
  },
  {
    id: 'json-viewer',
    title: 'JSON Viewer',
    body: () => frag(
      p( 'The JSON viewer is a full-screen popup that displays JSON file contents as a navigable tree with fold/unfold support. It opens from the fuzzy finder when you select a file.' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ 'z / Enter', 'Toggle fold on current node' ],
          [ 'l / Right', 'Unfold current node' ],
          [ 'h / Left', 'Fold current node' ],
          [ 'j / k / Up / Down', 'Navigate between visible nodes' ],
          [ 'PgUp / PgDn', 'Jump one page up or down' ],
          [ 'Home / End', 'Jump to first or last visible node' ],
          [ 'Esc', 'Close viewer, return to fuzzy finder' ],
          [ 'q', 'Close everything (viewer + fuzzy), return to Normal' ],
        ],
      ),
      p( 'All container nodes (objects and arrays) start folded. Folded containers show a [+] indicator; unfolded show [-]. The tree uses indentation to show nesting depth.' ),
      p( 'Syntax highlighting: keys are cyan, strings are green, numbers are yellow, booleans are magenta, null is dim.' ),
    ),
  },
  {
    id: 'inline-fold',
    title: 'Inline JSON Fold',
    body: () => frag(
      p( 'Press z in Normal mode (with the cursor on a line containing JSON) to parse and pretty-print the JSON inline within the panel. The folded view replaces the original line with a syntax-highlighted, indented representation.' ),
      p( 'Press z again on the same line to collapse the fold and restore the original content. This is useful for inspecting JSON payloads embedded in log lines without leaving the panel view.' ),
    ),
  },
  {
    id: 'popups',
    title: 'Popups',
    body: () => frag(
      p( 'Popups are centered overlay windows rendered on top of all panels. Six popup types are available:' ),
      table(
        [ 'Popup', 'Open With', 'Description' ],
        [
          [ 'Help', '?', 'All keybindings. Footer: c for commands, h for phosphor help.' ],
          [ 'About', 'a', 'Version and project description.' ],
          [ 'Commands', 'c (from Help)', 'List of :cmd dashboard commands.' ],
          [ 'Phosphor Help', 'h (from Help)', 'All phosphor CLI commands.' ],
          [ 'Fuzzy Finder', 'g', 'JSON file search and selection.' ],
          [ 'JSON Viewer', 'Enter (from Fuzzy)', 'Tree browser for JSON files.' ],
        ],
      ),
      p( 'Help, About, Commands, and Phosphor Help close with Esc, Enter, or q. From Commands and Phosphor Help, ? returns to the Help popup.' ),
      p( 'Popup windows are recreated each frame to respect terminal resize.' ),
    ),
  },
  {
    id: 'buttons',
    title: 'Start / Stop Buttons',
    body: () => frag(
      p( 'When a serve configuration is available, the status bar shows Start and Stop buttons.' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ 'Ctrl-S', 'Select the Start button' ],
          [ 'Ctrl-T', 'Select the Stop button' ],
          [ 'Enter', 'Activate the selected button' ],
        ],
      ),
      p( 'Start is green when actionable (server stopped) and grey when the server is already running. Stop is red when actionable and grey when already stopped. Ctrl-S requires terminal XON/XOFF flow control to be disabled, which the dashboard handles automatically.' ),
    ),
  },
  {
    id: 'zoom',
    title: 'Zoom (Fullscreen)',
    body: () => frag(
      p( 'Press f to toggle fullscreen zoom on the focused panel. In zoom mode, only the focused panel is visible -- all others are hidden, along with the info box.' ),
      p( 'Tab while zoomed switches which panel is displayed fullscreen. Press f again to return to the normal multi-panel layout. Terminal resize works normally while zoomed.' ),
    ),
  },
  {
    id: 'viewport',
    title: 'Viewport Freeze',
    body: () => frag(
      p( 'When the cursor is active or the panel is scrolled away from the bottom, the viewport freezes. New output from child processes continues to fill the ring buffer, but the scroll offset is automatically adjusted so the visible content does not shift.' ),
      p( 'This allows stable reading of log output while new data arrives. Press End or Esc to return to auto-follow and see the latest output.' ),
    ),
  },
  {
    id: 'status-bar',
    title: 'Status Bar',
    body: () => frag(
      p( 'The status bar at the bottom of the screen shows contextual information based on the current mode:' ),
      table(
        [ 'Context', 'Status Bar Content' ],
        [
          [ 'Normal', 'Panel name, process info -- hints: q:quit  v:select  g:fuzzy-log  ?:help  :cmd' ],
          [ 'Search active', 'Hints: v:select  n/N:search  ?:help  :cmd' ],
          [ 'Zoomed', 'Hints: v:select  f:unzoom  /:search  ?:help  :cmd' ],
          [ 'Select mode', 'Hints: V:export  Esc:cancel  j/k:extend  ?:help' ],
          [ 'Command mode', 'Shows :  followed by typed command text' ],
          [ 'Search mode', 'Shows /  followed by typed search pattern' ],
          [ 'Fuzzy mode', 'Fuzzy finder search + results in popup' ],
          [ 'JSON viewer', 'Hints: z:fold  l/h:open/close  j/k:navigate  Esc:back  q:close' ],
        ],
      ),
      p( 'The status bar also shows transient messages (command results, error messages) that auto-dismiss after a countdown.' ),
    ),
  },
  {
    id: 'keybindings',
    title: 'All Keybindings',
    body: () => frag(
      p( 'Complete reference of all keybindings across all modes.' ),
      table(
        [ 'Key', 'Mode', 'Action' ],
        [
          [ 'q / Q', 'Normal', 'Quit dashboard' ],
          [ 'Tab', 'Normal', 'Switch panel focus' ],
          [ 'f', 'Normal', 'Toggle fullscreen zoom' ],
          [ 'c', 'Normal', 'Clear focused panel' ],
          [ '/', 'Normal', 'Enter search mode' ],
          [ 'n', 'Normal', 'Jump to next search match' ],
          [ 'N', 'Normal', 'Jump to previous search match' ],
          [ 'Up / k', 'Normal', 'Activate cursor / move up' ],
          [ 'Down / j', 'Normal', 'Move cursor down' ],
          [ 'PgUp', 'Normal', 'Move cursor up one page' ],
          [ 'PgDn', 'Normal', 'Move cursor down one page' ],
          [ 'Home', 'Normal', 'Cursor to first line' ],
          [ 'End', 'Normal', 'Return to auto-follow' ],
          [ 'v', 'Normal', 'Toggle visual select mode' ],
          [ 'Shift+Up', 'Normal', 'Enter selection + extend up' ],
          [ 'Shift+Down', 'Normal', 'Enter selection + extend down' ],
          [ 'V', 'Normal', 'Export selection to JSON' ],
          [ 'g', 'Normal', 'Open fuzzy log finder' ],
          [ 'z', 'Normal', 'Toggle inline JSON fold at cursor' ],
          [ ':', 'Normal', 'Enter command mode' ],
          [ '?', 'Normal', 'Open help popup' ],
          [ 'a', 'Normal', 'Open about popup' ],
          [ 'Ctrl-S', 'Normal', 'Select Start button' ],
          [ 'Ctrl-T', 'Normal', 'Select Stop button' ],
          [ 'Enter', 'Normal', 'Activate selected button' ],
          [ 'Esc', 'Normal', 'Clear cursor + selection + search' ],
          [ 'Up / k', 'Select', 'Extend selection upward' ],
          [ 'Down / j', 'Select', 'Extend selection downward' ],
          [ 'PgUp', 'Select', 'Extend selection up one page' ],
          [ 'PgDn', 'Select', 'Extend selection down one page' ],
          [ 'Home', 'Select', 'Extend selection to first line' ],
          [ 'Enter', 'Command', 'Execute command' ],
          [ 'Esc', 'Command', 'Cancel command input' ],
          [ 'Backspace', 'Command', 'Delete last character' ],
          [ 'Enter', 'Search', 'Confirm search pattern' ],
          [ 'Esc', 'Search', 'Cancel search input' ],
          [ 'Backspace', 'Search', 'Delete last character' ],
          [ 'Esc / q / Enter', 'Popup', 'Close popup' ],
          [ 'c', 'Help popup', 'Open commands popup' ],
          [ 'h', 'Help popup', 'Open phosphor help popup' ],
          [ '?', 'Sub-popup', 'Return to help popup' ],
          [ 'z / Enter', 'JSON viewer', 'Toggle fold' ],
          [ 'l / Right', 'JSON viewer', 'Unfold node' ],
          [ 'h / Left', 'JSON viewer', 'Fold node' ],
          [ 'j / k / Up / Down', 'JSON viewer', 'Navigate nodes' ],
          [ 'PgUp / PgDn', 'JSON viewer', 'Jump one page' ],
          [ 'Home / End', 'JSON viewer', 'Jump to first / last node' ],
          [ 'Esc', 'JSON viewer', 'Back to fuzzy finder' ],
          [ 'q', 'JSON viewer', 'Close all (viewer + fuzzy)' ],
          [ 'Type', 'Fuzzy', 'Filter file list' ],
          [ 'Up / Down', 'Fuzzy', 'Navigate file list' ],
          [ 'Enter', 'Fuzzy', 'Open selected file in viewer' ],
          [ 'Esc / q', 'Fuzzy', 'Close fuzzy finder' ],
          [ '1..4', 'Normal', 'Switch panel tab (when available)' ],
          [ 'Ctrl+P', 'Any', 'Open shell / new view tab' ],
          [ 'Ctrl+Q', 'Any', 'Close shell (kill all)' ],
          [ 'Ctrl+D', 'Shell', 'Open phosphor command bar' ],
          [ 'Ctrl+X', 'Shell', 'Minimize focused screen' ],
          [ 'Ctrl+S', 'Shell', 'Save screen output to file' ],
          [ '1-9', 'Shell', 'Open screen by number' ],
          [ 'Esc', 'Shell', 'Minimize all screens, return to Normal' ],
        ],
      ),
    ),
  },
  {
    id: 'tabs',
    title: 'Panel Tabs',
    body: () => frag(
      p( 'Panels can have tabs that split stdout and stderr into separate views. The neonsignal panel has two tabs: live-stream (stdout -- request/response logs) and debug-stream (stderr -- debug logs).' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ '1', 'Switch to tab 1 (live-stream)' ],
          [ '2', 'Switch to tab 2 (debug-stream)' ],
          [ '3..4', 'Switch to tab N (if present)' ],
        ],
      ),
      p( 'Each tab has its own ring buffer, scroll position, cursor, selection, and JSON fold state. Switching tabs preserves the state of all tabs independently.' ),
      p( 'Tab indicators appear in the panel title bar after the panel name. The active tab is bold; inactive tabs are dim. Panels without tabs (redirect, watcher) work identically to before.' ),
    ),
  },
  {
    id: 'shell',
    title: 'Embedded Shell',
    body: () => frag(
      p( 'The dashboard includes an embedded terminal panel between the process panels and the status bar. It provides interactive command execution without leaving the TUI.' ),
      p( 'Terminology: the shell is the panel region, a view is a tab inside the shell, and a screen is a popup overlay showing one command\'s output.' ),
      table(
        [ 'Key', 'Action' ],
        [
          [ 'Ctrl+P', 'Toggle shell open / open new view tab' ],
          [ 'Ctrl+Q', 'Close shell entirely (kill all processes)' ],
          [ 'Ctrl+D', 'Open phosphor command bar from shell' ],
          [ 'Enter', 'Execute typed command (creates a screen)' ],
          [ 'Ctrl+X', 'Minimize focused screen overlay' ],
          [ 'Ctrl+S', 'Save screen output to shell/[date].command.txt' ],
          [ '1-9', 'Open screen by number' ],
          [ 'Esc', 'Minimize all screens, return focus to panels' ],
        ],
      ),
      p( 'Each view has a $ input line. Typing a command and pressing Enter spawns it via PTY using your login shell. Output appears in a screen popup overlay. Blocking commands disable the view\'s input until the process exits.' ),
      p( 'Screens can be minimized (hidden but still running). Ctrl+S saves the focused screen\'s output to a plain text file in cwd/shell/. The filename includes the date and the command name.' ),
    ),
  },
];

export const DashboardManual = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/pages/dashboard-manual.css' );

  if ( typeof document !== 'undefined' ) {
    setTimeout( () => {
      const contentEl = document.querySelector( '.dash-man__content' );
      const tocList = document.querySelector( '.dash-man__toc-list' );

      if ( ! contentEl || ! tocList ) {
        return;
      }

      /* render ToC */
      tocList.innerHTML = '';
      sections.forEach( section => {
        const li = document.createElement( 'li' );
        li.className = 'dash-man__toc-item';
        const a = document.createElement( 'a' );
        a.className = 'dash-man__toc-link';
        a.href = `#${section.id}`;
        a.textContent = section.title;
        li.appendChild( a );
        tocList.appendChild( li );
      } );

      /* render content sections */
      contentEl.innerHTML = '';
      sections.forEach( section => {
        const sectionEl = document.createElement( 'section' );
        sectionEl.className = 'dash-man__section';
        sectionEl.id = section.id;

        const titleEl = document.createElement( 'h2' );
        titleEl.className = 'dash-man__section-title';
        titleEl.textContent = section.title;
        sectionEl.appendChild( titleEl );

        const bodyEl = section.body();
        sectionEl.appendChild( bodyEl );

        contentEl.appendChild( sectionEl );
      } );

      initScrollTracker( {
        linkSelector: '.dash-man__toc-link',
        sectionSelector: '.dash-man__section',
        activeClass: 'dash-man__toc-item--active',
      } );
    }, 0 );
  }

  return (
    <div>
      <Header />
      <main class="dash-man">
        <aside class="dash-man__toc">
          <h3 class="dash-man__toc-title">Sections</h3>
          <ul class="dash-man__toc-list"></ul>
        </aside>
        <div class="dash-man__content">
          <h1 class="dash-man__title">Dashboard Manual</h1>
          <p class="dash-man__text">Loading...</p>
        </div>
      </main>
      <BackToTop />
      <Footer />
    </div>
  );
};

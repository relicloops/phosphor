import { css } from '@relicloops/cathode';

import type { HighlightConfig } from '../scripts/syntax-highlight';
import { highlightLine } from '../scripts/syntax-highlight';

/**
 * Terminal -- reusable terminal emulator with typing effect.
 *
 * On first visit the commands type out at "AI speed" (very fast).
 * After completion the result is cached in localStorage so subsequent
 * loads render the final state instantly.
 */

export type TerminalLine =
  | { type: 'comment'; text: string }
  | { type: 'command'; text: string }
  | { type: 'output';  text: string }
  | { type: 'blank' };

interface TerminalProps {
  id: string;
  label?: string;
  lines: TerminalLine[];
  highlight?: HighlightConfig;
}

/* hash the line data so localStorage knows when content changed */
function hashLines( lines: TerminalLine[] ): string {
  let h = 0;
  const s = JSON.stringify( lines );
  for ( let i = 0; i < s.length; i++ ) {
    h = ( ( h << 5 ) - h + s.charCodeAt( i ) ) | 0;
  }

  return 'ph-term-' + ( h >>> 0 ).toString( 36 );
}

function storageKey( id: string, lines: TerminalLine[] ): string {
  return `${id}:${hashLines( lines )}`;
}

function wasPlayed( key: string ): boolean {
  try {
    return localStorage.getItem( key ) === '1';
  }
  catch {
    return false;
  }
}

function markPlayed( key: string ): void {
  try {
    localStorage.setItem( key, '1' );
  }
  catch { /* storage full or blocked */ }
}

/* ── render helpers ──────────────────────────────────────────────── */

function renderLineStatic( line: TerminalLine, hl?: HighlightConfig ): HTMLElement {
  const row = document.createElement( 'div' );
  row.className = 'terminal__line';

  if ( line.type === 'blank' ) {
    row.innerHTML = '&nbsp;';

    return row;
  }

  if ( line.type === 'comment' ) {
    const span = document.createElement( 'span' );
    span.className = 'terminal__comment';
    span.textContent = `# ${line.text}`;
    row.appendChild( span );

    return row;
  }

  if ( line.type === 'command' ) {
    const prompt = document.createElement( 'span' );
    prompt.className = 'terminal__prompt';
    prompt.textContent = '$ ';
    row.appendChild( prompt );

    const cmd = document.createElement( 'span' );
    cmd.className = 'terminal__cmd';
    cmd.textContent = line.text;
    row.appendChild( cmd );

    return row;
  }

  /* output -- optionally syntax-highlighted */
  const span = document.createElement( 'span' );
  span.className = 'terminal__output';
  if ( hl ) {
    highlightLine( span, line.text, hl );
  } else {
    span.textContent = line.text;
  }
  row.appendChild( span );

  return row;
}

/* ── typing animation ────────────────────────────────────────────── */

const CHAR_DELAY  = 12;   // ms per character  -- AI speed
const LINE_PAUSE  = 80;   // ms pause between lines
const COMMENT_PAUSE = 40; // ms pause after comments

function animateLines(
  container: HTMLElement,
  lines: TerminalLine[],
  onDone: () => void,
  hl?: HighlightConfig
): void {
  let lineIdx = 0;

  const prefersReducedMotion =
    typeof window !== 'undefined' &&
    window.matchMedia &&
    window.matchMedia( '(prefers-reduced-motion: reduce)' ).matches;

  if ( prefersReducedMotion ) {
    /* skip animation entirely */
    lines.forEach( ( line ) => {
      container.appendChild( renderLineStatic( line, hl ) );
    } );
    onDone();

    return;
  }

  function nextLine(): void {
    if ( lineIdx >= lines.length ) {
      /* remove cursor from last line */
      const cursor = container.querySelector( '.terminal__cursor' );
      if ( cursor ) {
        cursor.remove();
      }
      onDone();

      return;
    }

    const line = lines[ lineIdx ];
    lineIdx++;

    if ( line.type === 'blank' ) {
      container.appendChild( renderLineStatic( line, hl ) );
      setTimeout( nextLine, LINE_PAUSE );

      return;
    }

    if ( line.type === 'output' ) {
      container.appendChild( renderLineStatic( line, hl ) );
      setTimeout( nextLine, LINE_PAUSE );

      return;
    }

    if ( line.type === 'comment' ) {
      /* comments appear instantly */
      container.appendChild( renderLineStatic( line, hl ) );
      setTimeout( nextLine, COMMENT_PAUSE );

      return;
    }

    /* command -- type character by character */
    const row = document.createElement( 'div' );
    row.className = 'terminal__line';

    const prompt = document.createElement( 'span' );
    prompt.className = 'terminal__prompt';
    prompt.textContent = '$ ';
    row.appendChild( prompt );

    const cmd = document.createElement( 'span' );
    cmd.className = 'terminal__cmd';
    row.appendChild( cmd );

    /* blinking cursor */
    const cursor = document.createElement( 'span' );
    cursor.className = 'terminal__cursor';
    cursor.textContent = '\u2588'; // full block
    row.appendChild( cursor );

    /* remove previous cursor if any */
    const prevCursor = container.querySelector( '.terminal__cursor' );
    if ( prevCursor ) {
      prevCursor.remove();
    }

    container.appendChild( row );

    const text = line.text;
    let charIdx = 0;

    function typeChar(): void {
      if ( charIdx >= text.length ) {
        cursor.remove();
        setTimeout( nextLine, LINE_PAUSE );

        return;
      }
      cmd.textContent += text[ charIdx ];
      charIdx++;
      setTimeout( typeChar, CHAR_DELAY );
    }

    typeChar();
  }

  nextLine();
}

/* ── component ───────────────────────────────────────────────────── */

export const Terminal = ( { id, label, lines, highlight }: TerminalProps ) => {
  css( './css/fonts/share-tech-mono.css' );
  css( './css/theme.css' );
  css( './css/components/terminal.css' );
  if ( highlight ) css( './css/syntax-highlight.css' );

  if ( typeof document !== 'undefined' ) {
    const key = storageKey( id, lines );
    const played = wasPlayed( key );

    setTimeout( () => {
      const el = document.querySelector( `[data-terminal-id="${id}"]` );
      const body = el?.querySelector( '.terminal__body' ) as HTMLElement | null;
      if ( ! el || ! body || body.hasAttribute( 'data-rendered' ) ) {
        return;
      }

      if ( played ) {
        /* already seen -- render static immediately */
        body.setAttribute( 'data-rendered', 'true' );
        body.innerHTML = '';
        lines.forEach( ( line ) => {
          body.appendChild( renderLineStatic( line, highlight ) );
        } );

        return;
      }

      /* first visit -- component was lazy-loaded on scroll via LazyOnVisible */
      body.setAttribute( 'data-rendered', 'true' );
      body.innerHTML = '';
      animateLines( body, lines, () => {
        markPlayed( key );
      }, highlight );
    }, 0 );
  }

  return (
    <div class="terminal" data-terminal-id={id}>
      <div class="terminal__bar">
        <span class="terminal__dot terminal__dot--red"></span>
        <span class="terminal__dot terminal__dot--yellow"></span>
        <span class="terminal__dot terminal__dot--green"></span>
        {label && <span class="terminal__label">{label}</span>}
      </div>
      <div class="terminal__body">
        {/* populated by the typing engine above */}
      </div>
    </div>
  );
};

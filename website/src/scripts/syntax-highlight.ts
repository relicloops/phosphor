/**
 * syntax-highlight -- generic tokenizer and DOM renderer.
 *
 * Handles strings, numbers, booleans, keywords, operators, brackets,
 * punctuation, and line comments. Language-specific keywords are passed
 * in via HighlightConfig so the same engine works for TOML, JSON, C,
 * shell, or anything else.
 */

export interface HighlightConfig {
  keywords?: string[];
  lineComment?: string;
}

export interface HighlightToken {
  type:
    | 'keyword'
    | 'string'
    | 'number'
    | 'boolean'
    | 'operator'
    | 'bracket'
    | 'punctuation'
    | 'comment'
    | 'text';
  value: string;
}

const BOOLEANS    = new Set( [ 'true', 'false', 'null', 'nil' ] );
const OPERATORS   = new Set( [ '=', '+', '-', '*', '/', '<', '>', '!', '%', '&', '|', '^', '~' ] );
const BRACKETS    = new Set( [ '[', ']', '{', '}', '(', ')' ] );
const PUNCTUATION = new Set( [ ':', ';', ',', '.' ] );

function isDigit( ch: string ): boolean {
  return ch >= '0' && ch <= '9';
}

function isWordStart( ch: string ): boolean {
  return ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ch === '_';
}

function isWordChar( ch: string ): boolean {
  return isWordStart( ch ) || isDigit( ch );
}

/* ── tokenizer ──────────────────────────────────────────────────── */

export function tokenize( text: string, config: HighlightConfig = {} ): HighlightToken[] {
  const tokens: HighlightToken[] = [];
  const keywords    = new Set( config.keywords ?? [] );
  const lineComment = config.lineComment ?? '';
  let i = 0;

  while ( i < text.length ) {
    const ch = text[ i ];

    /* line comment */
    if ( lineComment && text.startsWith( lineComment, i ) ) {
      tokens.push( { type: 'comment', value: text.slice( i ) } );
      break;
    }

    /* quoted string */
    if ( ch === '"' || ch === "'" ) {
      let j = i + 1;
      while ( j < text.length && text[ j ] !== ch ) {
        if ( text[ j ] === '\\' ) j++;
        j++;
      }
      if ( j < text.length ) j++;
      tokens.push( { type: 'string', value: text.slice( i, j ) } );
      i = j;
      continue;
    }

    /* number */
    if ( isDigit( ch ) ) {
      let j = i;
      while ( j < text.length && ( isDigit( text[ j ] ) || text[ j ] === '.' ) ) j++;
      tokens.push( { type: 'number', value: text.slice( i, j ) } );
      i = j;
      continue;
    }

    /* bracket */
    if ( BRACKETS.has( ch ) ) {
      tokens.push( { type: 'bracket', value: ch } );
      i++;
      continue;
    }

    /* operator -- consume consecutive (<<, >>, !=, ==, etc.) */
    if ( OPERATORS.has( ch ) ) {
      let j = i;
      while ( j < text.length && OPERATORS.has( text[ j ] ) ) j++;
      tokens.push( { type: 'operator', value: text.slice( i, j ) } );
      i = j;
      continue;
    }

    /* punctuation */
    if ( PUNCTUATION.has( ch ) ) {
      tokens.push( { type: 'punctuation', value: ch } );
      i++;
      continue;
    }

    /* word -- identifier, keyword, or boolean */
    if ( isWordStart( ch ) ) {
      let j = i;
      while ( j < text.length && isWordChar( text[ j ] ) ) j++;
      const word = text.slice( i, j );
      if ( BOOLEANS.has( word ) ) {
        tokens.push( { type: 'boolean', value: word } );
      } else if ( keywords.has( word ) ) {
        tokens.push( { type: 'keyword', value: word } );
      } else {
        tokens.push( { type: 'text', value: word } );
      }
      i = j;
      continue;
    }

    /* whitespace / anything else -- accumulate as plain text */
    let j = i + 1;
    while (
      j < text.length &&
      !BRACKETS.has( text[ j ] ) &&
      !OPERATORS.has( text[ j ] ) &&
      !PUNCTUATION.has( text[ j ] ) &&
      text[ j ] !== '"' && text[ j ] !== "'" &&
      !isWordStart( text[ j ] ) &&
      !isDigit( text[ j ] ) &&
      !( lineComment && text.startsWith( lineComment, j ) )
    ) {
      j++;
    }
    tokens.push( { type: 'text', value: text.slice( i, j ) } );
    i = j;
  }

  return tokens;
}

/* ── DOM renderer ───────────────────────────────────────────────── */

export function highlightLine( parent: HTMLElement, text: string, config: HighlightConfig ): void {
  const tokens = tokenize( text, config );
  for ( const token of tokens ) {
    if ( token.type === 'text' ) {
      parent.appendChild( document.createTextNode( token.value ) );
    } else {
      const span = document.createElement( 'span' );
      span.className = `hl--${token.type}`;
      span.textContent = token.value;
      parent.appendChild( span );
    }
  }
}

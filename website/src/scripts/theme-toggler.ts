const THEME_STORAGE_KEY = 'ph-theme';
const THEMES = [ 'dark', 'light', 'system' ] as const;
type Theme = typeof THEMES[number];

const NAME_STORAGE_PREFIX = 'ph:';
const preferNameStore = typeof window !== 'undefined' && window.location && window.location.protocol === 'file:';

/* Theme icons (Unicode) */
const THEME_ICONS: Record<Theme, string> = {
  dark: '\u263D', // ☽
  light: '\u263C', // ☼
  system: '\u25D0' // ◐
};

const THEME_LABELS: Record<Theme, string> = {
  dark: 'Dark theme. Click for Light.',
  light: 'Light theme. Click for System.',
  system: 'System theme. Click for Dark.'
};

/* Safe storage access */
function safeGet( storage: Storage | null, key: string ): string | null {
  if ( ! storage ) {
    return null;
  }
  try {
    return storage.getItem( key );
  }
  catch {
    return null;
  }
}

function safeSet( storage: Storage | null, key: string, value: string ): boolean {
  if ( ! storage ) {
    return false;
  }
  try {
    storage.setItem( key, value );

    return true;
  }
  catch {
    return false;
  }
}

/* window.name storage fallback (for file:// protocol) */
function readNameStore(): Record<string, string> | null {
  if ( ! window.name || window.name.indexOf( NAME_STORAGE_PREFIX ) !== 0 ) {
    return null;
  }
  const raw = window.name.slice( NAME_STORAGE_PREFIX.length );
  try {
    return JSON.parse( raw ) || {};
  }
  catch {
    return null;
  }
}

function writeNameStore( store: Record<string, string> | null, force?: boolean ): boolean {
  if ( ! force && window.name && window.name.indexOf( NAME_STORAGE_PREFIX ) !== 0 ) {
    return false;
  }
  try {
    window.name = NAME_STORAGE_PREFIX + JSON.stringify( store || {} );

    return true;
  }
  catch {
    return false;
  }
}

function getNameValue( key: string ): string | null {
  const store = readNameStore();
  if ( store && Object.prototype.hasOwnProperty.call( store, key ) ) {
    return store[ key ];
  }

  return null;
}

function setNameValue( key: string, value: string, force?: boolean ): void {
  const store = readNameStore() || {};
  store[ key ] = value;
  writeNameStore( store, force );
}

/* System theme detection */
function getSystemTheme(): 'dark' | 'light' {
  if ( window.matchMedia && window.matchMedia( '(prefers-color-scheme: light)' ).matches ) {
    return 'light';
  }

  return 'dark';
}

function getEffectiveTheme( theme: Theme ): 'dark' | 'light' {
  return theme === 'system' ? getSystemTheme() : theme;
}

/* Theme storage */
export function getStoredTheme(): Theme {
  if ( preferNameStore ) {
    const nameValue = getNameValue( THEME_STORAGE_KEY );
    if ( nameValue && THEMES.includes( nameValue as Theme ) ) {
      return nameValue as Theme;
    }
  }

  const stored = safeGet( localStorage, THEME_STORAGE_KEY );
  if ( stored && THEMES.includes( stored as Theme ) ) {
    return stored as Theme;
  }

  if ( ! preferNameStore ) {
    const nameValue = getNameValue( THEME_STORAGE_KEY );
    if ( nameValue && THEMES.includes( nameValue as Theme ) ) {
      return nameValue as Theme;
    }
  }

  return 'system';
}

function setStoredTheme( theme: Theme ): void {
  safeSet( localStorage, THEME_STORAGE_KEY, theme );
  setNameValue( THEME_STORAGE_KEY, theme, preferNameStore );
}

/* Apply theme to DOM */
export function applyTheme( theme: Theme ): void {
  const effective = getEffectiveTheme( theme );
  document.documentElement.setAttribute( 'data-theme', effective );
  document.documentElement.setAttribute( 'data-theme-setting', theme );

  const metaThemeColor = document.querySelector( 'meta[name="theme-color"]' );
  if ( metaThemeColor ) {
    const bg = getComputedStyle( document.documentElement ).getPropertyValue( '--bg-primary' ).trim();
    if ( bg ) {
      metaThemeColor.setAttribute( 'content', bg );
    }
  }
}

/* Update toggle button UI */
function updateToggleUI( theme: Theme ): void {
  const toggle = document.getElementById( 'theme-toggle' );
  if ( ! toggle ) {
    return;
  }

  toggle.textContent = THEME_ICONS[ theme ];
  toggle.setAttribute( 'title', THEME_LABELS[ theme ] );
  toggle.setAttribute( 'aria-label', THEME_LABELS[ theme ] );
}

/* Cycle through themes */
function cycleTheme(): Theme {
  const current = getStoredTheme();
  const currentIndex = THEMES.indexOf( current );
  const nextIndex = ( currentIndex + 1 ) % THEMES.length;
  const nextTheme = THEMES[ nextIndex ];

  setStoredTheme( nextTheme );
  applyTheme( nextTheme );
  updateToggleUI( nextTheme );

  return nextTheme;
}

/* Initialize toggle button with event delegation */
let delegationInitialized = false;

function initToggle(): void {
  /* Use event delegation - attach to document, works even if button doesn't exist yet */
  if ( ! delegationInitialized ) {
    delegationInitialized = true;
    document.addEventListener( 'click', ( event: MouseEvent ) => {
      const target = event.target as HTMLElement;
      if ( target && target.id === 'theme-toggle' ) {
        cycleTheme();
      }
    } );
  }

  /* Try to update UI now, and also observe for when element appears */
  const current = getStoredTheme();
  updateToggleUI( current );

  /* Use MutationObserver to catch when toggle is added to DOM */
  if ( typeof MutationObserver !== 'undefined' ) {
    const observer = new MutationObserver( () => {
      const toggle = document.getElementById( 'theme-toggle' );
      if ( toggle ) {
        updateToggleUI( getStoredTheme() );
        observer.disconnect();
      }
    } );
    observer.observe( document.body || document.documentElement, {
      childList: true,
      subtree: true
    } );
  }
}

/* Watch for system theme changes */
function watchSystemTheme(): void {
  if ( ! window.matchMedia ) {
    return;
  }

  const mediaQuery = window.matchMedia( '(prefers-color-scheme: dark)' );
  const handler = (): void => {
    const stored = getStoredTheme();
    if ( stored === 'system' ) {
      applyTheme( 'system' );
    }
  };

  if ( mediaQuery.addEventListener ) {
    mediaQuery.addEventListener( 'change', handler );
  }
}

/* Initialize theme system */
export function initTheme(): void {
  applyTheme( getStoredTheme() );

  if ( document.readyState === 'loading' ) {
    document.addEventListener( 'DOMContentLoaded', () => {
      initToggle();
      watchSystemTheme();
    } );
  }
  else {
    initToggle();
    watchSystemTheme();
  }
}

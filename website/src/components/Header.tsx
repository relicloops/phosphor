import { css } from '@relicloops/cathode';

import { NavDropdown } from './NavDropdown';
import { initTheme } from '../scripts/theme-toggler';

export const Header = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/components/header.css' );

  /* initialize theme system */
  initTheme();

  /* mobile menu toggle - use event delegation */
  if ( typeof document !== 'undefined' ) {
    document.addEventListener( 'click', ( e: MouseEvent ) => {
      const target = e.target as HTMLElement;
      if ( target && target.id === 'menu-toggle' ) {
        const nav = document.getElementById( 'mobile-nav' );
        if ( nav ) {
          const isOpen = nav.classList.toggle( 'header__nav--open' );
          target.textContent = isOpen ? '\u25B8' : '\u25BC'; // ▸ or ▼
          target.setAttribute( 'aria-expanded', String( isOpen ) );
        }
      }
    } );
  }

  return (
    <header class="header" aria-label="Site header">
      <div class="header__brand">
        <a href="/" class="header__logo-link" aria-label="Home">
          <span class="header__logo-symbol">&#x1F73D;</span>
        </a>
        <a href="/" class="header__name">
          phosphor
        </a>
      </div>

      <nav id="mobile-nav" class="header__nav" aria-label="Main navigation">
        <ul class="header__nav-list">
          <li><a href="/" class="header__nav-link">Home</a></li>
          <li><a href="/docs.html" class="header__nav-link">Docs</a></li>
          <li><a href="/cli-commands.html" class="header__nav-link">CLI</a></li>
          <li><a href="/ai-history.html" class="header__nav-link">AI History</a></li>
          <NavDropdown
            label="Dashboard"
            href="/dashboard.html"
            items={[
              { label: 'Manual', href: '/dashboard-manual.html' },
              { label: 'Implementation', href: '/dashboard-implementation.html' },
            ]}
          />
        </ul>
      </nav>

      <div class="header__actions">
        <button
          id="theme-toggle"
          class="header__theme-toggle"
          type="button"
          aria-label="Toggle theme"
          title="System theme. Click for Dark."
        >
          &#x25D0;
        </button>
        <a
          href="https://github.com/relicloops/phosphor"
          class="header__cta"
          target="_blank"
          rel="noopener"
        >
          GitHub
        </a>
        <button
          id="menu-toggle"
          class="header__menu-toggle"
          type="button"
          aria-label="Open menu"
          aria-expanded="false"
          aria-controls="mobile-nav"
        >
          &#x25BC;
        </button>
      </div>
    </header>
  );
};

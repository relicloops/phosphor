import { css } from '@relicloops/cathode';

import { BackToTop } from '../components/BackToTop';
import { Footer } from '../components/Footer';
import { Header } from '../components/Header';
import { Spinner } from '../components/Spinner';
import { initScrollTracker } from '../scripts/scroll-tracker';

interface DocSection {
  id: string;
  title: string;
  content: string;
}

export const Docs = () => {
  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/pages/docs.css' );

  if ( typeof document !== 'undefined' ) {
    /* defer to next tick so the JSX below is rendered into the DOM first */
    setTimeout( () => {
      const contentEl = document.querySelector( '.docs__content' );
      const tocList = document.querySelector( '.docs-toc__list' );

      if ( ! contentEl || ! tocList ) {
        return;
      }

      /* clear the Spinner placeholder rendered by JSX */
      contentEl.innerHTML = '';

      fetch( './docs.json' )
        .then( res => res.json() )
        .then( ( sections: DocSection[] ) => {
          /* render ToC */
          tocList.innerHTML = '';
          sections.forEach( ( section ) => {
            const li = document.createElement( 'li' );
            li.className = 'docs-toc__item';
            const a = document.createElement( 'a' );
            a.className = 'docs-toc__link';
            a.href = `#${section.id}`;
            a.textContent = section.title;
            li.appendChild( a );
            tocList.appendChild( li );
          } );

          /* render content sections */
          contentEl.innerHTML = '';
          sections.forEach( ( section ) => {
            const sectionEl = document.createElement( 'section' );
            sectionEl.className = 'docs__section';
            sectionEl.id = section.id;

            const titleEl = document.createElement( 'h2' );
            titleEl.className = 'docs__section-title';
            titleEl.textContent = section.title;
            sectionEl.appendChild( titleEl );

            const bodyEl = document.createElement( 'div' );
            bodyEl.className = 'docs__section-body';
            bodyEl.innerHTML = section.content;
            sectionEl.appendChild( bodyEl );

            contentEl.appendChild( sectionEl );
          } );

          /* initialize scroll tracker after content is rendered */
          initScrollTracker();
        } )
        .catch( () => {
          contentEl.innerHTML = '';
          const errorEl = document.createElement( 'p' );
          errorEl.style.color = 'var(--neon-red)';
          errorEl.textContent = 'Failed to load documentation.';
          contentEl.appendChild( errorEl );
        } );
    }, 0 );

    /* mobile ToC toggle */
    document.addEventListener( 'click', ( e: MouseEvent ) => {
      const target = e.target as HTMLElement;
      if ( target && target.id === 'toc-toggle' ) {
        const nav = document.querySelector( '.docs-toc__nav' );
        if ( nav ) {
          const isOpen = nav.classList.toggle( 'docs-toc__nav--open' );
          target.textContent = isOpen ? '\u25BE' : '\u25B8'; // \u25BE or \u25B8
          target.setAttribute( 'aria-expanded', String( isOpen ) );
        }
      }
    } );
  }

  return (
    <>
      <Header />
      <main class="docs">
        <aside class="docs-toc" aria-label="Table of contents">
          <h2 class="docs-toc__title">Contents</h2>
          <button
            id="toc-toggle"
            class="docs-toc__toggle"
            type="button"
            aria-label="Toggle table of contents"
            aria-expanded="false"
          >
            &#x25B8;
          </button>
          <nav class="docs-toc__nav" aria-label="Documentation sections">
            <ul class="docs-toc__list">
              {/* populated dynamically from docs.json */}
            </ul>
          </nav>
        </aside>
        <div class="docs__content">
          <Spinner message="Loading documentation..." />
        </div>
      </main>
      <Footer />
      <BackToTop />
    </>
  );
};

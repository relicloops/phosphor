import { css } from '@relicloops/cathode';

import { BackToTop } from '../components/BackToTop';
import { Footer } from '../components/Footer';
import { Header } from '../components/Header';
import { Spinner } from '../components/Spinner';
import { initScrollTracker } from '../scripts/scroll-tracker';

interface AiHistoryChild {
  id: string;
  title: string;
  status: string;
  statusSymbol: string;
  content: string;
}

interface AiHistoryEntry {
  id: string;
  title: string;
  status: string;
  statusSymbol: string;
  updated: string;
  category: string;
  content: string;
  children?: AiHistoryChild[];
}

type CategoryGroup = {
  label: string;
  entries: AiHistoryEntry[];
};

const CATEGORY_ORDER: Record<string, string> = {
  'overview': 'Overview',
  'masterplan': 'Masterplan',
  'phase': 'Phases',
  'feature-plan': 'Feature Plans',
  'reference': 'Reference',
};

function groupByCategory( entries: AiHistoryEntry[] ): CategoryGroup[] {
  const groups: Record<string, AiHistoryEntry[]> = {};

  for ( const entry of entries ) {
    const cat = entry.category;
    if ( ! groups[ cat ] ) {
      groups[ cat ] = [];
    }
    groups[ cat ].push( entry );
  }

  const result: CategoryGroup[] = [];
  for ( const [ key, label ] of Object.entries( CATEGORY_ORDER ) ) {
    if ( groups[ key ] ) {
      result.push( { label, entries: groups[ key ] } );
    }
  }

  return result;
}

function renderStatusBadge( status: string, symbol: string ): string {
  return `<span class="ai-status ai-status--${status}">${symbol} ${status}</span>`;
}

function renderTocItem( entry: AiHistoryEntry ): HTMLElement {
  const li = document.createElement( 'li' );
  li.className = 'ai-toc__item';
  li.dataset.sectionId = entry.id;

  const a = document.createElement( 'a' );
  a.className = 'ai-toc__link';
  a.href = `#${entry.id}`;
  a.innerHTML = `<span class="ai-status ai-status--${entry.status}" style="font-size:0.55rem;padding:0.05rem 0.25rem;">${entry.statusSymbol}</span> ${entry.title}`;
  li.appendChild( a );

  return li;
}

function renderSection( entry: AiHistoryEntry ): HTMLElement {
  const section = document.createElement( 'section' );
  section.className = 'ai-history__section';
  section.id = entry.id;

  /* header: title + status + date */
  const header = document.createElement( 'div' );
  header.className = 'ai-history__section-header';
  header.innerHTML = `
    <h2 class="ai-history__section-title">${entry.title}</h2>
    ${renderStatusBadge( entry.status, entry.statusSymbol )}
    ${entry.updated ? `<span class="ai-history__section-date">${entry.updated}</span>` : ''}
  `;
  section.appendChild( header );

  /* body content -- truncate masterplan to avoid DOM overload */
  const body = document.createElement( 'div' );
  body.className = 'ai-history__section-body';
  if ( entry.content.length > 50000 ) {
    body.innerHTML = entry.content.slice( 0, 50000 )
      + '<p style="color:var(--text-dim);font-style:italic;">Content truncated. See full document in docs/source/plans/.</p>';
  }
  else {
    body.innerHTML = entry.content;
  }
  section.appendChild( body );

  /* children (phase tasks) */
  if ( entry.children && entry.children.length > 0 ) {
    const childrenWrap = document.createElement( 'div' );
    childrenWrap.className = 'ai-history__children';

    const toggle = document.createElement( 'button' );
    toggle.className = 'ai-history__child-toggle';
    toggle.type = 'button';
    toggle.innerHTML = `<span class="ai-history__child-toggle-icon">\u25B8</span> ${entry.children.length} task files`;
    childrenWrap.appendChild( toggle );

    const list = document.createElement( 'div' );
    list.className = 'ai-history__child-list';

    for ( const child of entry.children ) {
      const item = document.createElement( 'div' );
      item.className = 'ai-history__child-item';
      item.id = child.id;

      const childHeader = document.createElement( 'div' );
      childHeader.className = 'ai-history__child-header';
      childHeader.innerHTML = `
        <span class="ai-history__child-title">${child.title}</span>
        ${renderStatusBadge( child.status, child.statusSymbol )}
      `;
      item.appendChild( childHeader );

      if ( child.content ) {
        const childBody = document.createElement( 'div' );
        childBody.className = 'ai-history__child-body';
        childBody.innerHTML = child.content.length > 20000
          ? child.content.slice( 0, 20000 ) + '<p style="color:var(--text-dim);font-style:italic;">Truncated.</p>'
          : child.content;
        item.appendChild( childBody );
      }

      list.appendChild( item );
    }

    childrenWrap.appendChild( list );
    section.appendChild( childrenWrap );

    /* toggle click handler */
    toggle.addEventListener( 'click', () => {
      const isOpen = list.classList.toggle( 'ai-history__child-list--open' );
      const icon = toggle.querySelector( '.ai-history__child-toggle-icon' );
      if ( icon ) {
        icon.classList.toggle( 'ai-history__child-toggle-icon--open', isOpen );
      }
    } );
  }

  return section;
}

export const AiHistory = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/pages/ai-history.css' );

  if ( typeof document !== 'undefined' ) {
    /* defer to next tick so the JSX below is rendered first */
    setTimeout( () => {
      const contentEl = document.querySelector( '.ai-history__content' );
      const tocList = document.querySelector( '.ai-toc__list' );

      if ( ! contentEl || ! tocList ) {
        return;
      }

      /* clear spinner placeholder */
      contentEl.innerHTML = '';

      fetch( './ai-history.json' )
        .then( res => res.json() )
        .then( ( entries: AiHistoryEntry[] ) => {
          const groups = groupByCategory( entries );

          /* render page title and intro */
          const titleEl = document.createElement( 'h1' );
          titleEl.className = 'ai-history__page-title';
          titleEl.textContent = 'AI History';
          contentEl.appendChild( titleEl );

          const introEl = document.createElement( 'p' );
          introEl.className = 'ai-history__intro';
          introEl.textContent = 'Complete log of AI contributions to the phosphor project. '
            + 'Plans, phase task breakdowns, feature development, reference documentation, and audit reports.';
          contentEl.appendChild( introEl );

          /* render ToC and content by category */
          tocList.innerHTML = '';

          for ( const group of groups ) {
            /* ToC group label */
            const groupLabel = document.createElement( 'li' );
            groupLabel.className = 'ai-toc__group-label';
            groupLabel.textContent = group.label;
            tocList.appendChild( groupLabel );

            for ( const entry of group.entries ) {
              /* ToC item */
              tocList.appendChild( renderTocItem( entry ) );

              /* content section */
              contentEl.appendChild( renderSection( entry ) );
            }
          }

          /* initialize scroll tracker */
          initScrollTracker();
        } )
        .catch( () => {
          contentEl.innerHTML = '';
          const errorEl = document.createElement( 'p' );
          errorEl.style.color = 'var(--neon-red)';
          errorEl.textContent = 'Failed to load AI history data.';
          contentEl.appendChild( errorEl );
        } );
    }, 0 );

    /* mobile ToC toggle */
    document.addEventListener( 'click', ( e: MouseEvent ) => {
      const target = e.target as HTMLElement;
      if ( target && target.id === 'ai-toc-toggle' ) {
        const nav = document.querySelector( '.ai-toc__nav' );
        if ( nav ) {
          const isOpen = nav.classList.toggle( 'ai-toc__nav--open' );
          target.textContent = isOpen ? '\u25BE' : '\u25B8';
          target.setAttribute( 'aria-expanded', String( isOpen ) );
        }
      }
    } );
  }

  return (
    <>
      <Header />
      <main class="ai-history">
        <aside class="ai-toc" aria-label="Table of contents">
          <h2 class="ai-toc__title">Contents</h2>
          <button
            id="ai-toc-toggle"
            class="ai-toc__toggle"
            type="button"
            aria-label="Toggle table of contents"
            aria-expanded="false"
          >
            &#x25B8;
          </button>
          <nav class="ai-toc__nav" aria-label="AI history sections">
            <ul class="ai-toc__list">
              {/* populated dynamically from ai-history.json */}
            </ul>
          </nav>
        </aside>
        <div class="ai-history__content">
          <Spinner message="Loading AI history..." />
        </div>
      </main>
      <Footer />
      <BackToTop />
    </>
  );
};

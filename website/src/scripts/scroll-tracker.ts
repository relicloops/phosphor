/**
 * Scroll Tracker
 *
 * Tracks scroll position and updates Table of Contents active state
 * using Intersection Observer API for performance.
 *
 * Accepts an optional config to work with any page's TOC structure.
 * Defaults to the Docs page selectors for backward compatibility.
 */

interface ScrollTrackerConfig {
  linkSelector: string;
  sectionSelector: string;
  activeClass: string;
  navSelector: string;
  navOpenClass: string;
  toggleId: string;
}

export const initScrollTracker = ( config?: Partial<ScrollTrackerConfig> ) => {
  if ( typeof document === 'undefined' || typeof IntersectionObserver === 'undefined' ) {
    return;
  }

  const cfg: ScrollTrackerConfig = {
    linkSelector: '.docs-toc__link',
    sectionSelector: '.docs__section',
    activeClass: 'docs-toc__item--active',
    navSelector: '.docs-toc__nav',
    navOpenClass: 'docs-toc__nav--open',
    toggleId: 'toc-toggle',
    ...config,
  };

  const tocLinks = document.querySelectorAll( cfg.linkSelector );
  const sections = document.querySelectorAll( cfg.sectionSelector );

  if ( ! tocLinks.length || ! sections.length ) {
    return;
  }

  /**
   * Update active ToC link
   */
  const updateActiveLink = ( id: string ) => {
    tocLinks.forEach( ( link ) => {
      const item = link.parentElement;
      if ( ! item ) {
        return;
      }

      if ( link.getAttribute( 'href' ) === `#${id}` ) {
        item.classList.add( cfg.activeClass );
      }
      else {
        item.classList.remove( cfg.activeClass );
      }
    } );
  };

  /**
   * Intersection Observer configuration
   * rootMargin creates a "viewport" that triggers when sections
   * are 20% from top and 70% from bottom
   */
  const observer = new IntersectionObserver(
    ( entries ) => {
      entries.forEach( ( entry ) => {
        if ( entry.isIntersecting ) {
          const id = entry.target.getAttribute( 'id' );
          if ( id ) {
            updateActiveLink( id );
          }
        }
      } );
    },
    {
      rootMargin: '-20% 0px -70% 0px',
      threshold: 0,
    }
  );

  /* observe all sections */
  sections.forEach( section => observer.observe( section ) );

  /**
   * Smooth scroll to section on ToC link click
   * Also closes mobile ToC after selection
   */
  tocLinks.forEach( ( link ) => {
    link.addEventListener( 'click', ( e ) => {
      e.preventDefault();
      const href = link.getAttribute( 'href' );
      if ( ! href ) {
        return;
      }

      const targetId = href.replace( '#', '' );
      const target = document.getElementById( targetId );

      if ( target ) {
        /* smooth scroll to target */
        target.scrollIntoView( {
          behavior: 'smooth',
          block: 'start',
        } );

        /* update URL hash */
        history.pushState( null, '', href );

        /* close mobile ToC */
        const nav = document.querySelector( cfg.navSelector );
        const toggle = document.getElementById( cfg.toggleId );
        if ( nav && toggle && window.innerWidth <= 1024 ) {
          nav.classList.remove( cfg.navOpenClass );
          toggle.textContent = '\u25B8'; // ▸
          toggle.setAttribute( 'aria-expanded', 'false' );
        }
      }
    } );
  } );

  /* handle initial hash on page load */
  if ( window.location.hash ) {
    const initialId = window.location.hash.replace( '#', '' );
    updateActiveLink( initialId );
  }
};

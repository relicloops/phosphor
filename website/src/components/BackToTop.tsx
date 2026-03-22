import { css } from '@relicloops/cathode';

export const BackToTop = () => {
  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/components/backtotop.css' );

  if ( typeof document !== 'undefined' ) {
    const prefersReducedMotion = window.matchMedia && window.matchMedia( '(prefers-reduced-motion: reduce)' ).matches;

    const updateVisibility = () => {
      const button = document.getElementById( 'back-to-top' );
      if ( ! button ) {
        return;
      }
      const shouldShow = window.scrollY > 320;
      button.classList.toggle( 'backtotop--visible', shouldShow );
      button.setAttribute( 'aria-hidden', String( ! shouldShow ) );
      if ( shouldShow ) {
        button.removeAttribute( 'tabindex' );
      }
      else {
        button.setAttribute( 'tabindex', '-1' );
      }
    };

    window.addEventListener( 'scroll', updateVisibility, { passive: true } );
    window.addEventListener( 'load', updateVisibility );

    document.addEventListener( 'click', ( e: MouseEvent ) => {
      const target = e.target as HTMLElement;
      if ( target && target.id === 'back-to-top' ) {
        window.scrollTo( {
          top: 0,
          behavior: prefersReducedMotion ? 'auto' : 'smooth'
        } );
      }
    } );
  }

  return (
    <button
      id="back-to-top"
      class="backtotop"
      type="button"
      aria-label="Back to top"
      aria-hidden="true"
      tabindex="-1"
    >
      &#x2191;
    </button>
  );
};

import { css } from '@relicloops/cathode';

/**
 * ImageViewer -- reusable click-to-enlarge image component.
 *
 * Renders an image that opens a fullscreen overlay on click.
 * Click the overlay or press Esc to close.
 *
 * Usage:
 *   <ImageViewer src="/media/photo.jpg" alt="Description" />
 */

interface ImageViewerProps {
  src: string;
  alt: string;
  className?: string;
}

export const ImageViewer = ( props: ImageViewerProps ) => {

  css( './css/components/image-viewer.css' );

  if ( typeof document !== 'undefined' ) {
    document.addEventListener( 'click', ( e: MouseEvent ) => {
      const target = e.target as HTMLElement;

      /* thumbnail click -> open overlay */
      if ( target.classList.contains( 'imgview__thumb' ) ) {
        const src = target.getAttribute( 'data-imgview-src' );
        if ( ! src ) {
          return;
        }

        let overlay = document.getElementById( 'imgview-overlay' );
        if ( ! overlay ) {
          overlay = document.createElement( 'div' );
          overlay.id = 'imgview-overlay';
          overlay.className = 'imgview__overlay';
          overlay.innerHTML = '<img class="imgview__full" />';
          document.body.appendChild( overlay );
        }

        const img = overlay.querySelector( '.imgview__full' ) as HTMLImageElement;
        if ( img ) {
          img.src = src;
          img.alt = target.getAttribute( 'alt' ) || '';
        }
        overlay.classList.add( 'imgview__overlay--open' );
      }

      /* overlay click -> close */
      if ( target.classList.contains( 'imgview__overlay' )
        || target.classList.contains( 'imgview__full' ) ) {
        const overlay = document.getElementById( 'imgview-overlay' );
        if ( overlay ) {
          overlay.classList.remove( 'imgview__overlay--open' );
        }
      }
    } );

    /* Esc -> close */
    document.addEventListener( 'keydown', ( e: KeyboardEvent ) => {
      if ( e.key === 'Escape' ) {
        const overlay = document.getElementById( 'imgview-overlay' );
        if ( overlay ) {
          overlay.classList.remove( 'imgview__overlay--open' );
        }
      }
    } );
  }

  return (
    <img
      src={props.src}
      alt={props.alt}
      class={`imgview__thumb ${props.className || ''}`}
      data-imgview-src={props.src}
    />
  );
};

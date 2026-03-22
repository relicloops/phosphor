type AssetsReadyOptions = {
  attribute?: string;
  root?: HTMLElement;
};

const waitForNextFrame = () => {
  return new Promise<void>( ( resolve ) => {
    if ( typeof requestAnimationFrame === 'undefined' ) {
      resolve();

      return;
    }
    requestAnimationFrame( () => {
      resolve();
    } );
  } );
};

const waitForFonts = () => {
  if ( typeof document === 'undefined' ) {
    return Promise.resolve();
  }

  if ( 'fonts' in document && document.fonts && document.fonts.ready ) {
    return document.fonts.ready.catch( () => undefined );
  }

  return Promise.resolve();
};

const waitForStyleStability = ( idleMs = 120, timeoutMs = 3000 ) => {
  if ( typeof document === 'undefined' ) {
    return Promise.resolve();
  }

  return new Promise<void>( ( resolve ) => {
    let idleTimer: number | undefined;
    let timeoutTimer: number | undefined;
    const done = () => {
      if ( idleTimer ) {
        window.clearTimeout( idleTimer );
      }
      if ( timeoutTimer ) {
        window.clearTimeout( timeoutTimer );
      }
      observer.disconnect();
      resolve();
    };

    const kick = () => {
      if ( idleTimer ) {
        window.clearTimeout( idleTimer );
      }
      idleTimer = window.setTimeout( done, idleMs );
    };

    const observer = new MutationObserver( ( mutations ) => {
      for ( const mutation of mutations ) {
        mutation.addedNodes.forEach( ( node ) => {
          if ( node instanceof HTMLLinkElement && node.rel === 'stylesheet' ) {
            kick();
          }
          if ( node instanceof HTMLStyleElement ) {
            kick();
          }
        } );
      }
    } );

    observer.observe( document.head, { childList: true } );
    kick();
    timeoutTimer = window.setTimeout( done, timeoutMs );
  } );
};

const isStylesheetReady = ( link: HTMLLinkElement ) => {
  const status = link.getAttribute( 'data-neon-css-status' );
  if ( status === 'loaded' || status === 'error' ) {
    return true;
  }

  if ( link.sheet ) {
    try {
      void ( link.sheet as CSSStyleSheet ).cssRules;

      return true;
    }
    catch {
      return false;
    }
  }

  return false;
};

const waitForStyles = () => {
  if ( typeof document === 'undefined' ) {
    return Promise.resolve();
  }

  return waitForStyleStability().then( () => {
    const links = Array.from( document.querySelectorAll( 'link[rel="stylesheet"]' ) ) as HTMLLinkElement[];
    if ( links.length === 0 ) {
      return Promise.resolve();
    }

    const pending = links.map( ( link ) => {
      if ( isStylesheetReady( link ) ) {
        return Promise.resolve();
      }

      return new Promise<void>( ( resolve ) => {
        const done = () => resolve();
        link.addEventListener( 'load', done, { once: true } );
        link.addEventListener( 'error', done, { once: true } );
      } );
    } );

    return Promise.all( pending ).then( () => undefined );
  } );
};

const waitForImages = () => {
  if ( typeof document === 'undefined' ) {
    return Promise.resolve();
  }

  const images = Array.from( document.images );
  if ( images.length === 0 ) {
    return Promise.resolve();
  }

  const pending = images.map( ( image ) => {
    if ( image.complete ) {
      return Promise.resolve();
    }

    if ( 'decode' in image && typeof image.decode === 'function' ) {
      return image.decode().catch( () => undefined );
    }

    return new Promise<void>( ( resolve ) => {
      const done = () => resolve();
      image.addEventListener( 'load', done, { once: true } );
      image.addEventListener( 'error', done, { once: true } );
    } );
  } );

  return Promise.all( pending ).then( () => undefined );
};

export const markAssetsReady = async ( options: AssetsReadyOptions = {} ) => {
  if ( typeof document === 'undefined' ) {
    return;
  }

  const attribute = options.attribute ?? 'data-assets-ready';
  const target = options.root ?? document.documentElement;

  target.setAttribute( attribute, 'false' );

  await waitForNextFrame();
  await waitForStyles();
  await waitForNextFrame();
  await Promise.all( [
    waitForFonts(),
    waitForImages(),
  ] );

  target.setAttribute( attribute, 'true' );
};

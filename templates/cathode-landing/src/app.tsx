import { lazyOnDemand, render, Suspense } from '@relicloops/cathode';
import { connectHMR, enableDevMode, hmr } from '@relicloops/cathode/hmr';

import { Spinner } from './components/Spinner';
import { markAssetsReady } from './scripts/assets-ready';
import { asDefaultExport } from './scripts/lazy-utils';

const Index = lazyOnDemand( asDefaultExport( () => import( './pages/Index.js' ), mod => mod.Index ) );
const NotFound = lazyOnDemand( asDefaultExport( () => import( './pages/NotFound.js' ), mod => mod.NotFound ) );

declare const __PROJECT_DEV__: boolean;
declare const __PROJECT_PUBLIC_DIR__: string;
const isDev = __PROJECT_DEV__;

function bootstrap() {

  const root = document.getElementById( 'root' );
  if ( ! root ) {
    return;
  }

  let currentPage: any = null;
  let currentProps: Record<string, unknown> = {};
  const getRootNode = () => {
    if ( ! currentPage ) {
      return null;
    }
    const PageComponent = currentPage;

    return (
      <Suspense fallback={<Spinner message="Loading page..." />}>
        <PageComponent {...currentProps} />
      </Suspense>
    );
  };

  if ( isDev ) {
    enableDevMode();
    hmr.registerRoot( import.meta.url, root, getRootNode );
  }

  document.documentElement.setAttribute( 'data-assets-ready', 'false' );

  const neonStatus = ( window as any ).__NEON_STATUS;
  const neonPath = ( window as any ).__NEON_PATH || window.location.pathname;

  const resolveBootTheme = () => {
    const attributeTheme = document.documentElement.getAttribute( 'data-theme' );
    if ( attributeTheme === 'dark' || attributeTheme === 'light' ) {
      return attributeTheme;
    }

    let stored: string | null = null;
    try {
      stored = localStorage.getItem( 'cl-theme' );
    }
    catch {
      stored = null;
    }

    if ( stored === 'dark' || stored === 'light' ) {
      return stored;
    }

    if ( stored === 'system' || ! stored ) {
      return window.matchMedia && window.matchMedia( '(prefers-color-scheme: light)' ).matches ? 'light' : 'dark';
    }

    return 'dark';
  };

  const ensureBootLoader = () => {
    let loader = document.getElementById( 'boot-loader' );
    if ( ! loader ) {
      const theme = resolveBootTheme();
      loader = document.createElement( 'div' );
      loader.id = 'boot-loader';
      loader.style.position = 'fixed';
      loader.style.inset = '0';
      loader.style.zIndex = '9999';
      loader.style.display = 'flex';
      loader.style.alignItems = 'center';
      loader.style.justifyContent = 'center';
      loader.style.background = theme === 'light' ? '#f0f0f5' : '#0a0a0f';
      loader.style.color = theme === 'light' ? '#1a1a2e' : '#e0e0f0';
      loader.style.pointerEvents = 'none';
      document.body.appendChild( loader );
      render( <Spinner message="Loading page..." />, loader );
    }

    return loader;
  };

  ensureBootLoader();

  const renderPage = ( Page: any, props: Record<string, unknown> = {} ) => {
    currentPage = Page;
    currentProps = props;
    const loadPromise = Page && typeof Page.__load === 'function'
      ? Promise.resolve( Page.__load() )
      : Promise.resolve();

    render( getRootNode(), root );

    loadPromise
      .catch( () => { /* load error */ } )
      .finally( () => {
        void markAssetsReady().then( () => {
          const loader = document.getElementById( 'boot-loader' );
          if ( loader ) {
            loader.remove();
          }
        } );
      } );
  };

  if ( isDev ) {
    setTimeout( () => {
      let hmrRenderTimer: ReturnType<typeof setTimeout> | null = null;

      connectHMR( {
        pathTransform: path =>
          path.startsWith( __PROJECT_PUBLIC_DIR__ )
            ? path.slice( __PROJECT_PUBLIC_DIR__.length )
            : path,
        onHotUpdate: ( _event, moduleUrl, mod ) => {
          hmr.applyUpdate( moduleUrl, mod );
          hmr.refreshModule( moduleUrl );
          if ( currentPage ) {
            if ( hmrRenderTimer ) {
              clearTimeout( hmrRenderTimer );
            }
            hmrRenderTimer = setTimeout( () => {
              hmrRenderTimer = null;
              render( getRootNode(), root );
            }, 100 );
          }
        },
        onHotError: ( event, error ) => {
          console.trace( '[hmr] update error', event, error );
        },
      } );
    }, 500 );
  }

  if ( neonStatus === 404 ) {
    renderPage( NotFound, { path: neonPath } );

    return;
  }

  renderPage( Index );
}

document.addEventListener( 'DOMContentLoaded', () => {
  bootstrap();
} );

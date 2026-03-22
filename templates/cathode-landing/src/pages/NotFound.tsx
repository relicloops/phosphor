import { css } from '@relicloops/cathode';

import { Footer } from '../components/Footer';
import { Header } from '../components/Header';

export const NotFound = ( props: { path: string } ) => {
  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/pages/notfound.css' );

  return (
    <>
      <Header />
      <main class="notfound">
        <section class="notfound__section">
          <div class="notfound__panel">
            <div class="notfound__code">404</div>
            <h1 class="notfound__title">Signal Lost</h1>

            <div class="notfound__body">
              <p>
                Resource not found: <code class="notfound__path">{props.path}</code>
              </p>
              <p>
                The requested resource could not be located.
              </p>
            </div>

            <a class="notfound__cta" href="/">&#x2190; Return to Home</a>
          </div>
        </section>
      </main>
      <Footer />
    </>
  );
};

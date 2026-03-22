import { css } from '@relicloops/cathode';

import { Heart } from './Heart';
import { OrnamentalDiamond } from './OrnamentalDiamond';

export const Footer = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/components/footer.css' );

  return (
    <footer class="footer" aria-label="Site footer">
      <div class="footer__inner">
        <div class="footer__top">
          <div class="footer__brand">
            <a href="/" class="footer__logo-link" aria-label="Home">
              <span class="footer__logo-symbol">&#x1F73D;</span>
              <span class="footer__name">phosphor</span>
            </a>
            <p class="footer__tagline">
              Scaffolding for NeonSignal/Cathode projects.
            </p>
          </div>

          <div class="footer__group">
            <h4 class="footer__group-title">Ecosystem</h4>
            <ul class="footer__list">
              <li><a class="footer__link" href="https://github.com/relicloops/cathode" target="_blank" rel="noopener">Cathode</a></li>
              <li><a class="footer__link" href="https://github.com/relicloops/phosphor" target="_blank" rel="noopener">Phosphor</a></li>
            </ul>
          </div>

          <div class="footer__group">
            <h4 class="footer__group-title">Resources</h4>
            <ul class="footer__list">
              <li><a class="footer__link" href="/docs.html">Documentation</a></li>
              <li><a class="footer__link" href="https://github.com/relicloops/phosphor" target="_blank" rel="noopener">Source Code</a></li>
            </ul>
          </div>
        </div>

        <div class="footer__bottom">
          <div class="footer__legal">
            <span>Apache License 2.0</span>
          </div>
          <div class="footer__credit">
            <span>Made with <Heart /> and </span>
            <span class="footer__credit-phrase">
              <a class="footer__link" href="https://github.com/relicloops/cathode" target="_blank" rel="noopener">
                cathode
              </a>
              <OrnamentalDiamond />
              <span>riding</span>
              <a
                class="footer__link"
                href="https://github.com/nutsloop/neonsignal"
                target="_blank"
                rel="noopener"
              >
                neonsignal
              </a>
            </span>
          </div>
        </div>
      </div>
    </footer>
  );
};

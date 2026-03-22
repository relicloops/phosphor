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
              <span class="footer__name"><<name>></span>
            </a>
            <p class="footer__tagline">
              <<project_description>>
            </p>
          </div>
        </div>

        <div class="footer__bottom">
          <div class="footer__legal">
            <span>Apache License 2.0</span>
          </div>
          <div class="footer__credit">
            <span>Made with <Heart /> riding </span>
            <span class="footer__credit-phrase">
              <a
                class="footer__link"
                href="https://github.com/nutsloop/neonsignal"
                target="_blank"
                rel="noopener"
              >
                NeonSignal
              </a>
              <OrnamentalDiamond />
              <span>Scaffolded with</span>
              <a
                class="footer__link"
                href="https://github.com/relicloops/phosphor"
                target="_blank"
                rel="noopener"
              >
                phosphor
              </a>
            </span>
          </div>
        </div>
      </div>
    </footer>
  );
};

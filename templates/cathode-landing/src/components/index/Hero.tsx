import { css } from '@relicloops/cathode';

export const Hero = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/components/index/hero.css' );

  return (
    <section class="hero" aria-label="Hero">
      <div class="hero__inner">
        <div class="hero__content">
          <h1 class="hero__title">
            <<name>>
          </h1>
          <p class="hero__lead">
            <<project_description>>
          </p>
          <div class="hero__actions">
            <a class="hero__cta hero__cta--primary" href="<<github_url>>" target="_blank" rel="noopener">
              View Source
            </a>
          </div>
        </div>
      </div>
    </section>
  );
};

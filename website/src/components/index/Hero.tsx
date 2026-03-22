import { css, lazyOnDemand, LazyOnVisible } from '@relicloops/cathode';

import { asDefaultExport } from '../../scripts/lazy-utils';
import { EcosystemBrand } from '../EcosystemBrand';
import type { TerminalLine } from '../Terminal';

const LazyTerminal = lazyOnDemand(
  asDefaultExport( () => import( '../Terminal.js' ), mod => mod.Terminal )
);

const QUICK_START_LINES: TerminalLine[] = [
  { type: 'comment', text: 'clone and build phosphor' },
  { type: 'command', text: 'git clone https://github.com/relicloops/phosphor.git' },
  { type: 'command', text: 'cd phosphor' },
  { type: 'command', text: 'meson setup build' },
  { type: 'command', text: 'ninja -C build' },
  { type: 'blank' },
  { type: 'comment', text: 'scaffold a new project from a template' },
  { type: 'command', text: './build/phosphor create --name=my-site --template=./templates/landing' },
  { type: 'blank' },
  { type: 'comment', text: 'verify the installation' },
  { type: 'command', text: './build/phosphor version' },
  { type: 'output',  text: 'phosphor 0.0.1-022' },
];

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
          <p class="hero__eyebrow"><EcosystemBrand size="md" /></p>
          <div class="hero__symbol">&#x1F73D;</div>
          <h1 class="hero__title">
            phosphor
          </h1>
          <p class="hero__lead">
            A pure C CLI tool that scaffolds projects from template manifests,
            builds, cleans, and runs diagnostics for the NeonSignal/Cathode
            ecosystem.
          </p>
          <div class="hero__actions">
            <a class="hero__cta hero__cta--primary" href="/docs.html">
              Read the Docs
            </a>
            <a class="hero__cta hero__cta--ghost" href="https://github.com/relicloops/phosphor" target="_blank" rel="noopener">
              View Source
            </a>
          </div>
          <div class="hero__meta">
            <div class="hero__meta-item">
              <span class="hero__meta-label">Language</span>
              <span class="hero__meta-value">C17 / POSIX</span>
            </div>
            <div class="hero__meta-item">
              <span class="hero__meta-label">License</span>
              <span class="hero__meta-value">Apache 2.0</span>
            </div>
            <div class="hero__meta-item">
              <span class="hero__meta-label">Build</span>
              <span class="hero__meta-value">Meson + Ninja</span>
            </div>
            <div class="hero__meta-item">
              <span class="hero__meta-label">Version</span>
              <span class="hero__meta-value">0.0.1-022</span>
            </div>
          </div>
        </div>
        <div class="hero__terminal">
          <LazyOnVisible
            component={LazyTerminal}
            componentProps={{ id: 'quick-start', label: 'terminal', lines: QUICK_START_LINES }}
            rootMargin="200px"
            threshold={0.1}
          />
        </div>
      </div>
    </section>
  );
};

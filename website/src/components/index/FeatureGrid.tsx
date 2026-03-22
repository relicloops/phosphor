import { css } from '@relicloops/cathode';

const FEATURES = [
  {
    icon: '+',
    name: 'create',
    desc: 'Scaffold new projects from TOML template manifests. Variables, filters, ops, and hooks -- all declared, all reproducible.',
    usage: 'phosphor create --name=my-app --template=./tpl',
  },
  {
    icon: '\u25B8', // ▸
    name: 'build',
    desc: 'Compile and bundle projects via spawned child processes. Exit codes map cleanly to phosphor diagnostics.',
    usage: 'phosphor build --project=./my-app',
  },
  {
    icon: '\u00D7', // x
    name: 'clean',
    desc: 'Remove build artifacts and staging directories. Supports dry-run mode to preview what would be deleted.',
    usage: 'phosphor clean --dry-run',
  },
  {
    icon: '\u2713', // checkmark
    name: 'doctor',
    desc: 'Run diagnostic checks on the local environment. Verifies dependencies, paths, and configuration integrity.',
    usage: 'phosphor doctor',
  },
];

export const FeatureGrid = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/components/index/feature-grid.css' );

  return (
    <section class="feature-grid" aria-label="Features">
      <h2 class="feature-grid__title">Commands</h2>
      <div class="feature-grid__cards">
        {FEATURES.map( feature => (
          <div class="feature-card" key={feature.name}>
            <div class="feature-card__icon">{feature.icon}</div>
            <h3 class="feature-card__name">{feature.name}</h3>
            <p class="feature-card__desc">{feature.desc}</p>
            <code class="feature-card__usage">{feature.usage}</code>
          </div>
        ) )}
      </div>
    </section>
  );
};

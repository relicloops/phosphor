import { css } from '@relicloops/cathode';

import { BackToTop } from '../components/BackToTop';
import { Footer } from '../components/Footer';
import { Header } from '../components/Header';
import { initScrollTracker } from '../scripts/scroll-tracker';

interface FlagDef {
  name: string;
  syntax: string;
  description: string;
  annotation?: string;
}

interface CommandDef {
  name: string;
  description: string;
  usage: string;
  flags: FlagDef[];
}

const commands: CommandDef[] = [
  {
    name: 'create',
    description: 'Scaffold a new project from a template.',
    usage: 'phosphor create --name=<name> --template=<path>',
    flags: [
      { name: 'name', syntax: '--name=<string>', description: 'Project name; used as the destination folder.', annotation: 'required' },
      { name: 'template', syntax: '--template=<path>', description: 'Template source. Can be a local path, a git URL, or an archive file (tar.gz, tar.zst, zip).' },
      { name: 'output', syntax: '--output=<path>', description: 'Custom output directory. When set, overrides --name for the destination folder.' },
      { name: 'tld', syntax: '--tld=<string>', description: 'Top-level domain passed to template variables.', annotation: 'default: .host' },
      { name: 'owner', syntax: '--owner=<string>', description: 'Project owner name. Passed through to template variables for rendering.' },
      { name: 'owner-slug', syntax: '--owner-slug=<string>', description: 'Slugified owner name. Passed through to template variables.' },
      { name: 'github', syntax: '--github=<url>', description: 'GitHub URL. Passed through to template variables.' },
      { name: 'instagram', syntax: '--instagram=<url>', description: 'Instagram URL. Passed through to template variables.' },
      { name: 'x', syntax: '--x=<url>', description: 'X (Twitter) URL. Passed through to template variables.' },
      { name: 'force', syntax: '--force', description: 'Overwrite existing destination files without prompting.' },
      { name: 'dry-run', syntax: '--dry-run', description: 'Print the operations that would be executed without actually creating any files.' },
      { name: 'checksum', syntax: '--checksum=<string>', description: 'SHA256 checksum for archive template verification. If the archive does not match, the operation is aborted.' },
      { name: 'toml', syntax: '--toml', description: 'Output the create report in TOML format. Reserved for future use.' },
      { name: 'allow-hooks', syntax: '--allow-hooks', description: 'Run post-create hooks defined in the template manifest. Reserved for future use.' },
      { name: 'yes', syntax: '--yes', description: 'Auto-confirm all interactive prompts. Reserved for future use.' },
      { name: 'normalize-eol', syntax: '--normalize-eol=<lf|crlf>', description: 'Normalize line endings in rendered text files. Reserved for future use.' },
      { name: 'allow-hidden', syntax: '--allow-hidden', description: 'Include hidden (dot) files from the template source. Reserved for future use.' },
      { name: 'verbose', syntax: '--verbose', description: 'Enable verbose output during scaffolding. Reserved for future use.' },
    ],
  },
  {
    name: 'build',
    description: 'Bundle and deploy a Cathode JSX project via esbuild.',
    usage: 'phosphor build [--project=<path>]',
    flags: [
      { name: 'project', syntax: '--project=<string>', description: 'Project root directory. Defaults to the current working directory.' },
      { name: 'deploy-at', syntax: '--deploy-at=<string>', description: 'Custom deployment target directory. Must stay within the project root. Defaults to public/{SNI}{TLD}.' },
      { name: 'clean-first', syntax: '--clean-first', description: 'Remove the build/ and deploy directories before building, ensuring a fresh build from scratch.' },
      { name: 'tld', syntax: '--tld=<string>', description: 'Top-level domain used in deploy path construction and esbuild defines. Falls back to the TLD environment variable, then .host.' },
      { name: 'strict', syntax: '--strict', description: 'Treat post-build metadata cleanup warnings as errors. Useful in CI to catch stale platform files.' },
      { name: 'toml', syntax: '--toml', description: 'Output the build report in TOML format instead of plain text. Useful for machine consumption.' },
      { name: 'verbose', syntax: '--verbose', description: 'Set log level to DEBUG, showing detailed build progress and asset copying.' },
      { name: 'normalize-eol', syntax: '--normalize-eol=<lf|crlf>', description: 'Normalize line endings in build output. Reserved for future use.' },
      { name: 'legacy-scripts', syntax: '--legacy-scripts', description: 'Use legacy shell scripts (scripts/_default/all.sh) instead of the native esbuild pipeline. Deprecated; will be removed in a future release.' },
    ],
  },
  {
    name: 'clean',
    description: 'Remove build artifacts and stale staging directories.',
    usage: 'phosphor clean [--project=<path>]',
    flags: [
      { name: 'stale', syntax: '--stale', description: 'Remove only stale .phosphor-staging-* directories left behind by interrupted create operations.' },
      { name: 'wipe', syntax: '--wipe', description: 'Remove build/, public/, and node_modules/. Use this for a full reset before a fresh install.' },
      { name: 'project', syntax: '--project=<path>', description: 'Project root directory. Defaults to the current working directory.' },
      { name: 'dry-run', syntax: '--dry-run', description: 'Preview what would be deleted without actually removing anything.' },
      { name: 'verbose', syntax: '--verbose', description: 'Set log level to DEBUG for detailed deletion logging.' },
      { name: 'legacy-scripts', syntax: '--legacy-scripts', description: 'Use the legacy clean script (scripts/_default/clean.sh) instead of native cleanup. Deprecated.' },
    ],
  },
  {
    name: 'rm',
    description: 'Remove a specific path within the project root.',
    usage: 'phosphor rm --specific=<path>',
    flags: [
      { name: 'specific', syntax: '--specific=<path>', description: 'Relative path to remove. Must not contain ".." traversal or escape the project root.', annotation: 'required' },
      { name: 'project', syntax: '--project=<path>', description: 'Project root directory. Defaults to the current working directory.' },
      { name: 'force', syntax: '--force', description: 'Remove even if no phosphor manifest (template.phosphor.toml or phosphor.toml) is found in the project root.' },
      { name: 'verbose', syntax: '--verbose', description: 'Enable debug-level logging.' },
    ],
  },
  {
    name: 'certs',
    description: 'Generate TLS certificates for local development (self-signed CA) or production (Let\'s Encrypt ACME HTTP-01).',
    usage: 'phosphor certs --generate\nphosphor certs --local [--ca-only] [--domain=<name>]\nphosphor certs --letsencrypt [--domain=<name>] [--action=<action>] [--staging]',
    flags: [
      { name: 'generate', syntax: '--generate', description: 'Generate all certificates from manifest: root CA, all local leaves, and all Let\'s Encrypt domains.' },
      { name: 'local', syntax: '--local', description: 'Local mode: generate the self-signed root CA and leaf certificates for all domains with mode = "local".' },
      { name: 'letsencrypt', syntax: '--letsencrypt', description: 'Let\'s Encrypt mode: request production TLS certificates via ACME HTTP-01 for all domains with mode = "letsencrypt".' },
      { name: 'ca-only', syntax: '--ca-only', description: 'Generate only the root CA (combine with --local). Skips leaf certificate generation.' },
      { name: 'domain', syntax: '--domain=<string>', description: 'Target a specific domain by name. Only the matching domain entry is processed.' },
      { name: 'action', syntax: '--action=<request|renew|verify>', description: 'Let\'s Encrypt action. "request" obtains a new certificate, "renew" re-requests, "verify" checks expiry.', annotation: 'default: request' },
      { name: 'staging', syntax: '--staging', description: 'Use the Let\'s Encrypt staging endpoint. Staging certificates are not trusted by browsers but have no rate limits. Useful for testing.' },
      { name: 'project', syntax: '--project=<path>', description: 'Project root directory. Defaults to the current working directory.' },
      { name: 'output', syntax: '--output=<path>', description: 'Override the certificate output directory from the manifest.' },
      { name: 'dry-run', syntax: '--dry-run', description: 'Print planned operations without executing. Shows the full ACME flow steps for Let\'s Encrypt domains.' },
      { name: 'force', syntax: '--force', description: 'Overwrite existing certificate files.' },
      { name: 'verbose', syntax: '--verbose', description: 'Enable debug-level logging.' },
    ],
  },
  {
    name: 'glow',
    description: 'Scaffold a Cathode landing page from the embedded template. Zero external dependencies -- the template is compiled into the phosphor binary.',
    usage: 'phosphor glow --name=<project-name>',
    flags: [
      { name: 'name', syntax: '--name=<string>', description: 'Project name; used as the destination folder and passed to template variables.', annotation: 'required' },
      { name: 'output', syntax: '--output=<path>', description: 'Custom output directory. When set, overrides --name for the destination folder.' },
      { name: 'description', syntax: '--description=<string>', description: 'Project description passed to template variables.', annotation: 'default: A Cathode JSX website' },
      { name: 'github-url', syntax: '--github-url=<url>', description: 'GitHub URL passed to template variables.', annotation: 'default: https://github.com' },
      { name: 'force', syntax: '--force', description: 'Overwrite existing destination files without prompting.' },
      { name: 'dry-run', syntax: '--dry-run', description: 'Print planned operations without creating any files.' },
      { name: 'verbose', syntax: '--verbose', description: 'Enable verbose output during scaffolding.' },
    ],
  },
  {
    name: 'serve',
    description: 'Start the neonsignal HTTPS dev server (and optionally the HTTP redirect service and file watcher) for the project. Displays an ncurses dashboard showing output from all processes. Reads defaults from the [serve] manifest section; CLI flags override.',
    usage: 'phosphor serve [--project=<path>]',
    flags: [
      { name: 'project', syntax: '--project=<path>', description: 'Project root directory. Defaults to the current working directory.' },
      { name: 'verbose', syntax: '--verbose', description: 'Enable debug-level logging.' },
      { name: 'neonsignal-bin', syntax: '--neonsignal-bin=<path>', description: 'Path to the neonsignal binary. Default: search PATH.' },
      { name: 'redirect-bin', syntax: '--redirect-bin=<path>', description: 'Path to the neonsignal_redirect binary. Default: search PATH.' },
      { name: 'no-redirect', syntax: '--no-redirect', description: 'Skip launching the HTTP->HTTPS redirect service.' },
      { name: 'threads', syntax: '--threads=<int>', description: 'Neonsignal worker threads.', annotation: 'default: 3' },
      { name: 'host', syntax: '--host=<string>', description: 'Neonsignal bind address.', annotation: 'default: 0.0.0.0' },
      { name: 'port', syntax: '--port=<int>', description: 'Neonsignal HTTPS listen port.', annotation: 'default: 9443' },
      { name: 'www-root', syntax: '--www-root=<path>', description: 'Static files root directory. Resolved from [serve] > [deploy] > "public".' },
      { name: 'certs-root', syntax: '--certs-root=<path>', description: 'TLS certificates directory. Resolved from [serve] > [certs] > "certs".' },
      { name: 'working-dir', syntax: '--working-dir=<path>', description: 'Neonsignal working directory for resolving relative paths.' },
      { name: 'upload-dir', syntax: '--upload-dir=<path>', description: 'Neonsignal upload directory override.' },
      { name: 'augments-dir', syntax: '--augments-dir=<path>', description: 'Neonsignal API augments directory.' },
      { name: 'grafts-dir', syntax: '--grafts-dir=<path>', description: 'Neonsignal grafts directory.' },
      { name: 'enable-debug', syntax: '--enable-debug', description: 'Enable neonsignal debug logging (stderr).' },
      { name: 'enable-log', syntax: '--enable-log', description: 'Enable neonsignal request/response access logging (stdout).' },
      { name: 'enable-log-color', syntax: '--enable-log-color', description: 'Colorful access log output with symbols.' },
      { name: 'enable-file-log', syntax: '--enable-file-log', description: 'Write JSON log files to disk.' },
      { name: 'log-directory', syntax: '--log-directory=<path>', description: 'Directory for JSON log files. Required with --enable-file-log. All dashboard saves also route here.' },
      { name: 'disable-proxies-check', syntax: '--disable-proxies-check', description: 'Log proxy health check requests (normally excluded).' },
      { name: 'watch', syntax: '--watch', description: 'Start a file watcher alongside neonsignal that rebuilds on source changes.' },
      { name: 'watch-cmd', syntax: '--watch-cmd=<string>', description: 'Custom watch command. Default: node scripts/_default/build.mjs --watch.' },
      { name: 'no-dashboard', syntax: '--no-dashboard', description: 'Disable ncurses dashboard; show raw process output.' },
      { name: 'redirect-instances', syntax: '--redirect-instances=<int>', description: 'Redirect service worker instances.', annotation: 'default: 2' },
      { name: 'redirect-host', syntax: '--redirect-host=<string>', description: 'Redirect bind address.', annotation: 'default: 0.0.0.0' },
      { name: 'redirect-port', syntax: '--redirect-port=<int>', description: 'Redirect HTTP listen port.', annotation: 'default: 9090' },
      { name: 'redirect-target-port', syntax: '--redirect-target-port=<int>', description: 'Redirect HTTPS target port. Defaults to the neonsignal port.' },
      { name: 'redirect-acme-webroot', syntax: '--redirect-acme-webroot=<path>', description: 'Redirect ACME webroot directory for HTTP-01 challenges.' },
      { name: 'redirect-working-dir', syntax: '--redirect-working-dir=<path>', description: 'Redirect working directory for resolving relative paths.' },
    ],
  },
  {
    name: 'doctor',
    description: 'Run project diagnostics: manifest detection, tool availability (openssl, esbuild, neonsignal, neonsignal_redirect), node deps, build state, stale staging dirs, cert status.',
    usage: 'phosphor doctor [--project=<path>]',
    flags: [
      { name: 'project', syntax: '--project=<path>', description: 'Project root directory. Defaults to the current working directory.' },
      { name: 'verbose', syntax: '--verbose', description: 'Enable verbose output.' },
      { name: 'toml', syntax: '--toml', description: 'Output diagnostic report in TOML format.' },
    ],
  },
  {
    name: 'version',
    description: 'Print the phosphor version string.',
    usage: 'phosphor version',
    flags: [],
  },
  {
    name: 'help',
    description: 'Show help for a specific command.',
    usage: 'phosphor help <command>',
    flags: [],
  },
];

const renderFlag = ( flag: FlagDef ) => {
  return (
    <div class="cli-cmd__flag" id={`flag-${flag.name}`}>
      <code class="cli-cmd__flag-syntax">{flag.syntax}</code>
      {flag.annotation
        ? <span class="cli-cmd__flag-tag">{flag.annotation}</span>
        : null}
      <p class="cli-cmd__flag-desc">{flag.description}</p>
    </div>
  );
};

const renderCommand = ( cmd: CommandDef ) => {
  return (
    <section class="cli-cmd__section" id={cmd.name}>
      <h2 class="cli-cmd__title">{cmd.name}</h2>
      <p class="cli-cmd__desc">{cmd.description}</p>
      <pre class="cli-cmd__usage"><code>{cmd.usage}</code></pre>
      {cmd.flags.length > 0
        ? (
          <>
            <h3 class="cli-cmd__flags-heading">Flags</h3>
            <div class="cli-cmd__flags">
              {cmd.flags.map( f => renderFlag( f ) )}
            </div>
          </>
        )
        : <p class="cli-cmd__no-flags">No flags.</p>}
    </section>
  );
};

export const CliCommands = () => {

  css( './css/fonts/share-tech-mono.css' );
  css( './css/theme.css' );
  css( './css/pages/cli-commands.css' );

  if ( typeof document !== 'undefined' ) {
    /* defer to next tick so JSX is rendered first */
    setTimeout( () => {
      initScrollTracker( {
        linkSelector: '.cli-ref__toc-link',
        sectionSelector: '.cli-cmd__section',
        activeClass: 'cli-ref__toc-item--active',
        navSelector: '.cli-ref__toc-nav',
        navOpenClass: 'cli-ref__toc-nav--open',
        toggleId: 'cli-toc-toggle',
      } );
    }, 0 );

    /* mobile ToC toggle */
    document.addEventListener( 'click', ( e: MouseEvent ) => {
      const target = e.target as HTMLElement;
      if ( target && target.id === 'cli-toc-toggle' ) {
        const nav = document.querySelector( '.cli-ref__toc-nav' );
        if ( nav ) {
          const isOpen = nav.classList.toggle( 'cli-ref__toc-nav--open' );
          target.textContent = isOpen ? '\u25BE' : '\u25B8';
          target.setAttribute( 'aria-expanded', String( isOpen ) );
        }
      }
    } );
  }

  return (
    <>
      <Header />
      <main class="cli-ref" aria-label="CLI command reference">

        <aside class="cli-ref__toc" aria-label="Command list">
          <h2 class="cli-ref__toc-title">Commands</h2>
          <button
            id="cli-toc-toggle"
            class="cli-ref__toc-toggle"
            type="button"
            aria-label="Toggle command list"
            aria-expanded="false"
          >
            &#x25B8;
          </button>
          <nav class="cli-ref__toc-nav" aria-label="Command sections">
            <ul class="cli-ref__toc-list">
              {commands.map( cmd => (
                <li class="cli-ref__toc-item">
                  <a href={`#${cmd.name}`} class="cli-ref__toc-link">{cmd.name}</a>
                </li>
              ) )}
            </ul>
          </nav>
        </aside>

        <div class="cli-ref__content">
          <h1 class="cli-ref__page-title">CLI Commands</h1>
          <p class="cli-ref__intro">
            Complete reference for every phosphor command and flag.
            Run <code>phosphor help &lt;command&gt;</code> for the same
            information in your terminal.
          </p>
          {commands.map( cmd => renderCommand( cmd ) )}
        </div>

      </main>
      <Footer />
      <BackToTop />
    </>
  );
};

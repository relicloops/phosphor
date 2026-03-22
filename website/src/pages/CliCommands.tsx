import { css } from '@relicloops/cathode';

import { BackToTop } from '../components/BackToTop';
import { Footer } from '../components/Footer';
import { Header } from '../components/Header';

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
    name: 'doctor',
    description: 'Run project diagnostics. Not yet implemented.',
    usage: 'phosphor doctor [--project=<path>]',
    flags: [
      { name: 'project', syntax: '--project=<path>', description: 'Project root directory. Not yet implemented.' },
      { name: 'verbose', syntax: '--verbose', description: 'Enable verbose output. Not yet implemented.' },
      { name: 'toml', syntax: '--toml', description: 'Output diagnostic report in TOML format. Not yet implemented.' },
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

  return (
    <>
      <Header />
      <main class="cli-ref" aria-label="CLI command reference">

        <aside class="cli-ref__toc" aria-label="Command list">
          <h2 class="cli-ref__toc-title">Commands</h2>
          <nav>
            <ul class="cli-ref__toc-list">
              {commands.map( cmd => (
                <li>
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

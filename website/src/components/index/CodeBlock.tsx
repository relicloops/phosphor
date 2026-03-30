import { css, lazyOnDemand, LazyOnVisible } from '@relicloops/cathode';

import { asDefaultExport } from '../../scripts/lazy-utils';
import type { HighlightConfig } from '../../scripts/syntax-highlight';
import type { TerminalLine } from '../Terminal';

const LazyTerminal = lazyOnDemand(
  asDefaultExport( () => import( '../Terminal.js' ), mod => mod.Terminal )
);

const TOML_LINES: TerminalLine[] = [
  { type: 'command', text: 'cat template.phosphor.toml' },
  { type: 'blank' },

  /* ---- [manifest] -- required metadata ---- */
  { type: 'comment', text: '# [manifest] -- required metadata' },
  { type: 'output', text: '[manifest]' },
  { type: 'output', text: 'schema = 1' },
  { type: 'output', text: 'id = "neonsignal-landing"' },
  { type: 'output', text: 'version = "0.1.0"' },
  { type: 'blank' },

  /* ---- [template] -- template metadata ---- */
  { type: 'comment', text: '# [template] -- template metadata' },
  { type: 'output', text: '[template]' },
  { type: 'output', text: 'name = "NeonSignal Landing Page"' },
  { type: 'output', text: 'source_root = "src"' },
  { type: 'output', text: 'description = "Static landing site template for NeonSignal"' },
  { type: 'output', text: 'min_phosphor = "0.1.0"' },
  { type: 'output', text: 'license = "Apache-2.0"' },
  { type: 'blank' },

  /* ---- [defaults] -- default variable values ---- */
  { type: 'comment', text: '# [defaults] -- default variable values (key = value pairs)' },
  { type: 'output', text: '[defaults]' },
  { type: 'output', text: 'owner = "relicloops"' },
  { type: 'output', text: 'owner_slug = "relicloops"' },
  { type: 'output', text: 'github = "https://github.com/relicloops"' },
  { type: 'blank' },

  /* ---- [[variables]] -- string, required ---- */
  { type: 'comment', text: '# [[variables]] -- variable definitions (all types shown)' },
  { type: 'output', text: '[[variables]]' },
  { type: 'output', text: 'name = "project_name"' },
  { type: 'output', text: 'type = "string"' },
  { type: 'output', text: 'required = true' },
  { type: 'output', text: 'pattern = "^[a-z][a-z0-9-]*$"' },
  { type: 'blank' },

  /* ---- [[variables]] -- enum with choices ---- */
  { type: 'output', text: '[[variables]]' },
  { type: 'output', text: 'name = "tld"' },
  { type: 'output', text: 'type = "enum"' },
  { type: 'output', text: 'default = ".host"' },
  { type: 'output', text: 'choices = [".host", ".com", ".io", ".dev"]' },
  { type: 'blank' },

  /* ---- [[variables]] -- int with min/max ---- */
  { type: 'output', text: '[[variables]]' },
  { type: 'output', text: 'name = "port"' },
  { type: 'output', text: 'type = "int"' },
  { type: 'output', text: 'default = "8080"' },
  { type: 'output', text: 'min = 1024' },
  { type: 'output', text: 'max = 65535' },
  { type: 'blank' },

  /* ---- [[variables]] -- bool ---- */
  { type: 'output', text: '[[variables]]' },
  { type: 'output', text: 'name = "enable_https"' },
  { type: 'output', text: 'type = "bool"' },
  { type: 'output', text: 'default = "true"' },
  { type: 'blank' },

  /* ---- [[variables]] -- path from env ---- */
  { type: 'output', text: '[[variables]]' },
  { type: 'output', text: 'name = "deploy_dir"' },
  { type: 'output', text: 'type = "path"' },
  { type: 'output', text: 'env = "PHOSPHOR_DEPLOY_DIR"' },
  { type: 'output', text: 'default = "./public"' },
  { type: 'blank' },

  /* ---- [[variables]] -- url ---- */
  { type: 'output', text: '[[variables]]' },
  { type: 'output', text: 'name = "homepage"' },
  { type: 'output', text: 'type = "url"' },
  { type: 'output', text: 'default = "https://<<project_name>>.host"' },
  { type: 'blank' },

  /* ---- [[variables]] -- secret ---- */
  { type: 'output', text: '[[variables]]' },
  { type: 'output', text: 'name = "api_key"' },
  { type: 'output', text: 'type = "string"' },
  { type: 'output', text: 'env = "API_KEY"' },
  { type: 'output', text: 'secret = true' },
  { type: 'blank' },

  /* ---- [filters] -- file filtering ---- */
  { type: 'comment', text: '# [filters] -- file inclusion/exclusion rules' },
  { type: 'output', text: '[filters]' },
  { type: 'output', text: 'exclude = ["*.bak", ".git", "node_modules"]' },
  { type: 'output', text: 'deny = ["secrets.toml", ".env.production"]' },
  { type: 'output', text: 'exclude_regex = ["^\\\\._", ".*\\\\.tmp$"]' },
  { type: 'output', text: 'deny_regex = ["password.*\\\\.txt"]' },
  { type: 'output', text: 'binary_ext = [".png", ".jpg", ".gif", ".ico", ".woff2"]' },
  { type: 'output', text: 'text_ext = [".ts", ".tsx", ".css", ".html", ".json", ".toml"]' },
  { type: 'blank' },

  /* ---- [build] -- esbuild configuration ---- */
  { type: 'comment', text: '# [build] -- esbuild pipeline configuration' },
  { type: 'output', text: '[build]' },
  { type: 'output', text: 'entry = "src/app.tsx"' },
  { type: 'blank' },

  /* ---- [[build.defines]] -- compile-time defines ---- */
  { type: 'output', text: '[[build.defines]]' },
  { type: 'output', text: 'name = "__PHOSPHOR_DEV__"' },
  { type: 'output', text: 'env = "PHOSPHOR_DEV"' },
  { type: 'output', text: 'default = "false"' },
  { type: 'blank' },
  { type: 'output', text: '[[build.defines]]' },
  { type: 'output', text: 'name = "__PHOSPHOR_PUBLIC_DIR__"' },
  { type: 'output', text: 'env = "PHOSPHOR_PUBLIC_DIR"' },
  { type: 'output', text: 'default = "/public"' },
  { type: 'blank' },

  /* ---- [deploy] -- deploy target ---- */
  { type: 'comment', text: '# [deploy] -- deploy target directory for phosphor build / serve' },
  { type: 'output', text: '[deploy]' },
  { type: 'output', text: 'public_dir = "public/<<project_name>>.host"' },
  { type: 'blank' },

  /* ---- [serve] -- dev server configuration ---- */
  { type: 'comment', text: '# [serve] -- neonsignal dev server defaults (overridden by CLI flags)' },
  { type: 'output', text: '[serve]' },
  { type: 'output', text: 'no_redirect = false' },
  { type: 'blank' },

  { type: 'output', text: '[serve.neonsignal]' },
  { type: 'output', text: 'threads = 3' },
  { type: 'output', text: 'port = 9443' },
  { type: 'output', text: 'www_root = "public"' },
  { type: 'output', text: 'certs_root = "certs"' },
  { type: 'blank' },

  { type: 'output', text: '[serve.redirect]' },
  { type: 'output', text: 'instances = 2' },
  { type: 'output', text: 'port = 9090' },
  { type: 'output', text: 'target_port = 9443' },
  { type: 'blank' },

  /* ---- [certs] -- TLS certificate generation ---- */
  { type: 'comment', text: '# [certs] -- TLS certificate generation (local CA + Let\'s Encrypt)' },
  { type: 'output', text: '[certs]' },
  { type: 'output', text: 'output_dir = "certs"' },
  { type: 'output', text: 'ca_cn = "phosphor-local-CA"' },
  { type: 'output', text: 'ca_bits = 4096' },
  { type: 'output', text: 'ca_days = 3650' },
  { type: 'output', text: 'leaf_bits = 2048' },
  { type: 'output', text: 'leaf_days = 825' },
  { type: 'output', text: 'account_key = "~/.phosphor/acme/account.key"' },
  { type: 'blank' },

  /* ---- [[certs.domains]] -- local self-signed ---- */
  { type: 'output', text: '[[certs.domains]]' },
  { type: 'output', text: 'name = "<<project_name>>.host"' },
  { type: 'output', text: 'mode = "local"' },
  { type: 'output', text: 'san = ["<<project_name>>.host", "localhost", "127.0.0.1"]' },
  { type: 'blank' },

  /* ---- [[certs.domains]] -- Let's Encrypt production ---- */
  { type: 'output', text: '[[certs.domains]]' },
  { type: 'output', text: 'name = "<<project_name>>.com"' },
  { type: 'output', text: 'mode = "letsencrypt"' },
  { type: 'output', text: 'san = ["<<project_name>>.com", "www.<<project_name>>.com"]' },
  { type: 'output', text: 'dir_name = "production"' },
  { type: 'output', text: 'email = "admin@<<project_name>>.com"' },
  { type: 'output', text: 'webroot = "/var/www/acme-challenge"' },
  { type: 'blank' },

  /* ---- [[ops]] -- all five operation kinds ---- */
  { type: 'comment', text: '# [[ops]] -- template operations (mkdir, copy, render, chmod, remove)' },
  { type: 'output', text: '[[ops]]' },
  { type: 'output', text: 'id = "create-root"' },
  { type: 'output', text: 'kind = "mkdir"' },
  { type: 'output', text: 'to = "<<project_name>>"' },
  { type: 'blank' },

  { type: 'output', text: '[[ops]]' },
  { type: 'output', text: 'id = "copy-assets"' },
  { type: 'output', text: 'kind = "copy"' },
  { type: 'output', text: 'from = "assets"' },
  { type: 'output', text: 'to = "<<project_name>>/assets"' },
  { type: 'output', text: 'overwrite = false' },
  { type: 'blank' },

  { type: 'output', text: '[[ops]]' },
  { type: 'output', text: 'id = "render-sources"' },
  { type: 'output', text: 'kind = "render"' },
  { type: 'output', text: 'from = "src"' },
  { type: 'output', text: 'to = "<<project_name>>/src"' },
  { type: 'output', text: 'atomic = true' },
  { type: 'output', text: 'newline = "lf"' },
  { type: 'blank' },

  { type: 'output', text: '[[ops]]' },
  { type: 'output', text: 'id = "set-executable"' },
  { type: 'output', text: 'kind = "chmod"' },
  { type: 'output', text: 'to = "<<project_name>>/scripts/dev.sh"' },
  { type: 'output', text: 'mode = "0755"' },
  { type: 'output', text: 'condition = "<<enable_https>>"' },
  { type: 'blank' },

  { type: 'output', text: '[[ops]]' },
  { type: 'output', text: 'id = "cleanup-readme"' },
  { type: 'output', text: 'kind = "remove"' },
  { type: 'output', text: 'to = "<<project_name>>/TEMPLATE-README.md"' },
  { type: 'blank' },

  /* ---- [[hooks]] -- lifecycle hooks ---- */
  { type: 'comment', text: '# [[hooks]] -- lifecycle hooks (pre-create, post-create)' },
  { type: 'output', text: '[[hooks]]' },
  { type: 'output', text: 'when = "pre-create"' },
  { type: 'output', text: 'run = ["echo", "Scaffolding <<project_name>>..."]' },
  { type: 'blank' },

  { type: 'output', text: '[[hooks]]' },
  { type: 'output', text: 'when = "post-create"' },
  { type: 'output', text: 'run = ["git", "init"]' },
  { type: 'output', text: 'cwd = "<<project_name>>"' },
  { type: 'output', text: 'allow_failure = true' },
  { type: 'blank' },

  { type: 'output', text: '[[hooks]]' },
  { type: 'output', text: 'when = "post-create"' },
  { type: 'output', text: 'run = ["npm", "install"]' },
  { type: 'output', text: 'cwd = "<<project_name>>"' },
  { type: 'output', text: 'condition = "<<enable_https>>"' },
  { type: 'output', text: 'allow_failure = false' },
];

const TOML_HIGHLIGHT: HighlightConfig = {
  keywords: [
    'manifest', 'template', 'variables', 'filters',
    'ops', 'hooks', 'defaults', 'build', 'defines',
    'deploy', 'serve', 'neonsignal', 'redirect',
    'certs', 'domains',
  ],
  lineComment: '#',
};

export const CodeBlock = () => {

  /* fonts */
  css( './css/fonts/share-tech-mono.css' );
  /* theme */
  css( './css/theme.css' );
  /* component styles */
  css( './css/components/index/code-block.css' );

  return (
    <section class="code-block" aria-label="Template manifest">
      <h2 class="code-block__title">Template Manifest</h2>
      <LazyOnVisible
        component={LazyTerminal}
        componentProps={{ id: 'toml-example', label: 'template.phosphor.toml', lines: TOML_LINES, highlight: TOML_HIGHLIGHT }}
        rootMargin="100px"
        threshold={0.15}
      />
    </section>
  );
};

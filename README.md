# phosphor

[![CI](https://github.com/relicloops/phosphor/actions/workflows/ci.yml/badge.svg)](https://github.com/relicloops/phosphor/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![C17](https://img.shields.io/badge/C-C17%20%2F%20POSIX-brightgreen.svg)]()
[![Meson](https://img.shields.io/badge/build-Meson%20%2B%20Ninja-orange.svg)]()
[![Status](https://img.shields.io/badge/status-not%20production%20ready-red.svg)]()

a pure C CLI tool for the relicloops ecosystem. scaffolds projects from
TOML template manifests, builds, serves, generates TLS certificates, and
runs diagnostics.

the name references the phosphor coating inside neon tubes -- the layer that
converts UV discharge into visible light. without phosphor, neon tubes produce
no usable glow.

**CLI commands**
```text
phosphor create   -- scaffold a new project from a template manifest
phosphor glow     -- scaffold a Cathode landing page from embedded template
phosphor build    -- bundle and deploy a Cathode JSX project via esbuild
phosphor serve    -- start neonsignal HTTPS server with optional HTTP redirect
phosphor clean    -- remove build artifacts and stale staging directories
phosphor rm       -- remove a specific path within the project root
phosphor certs    -- generate TLS certificates (local CA or Let's Encrypt ACME)
phosphor doctor   -- run project diagnostics
phosphor filament -- [experimental] reserved for future functionality
phosphor version  -- print version information
phosphor help     -- show help for a command
```

**Quick start**

```bash
git clone https://github.com/relicloops/phosphor.git
cd phosphor
meson setup build
ninja -C build
./build/phosphor version
```

scaffold a project from the embedded template (no external files needed):

```bash
./build/phosphor glow --name=my-site
```

or from an external template:

```bash
./build/phosphor create --name=my-site --template=./templates/cathode-landing
```

**Building**

phosphor uses Meson with the Ninja backend.

prerequisites:
- C compiler with C17 support (GCC 10+ or Clang 14+)
- Meson 1.1+
- Ninja
- CMake (for vendored cmake subprojects)

```bash
meson setup build
ninja -C build
```

the only meson options are `-Dscript_fallback=true` (enables shell-script
fallbacks for build/clean) and `-Ddashboard=false` (disables the ncurses
`phosphor serve` dashboard). vendored dependencies -- toml-c, libgit2,
libarchive, PCRE2, cJSON, libcurl -- are built unconditionally from wraps.
ncurses is the exception: meson resolves it system-first and only falls
back to `subprojects/ncurses.wrap` when no system install is found.

**Serving**

start the neonsignal HTTPS dev server for the current project:

```bash
phosphor serve
```

reads defaults from the `[serve]`, `[deploy]`, and `[certs]` manifest
sections; CLI flags override:

- `www-root` is derived from `[deploy].public_dir` (relative values are
  anchored under `--project`)
- `certs-root` is derived from the `[certs]` section
- the watcher command defaults to a build-on-change helper; override with
  a `[serve].watch_cmd` manifest entry or `--watch-cmd`
- `neonsignal` (required) and `neonsignal_redirect` (optional) are
  preflight-checked against PATH before any child is spawned
- use `--no-dashboard` for raw line-buffered output
- the dashboard skips clean when the manifest lacks enough data to serve

if any authoritative child (neonsignal, or the redirect when enabled)
exits, the remaining siblings are torn down and `phosphor serve` exits
with the worst observed exit code.

**Dashboard**

the default ncurses dashboard is far richer than a scrollable log view:

- multi-panel process view (neonsignal, redirect, watcher) with
  per-panel tabbed streams
- in-panel regex search
- zoom / fullscreen single-panel mode
- start/stop lifecycle controls for any tracked child
- popup help, about, and command reference
- embedded PTY shell with multiple views and screens
- JSON log save/export, plus a fuzzy JSON log picker and line search
- JSON folding and a JSON viewer popup
- dashboard fuzzy search honors `.gitignore` merged with
  `[fuzzy].exclude` from the manifest

**Building Cathode JSX projects**

```bash
phosphor build
```

drives esbuild to bundle a Cathode JSX project and deploys the output to
a `public/` tree. key behaviors:

- auto-runs `npm install` when `node_modules/.bin/esbuild` is missing
- reads `[build].entry` and `[build].defines` from the manifest
- deploy path is resolved through a 4-tier chain:
  1. `--deploy-at=<path>`
  2. `[deploy].public_dir`
  3. first `[[certs.domains]]` name
  4. env-derived fallback `public/<SNI><TLD>`
- copies `src/static/` into the build output
- `--toml` emits a machine-readable TOML build report
- `--clean-first` wipes the deploy target before rebuilding
- deploy targets are checked for containment under the project root
  before any writes happen
- a deprecated legacy shell-script fallback is available only when
  phosphor is compiled with `-Dscript_fallback=true`

**Testing**

the test framework (Ceedling 1.0.1) was removed. 38 test modules are
archived under `docs/source/reference/tests/`. a replacement framework
is pending.

**Template sources**

phosphor supports three template source types:
- **local directory** -- `--template=./path/to/template`
- **remote git** -- `--template=https://github.com/user/repo#branch` (requires libgit2)
- **archive file** -- `--template=./template.tar.gz` (requires libarchive)
- **embedded** -- `phosphor glow` uses the built-in cathode-landing template

supported archive formats: `.tar.gz`, `.tgz`, `.tar.zst`, `.zip`.

optional checksum verification:

```bash
phosphor create --name=my-project --template=./template.tar.gz \
    --checksum=sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

**TLS certificates**

generate a local development CA and leaf certificates:

```bash
phosphor certs --generate
phosphor certs --generate --ca-only         # CA only, skip leaves
phosphor certs --generate --domain=mysite    # only generate for one domain
```

drive the Let's Encrypt ACME HTTP-01 pipeline:

```bash
phosphor certs --letsencrypt --action=request              # request a new certificate
phosphor certs --letsencrypt --action=renew                # renew existing certs
phosphor certs --letsencrypt --action=verify               # verify without issuing
phosphor certs --letsencrypt --action=request --staging    # hit the LE staging endpoint
phosphor certs --letsencrypt --action=request --domain=mysite.com
```

additional flags:

- `--output=<path>` overrides the default cert output tree
- `--project=<path>` lets you run certs from outside the project root
- ACME account keys are managed automatically under `~/.phosphor/acme/`
  (one key per account) and reused across runs

certificate configuration lives in `[certs]` and `[[certs.domains]]`
sections of `template.phosphor.toml`. All cert-related paths
(`dir_name`, `webroot`, `output_dir`, `account_key`) are validated
against the project root and cannot escape it.

**Dependencies**

core (no external libraries):
- C17 standard library
- POSIX.1-2008 APIs

vendored (built automatically, fetched by meson wrap):
- `toml-c` v1.0.0 -- TOML manifest parsing
- `libgit2` v1.9.2 -- remote git template fetching
- `libarchive` v3.8.5 -- archive template support
- `PCRE2` v10.45 -- regex filters in template manifests
- `cJSON` v1.7.18 -- JSON for ACME + dashboard export
- `libcurl` -- ACME HTTP-01 certificate requests
- `ncurses` v6.5 -- serve dashboard TUI (system-first, wrap fallback)

runtime (optional, for `phosphor serve`):
- `neonsignal` -- HTTPS server binary (on PATH)
- `neonsignal_redirect` -- HTTP-to-HTTPS redirect binary (on PATH)

documentation:
- Sphinx (Python)

**Documentation**

sphinx documentation lives in `docs/`:

```bash
sphinx-build -b html docs/source public
```

**Just recipes**

a [justfile](https://github.com/casey/just) wraps common build, test, and
documentation workflows. install just, then run `just` to see available recipes.

prerequisites:
- [just](https://github.com/casey/just) 1.0+

```bash
# list recipes
just

# configure + compile
just setup && just build

# smoke test
just smoke-test
```

**Website**

the phosphor website is under development and will be available at
`phosphor.relicloops.host` once the domain is configured in Cloudflare.

**License**

Apache License 2.0. see `LICENSE` for details.

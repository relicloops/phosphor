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

to disable optional vendored dependencies:

```bash
meson setup build -Dlibgit2=false -Dlibarchive=false -Dpcre2=false
ninja -C build
```

**Serving**

start the neonsignal HTTPS dev server for the current project:

```bash
phosphor serve
```

shows an ncurses dashboard with scrollable panels for each process (neonsignal,
redirect, watcher). reads defaults from the `[serve]` manifest section; CLI
flags override. use `--no-dashboard` for raw output. requires `neonsignal`
and optionally `neonsignal_redirect` on PATH.

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

generate local development certificates (self-signed CA):

```bash
phosphor certs --local
```

request production certificates via Let's Encrypt ACME HTTP-01:

```bash
phosphor certs --letsencrypt --domain=mysite.com
```

certificate configuration lives in `[certs]` and `[[certs.domains]]` sections
of `template.phosphor.toml`.

**Dependencies**

core (no external libraries):
- C17 standard library
- POSIX.1-2008 APIs

vendored (built automatically, fetched by meson wrap):
- `toml-c` v1.0.0 -- TOML manifest parsing (always required)
- `libgit2` v1.9.2 -- remote git template fetching (`-Dlibgit2=true`, default on)
- `libarchive` v3.8.5 -- archive template support (`-Dlibarchive=true`, default on)
- `PCRE2` v10.45 -- regex filters in template manifests (`-Dpcre2=true`, default on)
- `libcurl` -- ACME HTTP-01 certificate requests (`-Dlibcurl=true`, default on)

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

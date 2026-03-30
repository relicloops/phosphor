.. meta::
   :title: phosphor project summary
   :tags: #neonsignal, #phosphor
   :status: active
   :updated: 2026-03-30

.. index::
   single: project summary
   single: roadmap

phosphor -- project summary
============================

what is phosphor
----------------

a pure C CLI tool for the NeonSignal/Cathode ecosystem. scaffolds projects from
template manifests, builds, cleans, and runs diagnostics. the name references the
phosphor coating inside neon tubes -- the layer that converts UV discharge into
visible light. without phosphor, neon tubes produce no usable glow.

CLI contract: ``phosphor create``, ``phosphor build``, ``phosphor clean``,
``phosphor rm``, ``phosphor certs``, ``phosphor doctor``, ``phosphor glow``,
``phosphor serve``, ``phosphor version``, ``phosphor help``.


completed work
--------------

.. list-table::
   :header-rows: 1
   :widths: 55 15 30

   * - Item
     - Status
     - Notes
   * - masterplan authored (``masterplan.[ACTIVE ▸].rst``)
     - [COMPLETED ✓]
     - full spec: CLI contract, EBNF grammar, SoC architecture,
       memory model, template schema, security policy
   * - 35 task files across 6 phases
     - [COMPLETED ✓]
     - each phase directory contains granular task .rst files
       with frontmatter, objectives, acceptance criteria
   * - rename from ``neonex`` to ``phosphor``
     - [COMPLETED ✓]
     - 193 content references replaced, 2 directories renamed,
       2 files renamed. see ``../renaming-neonex-to-phosphor.[COMPLETED ✓].md``
       zero ``neonex`` references remain
   * - phase 0: discovery and contract freeze (5 tasks)
     - [COMPLETED ✓]
     - script inventory, EBNF grammar freeze, exit codes and logging,
       variable mapping, shell compatibility matrix. all deliverables
       filed. 9 grammar issues found and resolved (DR-01..DR-05).
       25 env vars mapped (9 flags, 2 config, 6 deprecated, 8 out-of-scope).
       23 shell operations mapped with 9 intentional changes (IC-01..IC-09)
   * - sphinx documentation setup
     - [COMPLETED ✓]
     - ``tools/phosphor/docs/`` with neon-wave theme, C primary domain.
       frozen references: cli-grammar.rst, exit-codes-and-logging.rst.
       builds cleanly (``make html``)
   * - args-parser abstracted to generic, data-driven CLI library
     - [COMPLETED ✓]
     - ``ph_cmd_def_t`` + ``ph_cli_config_t`` config types.
       parser, spec registry fully data-driven. phosphor-specific
       definitions in ``commands.h`` / ``phosphor_commands.c``.
       see ``abstract-args-parser-into-generic.rst``
   * - Ceedling test infrastructure
     - [REMOVED]
     - Ceedling 1.0.1 was removed due to brittle ``TEST_SOURCE_FILE()``
       dependency tracking. 38 test modules archived under
       ``docs/source/reference/tests/``. setup docs in
       ``reference/ceedling-test-framework.[SKIP-STALE].rst``
   * - phase 1: core library and parser (9 tasks)
     - [COMPLETED ✓]
     - project scaffold, core primitives (alloc/bytes/str/vec/arena/error/log),
       platform layer (POSIX fs/proc/clock), args lexer/parser per frozen EBNF,
       argspec registry, KVP nested parser, semantic validator (UX001-UX007),
       CLI dispatch skeleton (version/help/stubs), 117 unit tests across
       11 test files (all passing). bug fix: kvp.c stack-free crash on
       duplicate key handling
   * - phase 2: create command MVP (9 tasks)
     - [COMPLETED ✓]
     - TOML integration, manifest loader, variable merge pipeline,
       path/fs ops, template engine, staging strategy, create command,
       config loader, integration tests. ~90 new unit tests,
       shell-based integration test suite
   * - phase 3: build compatibility mode (3 tasks)
     - [COMPLETED ✓]
     - proc layer (spawn, env sanitization, wait, 37 unit tests),
       build command (layout validation, script invocation via proc layer,
       exit code mapping, metadata cleanup), deploy guardrails (path escape
       guard, --strict mode, --toml CI output, structured build reports).
       also fixed PH_TYPE_PATH validator to reject absolute paths
   * - phase 4: internalize shell behavior (3 tasks)
     - [COMPLETED ✓]
     - native clean_cmd.c (--stale, --project, --dry-run, --verbose, 7 unit tests),
       native build_cmd.c (esbuild invocation, static asset copy, deploy orchestration,
       metadata skip filter), script fallback feature flag (compile-time
       ``-Dscript_fallback=true``, runtime ``--legacy-scripts`` with deprecation warning)
   * - code coverage infrastructure (gcov + gcovr)
     - [COMPLETED ✓]
     - Ceedling gcov plugin, gcovr HTML reports, CI coverage jobs
       (informative, non-blocking). see ``code-coverage-infrastructure.[COMPLETED ✓].rst``
   * - process management reference doc
     - [COMPLETED ✓]
     - ``reference/process-management.rst`` covering ph_env_t, ph_argv_builder_t,
       ph_proc_exec pipeline, exit code mapping table
   * - argspec var_name fix
     - [COMPLETED ✓]
     - added ``var_name`` field to ``ph_argspec_t`` for flag-to-manifest-variable
       name mapping. fixes silent drop of ``--description`` and ``--github-url``
       in ``phosphor glow``. ``var_merge.c`` resolves via argspec lookup
   * - [deploy] manifest section
     - [COMPLETED ✓]
     - ``ph_deploy_config_t`` with ``public_dir`` field. 4-tier deploy path
       resolution in ``build_cmd.c``: ``--deploy-at`` > ``[deploy]`` >
       ``[[certs.domains]]`` > ``SNI+TLD`` env. auto-populates
       ``__*_PUBLIC_DIR__`` build defines
   * - [serve] manifest section
     - [COMPLETED ✓]
     - ``ph_serve_manifest_config_t`` with ``[serve.neonsignal]`` and
       ``[serve.redirect]`` sub-tables. parsed by ``ph_manifest_load``.
       3-tier resolution in ``serve_cmd.c``: CLI flag > ``[serve]`` manifest
       > ``[deploy]``/``[certs]`` derived > built-in defaults
   * - serve command (reusable library)
     - [COMPLETED ✓]
     - ``src/serve/serve.c`` multi-process spawn/wait/stop library.
       ``src/commands/serve_cmd.c`` CLI layer. spawns neonsignal + redirect
       as background processes with ``fork``/``setpgid``/``execvp``. SIGTERM
       forwarding via process groups. manifest guard: skips when no
       ``[serve]``, ``[deploy]``, or ``[certs]`` in manifest


roadmap -- what comes next
--------------------------

phase 0: discovery and contract freeze [COMPLETED ✓]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

no code. pure spec work that unblocks everything.

- ✓ task 1: script inventory and behavior matrix
- ✓ task 2: freeze EBNF grammar (formal sign-off)
- ✓ task 3: formalize exit codes and logging conventions
- ✓ task 4: map ``global_variables.sh`` vars to phosphor flags
- ✓ task 5: shell compatibility matrix (scripts vs phosphor commands)

phase 1: core library and parser [COMPLETED ✓]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

foundational C code. depends on phase 0.

- ✓ task 1: project scaffold (meson.build, directory tree, compiler flags)
- ✓ task 2: core primitives (alloc, bytes, str, vec, arena, error, log)
- ✓ task 3: platform layer (POSIX fs/proc/clock stubs)
- ✓ task 4: args lexer/parser per frozen EBNF
- ✓ task 5: argspec registry (typed flag resolution)
- ✓ task 6: nested KVP parser (386 lines, full brace nesting + typed scalars)
- ✓ task 7: args validator (UX001-UX007 semantic checks, first-error-wins)
- ✓ task 8: CLI dispatch skeleton (version/help/stubs, full main.c pipeline)
- ✓ task 9: unit tests (117 tests across 11 files, all passing)

phase 2: create command MVP [COMPLETED ✓ → milestone M1]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

first user-facing deliverable. depends on phase 1.

- ✓ task 1: TOML integration (toml-c dependency)
- ✓ task 2: manifest loader (``template.phosphor.toml`` parsing)
- ✓ task 3: variable merge pipeline (CLI → env → config → defaults)
- ✓ task 4: path normalization and filesystem ops
- ✓ task 5: template engine (planner, renderer, transform, writer)
- ✓ task 6: staging directory strategy (atomic rename, EXDEV fallback)
- ✓ task 7: create command (full pipeline: args → manifest → stage → commit)
- ✓ task 8: config loader (project-local config, precedence chain)
- ✓ task 9: integration tests (golden output, failure injection, shell-based)

phase 3: build compatibility mode [COMPLETED ✓ → milestone M2]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

bridge existing shell scripts under ``phosphor build``.

- ✓ task 1: process execution layer (spawn, env sanitization, wait).
  proc.h API with ph_env_t, ph_argv_builder_t, ph_proc_opts_t.
  platform layer extended: envp param, setpgid isolation, SIGINT forwarding.
  37 unit tests across test_wait.c, test_env.c, test_spawn.c
- ✓ task 2: build command (compatibility mode). build_cmd.c validates project
  layout (scripts/_default/all.sh, src/), invokes scripts via ph_proc_exec,
  maps exit codes, cleans metadata. also fixed PH_TYPE_PATH absolute-path
  rejection in validator
- ✓ task 3: deploy guardrails and metadata hygiene. deploy-at path escape
  validation, cleanup_result_t with warning tracking, build_report_t with
  plain/TOML output modes, --strict (warnings become exit 6), --toml
  (machine-readable CI output)

phase 4: internalize shell behavior [COMPLETED ✓ → milestone M3]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

replace shell scripts with C modules.

- ✓ task 1: clean command in C (replace ``clean.sh``).
  clean_cmd.c: native clean pipeline with --stale (staging dir detection),
  --project, --dry-run, --verbose. removes build/ and public/ dirs.
  7 unit tests in test_clean_cmd.c
- ✓ task 2: copy/deploy orchestration in C.
  build_cmd.c rewritten: native esbuild invocation, static asset copying
  via ph_fs_copytree with metadata_skip_filter, deploy via rmtree+copytree.
  no bash dependency for standard build workflow
- ✓ task 3: script fallback feature flag.
  compile-time: ``-Dscript_fallback=true`` (meson_options.txt).
  runtime: ``--legacy-scripts`` flag with deprecation warning.
  both build and clean commands support fallback path

phase 5: remote templates [COMPLETED ✓]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ✓ task 1: optional libgit2 integration for remote fetch.
  vendored libgit2 v1.9.2 via Meson cmake subproject (``subprojects/libgit2.wrap``).
  compile-time toggle ``-Dlibgit2=true`` (default on). ``git_fetch.h`` / ``git_fetch.c``:
  URL parsing (always compiled), libgit2 clone (``#ifdef PHOSPHOR_HAS_LIBGIT2``).
  ``create_cmd.c`` detects URLs, clones to temp dir, feeds local path to existing
  pipeline. graceful degradation when compiled without libgit2.
  21 unit tests in ``test_git_fetch.c`` (URL parsing, cleanup). macOS frameworks
  (Security, CoreFoundation) and iconv linked explicitly for static libgit2.
  CI: default build includes libgit2, separate ``build-no-libgit2`` job
- ✓ task 2: libarchive integration, checksum verification.
  vendored libarchive v3.8.5 via Meson cmake subproject (``subprojects/libarchive.wrap``).
  compile-time toggle ``-Dlibarchive=true`` (default on). ``archive.h`` / ``archive.c``:
  format detection (always compiled), extraction (``#ifdef PHOSPHOR_HAS_LIBARCHIVE``).
  SHA256 checksum verification via ``sha256.h`` / ``sha256.c`` (public-domain, zero deps).
  ``create_cmd.c`` detects archive extensions (.tar.gz, .tar.zst, .zip), extracts to temp
  dir, feeds local path to existing pipeline. zip/tar slip prevention.
  ``--checksum=sha256:<hex>`` flag for integrity verification.
  unit tests in ``test_sha256.c`` and ``test_archive.c``.
  CI: default build includes libarchive, separate ``build-no-libarchive`` job
- ✓ task 3: PCRE2 integration and filter activation.
  vendored PCRE2 v10.45 via Meson cmake subproject (``subprojects/pcre2.wrap``).
  compile-time toggle ``-Dpcre2=true`` (default on). ``regex.h`` / ``regex.c``:
  availability check (always compiled), compile/match/destroy (``#ifdef PHOSPHOR_HAS_PCRE2``).
  manifest schema extended with ``exclude_regex`` and ``deny_regex`` arrays in ``[filters]``.
  filter infrastructure activated: ``writer.c`` wires glob excludes, regex excludes,
  metadata deny, glob deny, and regex deny into the execution pipeline.
  exclude patterns silently skip files in copytree callback; deny patterns hard-error (exit 6)
  before COPY/RENDER operations. graceful degradation when compiled without PCRE2 (glob
  filters still work, regex arrays rejected at manifest load).
  61 unit tests in ``test_regex.c`` (availability, compile, match, destroy, Unicode,
  lookahead/lookbehind, manifest filter patterns, edge cases).
  CI: default build includes pcre2, separate ``build-no-pcre2`` job

certs command [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^

- local CA + leaf certificate generation via openssl CLI
- ACME HTTP-01 (Let's Encrypt) flow: account registration, order creation,
  challenge response, CSR finalization, certificate download
- ``--staging`` flag for LE staging endpoint
- ``--action=verify`` cert expiry check (openssl x509 -checkend)
- production-tested against Let's Encrypt staging + production
- 23 unit tests (config + SAN parsing)

rm command [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^

- targeted file/directory removal within project root
- manifest guard: requires ``--force`` when no phosphor manifest found
- path traversal prevention (rejects ``..`` and absolute paths)
- escape guard: target must stay within project root

doctor command [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^^

- project diagnostics: manifest detection, tool availability (openssl, esbuild,
  neonsignal, neonsignal_redirect), node deps (package.json, node_modules),
  build state, stale staging dirs, cert status with expiry checking

code hygiene (v0.0.0-017 polish) [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- extracted ``ph_args_get_flag`` / ``ph_args_has_flag`` to public API
  (``args_helpers.c``), removed 5 static copies
- extracted ``json_extract_string`` / ``json_extract_string_array`` to shared
  ``acme_json.c``, removed 4 static copies
- ``ph_acme_base64url_encode`` moved outside ``#ifdef PHOSPHOR_HAS_LIBCURL``
  guard for testability
- 10 base64url unit tests (RFC 4648 vectors, padding edge cases)
- 16 ACME JSON extraction unit tests

serve command [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^

- reusable multi-process library: ``src/serve/serve.c`` + ``include/phosphor/serve.h``
- spawns neonsignal + neonsignal_redirect as background processes
- ``fork``/``setpgid``/``execvp`` with process group isolation
- SIGTERM forwarding on Ctrl+C via ``waitpid`` loop
- manifest ``[serve]`` section with ``[serve.neonsignal]`` and ``[serve.redirect]``
  sub-tables provides defaults; CLI flags override
- 3-tier resolution: CLI flag > ``[serve]`` manifest > ``[deploy]``/``[certs]`` derived
- manifest guard: skips when no config source found
- 20 CLI flags covering all neonsignal and redirect spin arguments
- ``phosphor help serve`` auto-generated from argspec descriptions
- IPv4/IPv6 host address validation (``inet_pton``)
- privileged port warnings (port < 1024)
- startup banner with OSC 8 clickable HTTPS URL, bind address, and port
- ``--watch`` flag + ``[serve.neonsignal] watch`` manifest option: spawns
  file watcher as third child process (default: ``node scripts/_default/build.mjs --watch``)
- ncurses dashboard TUI: pipe-captured child output displayed in scrollable
  panels. color-coded per-process (cyan/green/yellow). keyboard navigation
  (Tab focus, Up/Down scroll, q quit). auto-layout side-by-side or stacked.
  SIGWINCH terminal resize support. ``--no-dashboard`` flag to fall back to
  raw output. compile-time ``-Ddashboard=true`` option

dashboard library [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- reusable ncurses serve dashboard: ``include/phosphor/dashboard.h`` +
  ``src/dashboard/dashboard.c``
- ring buffer (2000 lines per panel) with stdout/stderr color differentiation
- ``poll()``-based event loop: multiplexes pipe fds + signal self-pipe +
  ncurses keyboard input
- color pairs: cyan (neonsignal), green (redirect), yellow (watcher), red
  (stderr), white-on-black status bar
- SIGWINCH handling via self-pipe trick (async-signal-safe)
- child process reaping with ``waitpid(WNOHANG)``
- graceful shutdown: SIGTERM forwarding to process groups on quit
- vendored ncurses via ``subprojects/ncurses.wrap`` (fallback when system
  ncurses unavailable)

terminal features library [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- reusable terminal output library: ``include/phosphor/term.h`` + ``src/core/term.c``
- OSC 8 clickable hyperlinks (``ph_term_link``, ``ph_term_linkf``)
- labeled key-value output (``ph_term_kv``, ``ph_term_kvf``, ``ph_term_kv_link``)
- graceful TTY fallback via ``ph_color_enabled()`` -- plain text when redirected
- used by ``serve_cmd.c`` startup banner

[deploy] manifest section [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``ph_deploy_config_t`` struct with ``public_dir`` field
- 4-tier deploy path resolution in ``build_cmd.c``:
  ``--deploy-at`` > ``[deploy]`` > ``[[certs.domains]]`` > ``SNI+TLD`` env
- auto-populates ``__*_PUBLIC_DIR__`` build defines from resolved path

argspec var_name mapping fix [COMPLETED]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- added ``const char *var_name`` field to ``ph_argspec_t``
- ``var_merge.c`` looks up argspec by ``var_name`` to find the correct CLI
  flag name, fixing silent drop of ``--description`` and ``--github-url``
  in glow command where flag names did not match manifest variable names

phase 6: stabilization [DEFERRED -- milestone M4]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- task 1: CI matrix (macOS + Linux, sanitizer jobs)
- task 2: packaging (Homebrew formula, deb/rpm)
- task 3: template author documentation and playbook

glow command -- embedded cathode-landing template [COMPLETED ✓]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

embedded the cathode-landing template (~40 files, ~60 KB) as C static byte
arrays in the phosphor binary. ``phosphor glow --name <project-name>`` scaffolds
a Cathode landing page from the embedded template using the existing create
pipeline. zero external dependencies. guarded by ``PHOSPHOR_HAS_EMBEDDED``
compile-time flag.

supersedes ``phase-init/`` (renamed ``init`` to ``glow``).

- task 1: template preparation (verify ``<<placeholder>>`` coverage) [DONE]
- task 2: C buffer embedding (code generation, meson custom_target) [DONE]
- task 3: glow command implementation (``glow_cmd.c``, virtual source adapter) [DONE]
- task 4: tests and cleanup [DONE -- note: Ceedling removed, tests archived]

see ``glow-command-embedded-template.[COMPLETED ✓].rst``

embedded build toolchain -- replace esbuild [DRAFT ○ -- milestone 1.0.0-000]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

replace external esbuild dependency with an embedded build/bundle capability.
after this milestone, ``phosphor build`` produces production-ready output
without Node.js, npm, or any external bundler. phosphor becomes a fully
self-contained tool.

approaches under evaluation:

- (A) custom minimal C bundler for the Cathode JSX dialect
- (B) embedded QuickJS engine running JS transforms (~600 KB)
- (C) esbuild WASM via embedded WASM runtime (likely too heavy)
- (D) tree-sitter parser + custom C codegen (hybrid)

see ``embedded-build-toolchain.[DRAFT ○].rst``

verbose flag implementation [DRAFT ○]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

wire the ``--verbose`` flag across all commands. audited 2026-03-19: create and
glow now have full coverage (7 debug calls each). build has partial coverage (6
calls, missing esbuild argv/npm/env). clean and rm have token coverage (1 call
each). certs and doctor enable debug level but emit zero debug messages.

- task 1: wire verbose in create and glow [DONE]
- task 2: improve verbose in build (esbuild argv, defines, env, npm install)
- task 3: improve verbose in clean, rm, certs, doctor (certs and doctor have
  zero debug calls despite enabling debug level)
- task 4: remove ``(reserved)`` from verbose descriptions in argspecs [DONE]

see ``verbose-flag-implementation.[DRAFT ○].rst``

SoC audit -- header placement and JSON library consolidation [DRAFT ○]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

fix structural inconsistencies accumulated during rapid feature development.
``src/certs/acme_json.h`` is the only internal header in ``src/`` -- breaks
the convention that all public/shared headers live in ``include/phosphor/``.
the hand-rolled JSON extraction (``strstr`` + manual quote scanning) has no
escape handling, no Unicode support, and silent failure on malformed input.

- task 1: vendor cJSON (MIT, ~2 KLOC, single .c/.h pair)
- task 2: create ``ph_json_*`` API in ``include/phosphor/json.h``
- task 3: migrate ACME module (4 files) to new API
- task 4: remove ``acme_json.h``/``acme_json.c``, update tests

see ``soc-audit-json-consolidation.[DRAFT ○].rst``


key technical decisions
-----------------------

- **language**: pure C (C17 standard)
- **build system**: Meson (Ninja backend)
- **memory**: explicit ownership, arena allocators, ASan/Valgrind verified
- **dependencies**: toml-c (tier 1), libarchive (tier 1), PCRE2 (tier 1), libgit2 (tier 2, optional)
- **testing**: removed -- Ceedling 1.0.1 was removed (brittle dependency tracking), waiting for replacement
- **platforms**: macOS + Linux (Windows deferred)
- **concurrency**: single-threaded v1 (thread pool deferred to v2)
- **config format**: ``template.phosphor.toml`` with versioned schema
- **placeholder syntax**: ``<<var_name>>`` (avoids shell/env collision)
- **env var prefix**: ``PHOSPHOR_*``
- **staging**: ``.phosphor-staging-<pid>-<timestamp>``


file index
----------

plans:

.. code-block:: text

   plans/
   ├── index.[ACTIVE ▸].rst
   ├── summary.[ACTIVE ▸].rst                           ← this file
   ├── masterplan.[ACTIVE ▸].rst                        ← full specification
   ├── abstract-args-parser-into-generic.[COMPLETED ✓].rst
   ├── versioning-and-ci-pipeline.[COMPLETED ✓].rst
   ├── testing-infrastructure-ceedling.[COMPLETED ✓].rst
   ├── code-coverage-infrastructure.[COMPLETED ✓].rst
   ├── glow-command-embedded-template.[COMPLETED ✓].rst
   ├── soc-audit-json-consolidation.[DRAFT ○].rst
   ├── phase-0/
   │   ├── index.[COMPLETED ✓].rst
   │   ├── task-1-script-inventory.[COMPLETED ✓].rst
   │   ├── deliverable-task-1-script-inventory.[COMPLETED ✓].rst
   │   ├── task-2-cli-grammar-freeze.[COMPLETED ✓].rst
   │   ├── deliverable-task-2-cli-grammar-freeze.[COMPLETED ✓].rst
   │   ├── task-3-exit-codes-and-logging.[COMPLETED ✓].rst
   │   ├── deliverable-task-3-exit-codes-and-logging.[COMPLETED ✓].rst
   │   ├── task-4-variable-mapping.[COMPLETED ✓].rst
   │   ├── deliverable-task-4-variable-mapping.[COMPLETED ✓].rst
   │   ├── task-5-shell-compat-matrix.[COMPLETED ✓].rst
   │   └── deliverable-task-5-shell-compat-matrix.[COMPLETED ✓].rst
   ├── phase-1/
   │   ├── index.[COMPLETED ✓].rst
   │   ├── task-1-project-scaffold.[COMPLETED ✓].rst
   │   ├── task-2-core-primitives.[COMPLETED ✓].rst
   │   ├── task-3-platform-layer.[COMPLETED ✓].rst
   │   ├── task-4-args-lexer-parser.[COMPLETED ✓].rst
   │   ├── task-5-argspec-registry.[COMPLETED ✓].rst
   │   ├── task-6-kvp-parser.[COMPLETED ✓].rst
   │   ├── task-7-args-validator.[COMPLETED ✓].rst
   │   ├── task-8-cli-dispatch.[COMPLETED ✓].rst
   │   └── task-9-unit-tests-core-parser.[COMPLETED ✓].rst
   ├── phase-2/
   │   ├── index.[COMPLETED ✓].rst
   │   ├── task-1-toml-integration.[COMPLETED ✓].rst
   │   ├── task-2-manifest-loader.[COMPLETED ✓].rst
   │   ├── task-3-variable-merge.[COMPLETED ✓].rst
   │   ├── task-4-path-and-fs.[COMPLETED ✓].rst
   │   ├── task-5-template-engine.[COMPLETED ✓].rst
   │   ├── task-6-staging-strategy.[COMPLETED ✓].rst
   │   ├── task-7-create-command.[COMPLETED ✓].rst
   │   ├── task-8-config-loader.[COMPLETED ✓].rst
   │   └── task-9-integration-tests.[COMPLETED ✓].rst
   ├── phase-3/
   │   ├── index.[COMPLETED ✓].rst
   │   ├── task-1-proc-layer.[COMPLETED ✓].rst
   │   ├── task-2-build-command.[COMPLETED ✓].rst
   │   └── task-3-deploy-and-hygiene.[COMPLETED ✓].rst
   ├── phase-4/
   │   ├── index.[COMPLETED ✓].rst
   │   ├── task-1-clean-command.[COMPLETED ✓].rst
   │   ├── task-2-copy-deploy-in-c.[COMPLETED ✓].rst
   │   └── task-3-script-fallback-flag.[COMPLETED ✓].rst
   ├── phase-5/
   │   ├── index.[COMPLETED ✓].rst
   │   ├── task-1-libgit2-source.[COMPLETED ✓].rst
   │   ├── task-2-archive-support.[COMPLETED ✓].rst
   │   └── task-3-extended-schema.[COMPLETED ✓].rst
   ├── phase-6/
   │   ├── index.[DEFERRED ⋯].rst
   │   ├── task-1-ci-matrix.[DEFERRED ⋯].rst
   │   ├── task-2-packaging.[DEFERRED ⋯].rst
   │   └── task-3-docs-playbook.[DEFERRED ⋯].rst
   └── phase-init/
       ├── index.[DEFERRED ⋯].rst
       ├── task-1-template-preparation.[DRAFT ○].rst
       ├── task-2-c-buffer-embedding.[DRAFT ○].rst
       ├── task-3-init-command.[DRAFT ○].rst
       └── task-4-remote-yaml-templates.[DRAFT ○].rst

sphinx documentation (``tools/phosphor/docs/``):

.. code-block:: text

   tools/phosphor/docs/
   ├── Makefile                              # make html / make open / make livehtml
   ├── requirements.txt                      # pinned python dependencies
   ├── .venv/                                # python virtual environment
   ├── source/
   │   ├── conf.py                           # sphinx config (neon-wave theme, C domain)
   │   ├── index.rst                         # documentation index
   │   └── reference/                        # frozen spec documents
   │       ├── cli-grammar.rst               ← frozen EBNF grammar (task-2 output)
   │       ├── exit-codes-and-logging.rst    ← frozen exit codes + logging (task-3 output)
   │       ├── template-manifest-schema.rst  ← template.phosphor.toml schema (phase-2 output)
   │       └── process-management.rst        ← proc layer API reference (phase-3 output)
   └── build/html/                           # generated HTML output

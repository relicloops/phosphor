.. meta::
   :title: phosphor project audit
   :tags: #neonsignal, #phosphor
   :status: active
   :updated: 2026-03-20

.. index::
   single: audit
   single: code review
   single: project health
   pair: audit; codebase

phosphor -- project audit (2026-03-20)
=======================================

executive summary
-----------------

phosphor is a pure C17 CLI tool for the NeonSignal/Cathode ecosystem. it
scaffolds projects from TOML template manifests, builds via esbuild, cleans
artifacts, manages TLS certificates (local CA + ACME/Let's Encrypt), and runs
project diagnostics.

.. list-table::
   :widths: 25 75

   * - version
     - ``0.0.1-021``
   * - language
     - C17 (``-std=c17 -pedantic -Wall -Wextra``)
   * - build system
     - Meson 1.1+ / Ninja
   * - license
     - Apache 2.0
   * - platforms
     - macOS (arm64), Linux (x86_64, arm64)
   * - source
     - 12,534 lines across 63 ``.c`` files
   * - headers
     - 1,926 lines across 26 ``.h`` files
   * - unit tests
     - 531 tests across 38 test files
   * - integration tests
     - 2 shell scripts (12 test cases)

**maturity**: solid pre-1.0 codebase. core infrastructure (memory, parsing,
template engine, I/O, process management) is well-tested and production-quality.
certificate management and the glow command are newer and less tested.

CLI contract: ``phosphor create``, ``phosphor build``, ``phosphor clean``,
``phosphor rm``, ``phosphor certs``, ``phosphor doctor``, ``phosphor glow``,
``phosphor version``, ``phosphor help``.


architecture review
-------------------

module breakdown
^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 20 8 8 64

   * - module
     - files
     - lines
     - purpose
   * - ``src/commands/``
     - 8
     - 3,397
     - command handlers: create, build, clean, rm, doctor, glow, certs
   * - ``src/certs/``
     - 12
     - 2,537
     - ACME/Let's Encrypt automation, local CA, leaf cert generation
   * - ``src/template/``
     - 8
     - 2,127
     - template engine: manifest loading, variable merge, staging,
       planner, renderer, transform, writer, embedded registry
   * - ``src/io/``
     - 7
     - 1,254
     - file I/O, path normalization, libgit2 clone, libarchive extraction
   * - ``src/args-parser/``
     - 6
     - 1,146
     - CLI argument lexer, parser, KVP nested parser, spec registry,
       semantic validator, helper utilities
   * - ``src/core/``
     - 10
     - 993
     - memory (alloc/arena), data structures (str/vec/bytes), error
       chain, logging, config, color output, PCRE2 regex wrapper
   * - ``src/proc/``
     - 3
     - 307
     - process spawning with argv builder, wait, environment utilities
   * - ``src/crypto/``
     - 1
     - 228
     - SHA256 file hashing and verification (public-domain, zero deps)
   * - ``src/cli/``
     - 3
     - 175
     - CLI dispatch (command routing), colorized help, version output
   * - ``src/platform/``
     - 4
     - 370
     - platform abstraction: signal handling, POSIX fs/proc/clock

entry point flow
^^^^^^^^^^^^^^^^

``main.c`` (55 lines):

1. initialize color output (respects ``NO_COLOR``, ``FORCE_COLOR``)
2. install signal handlers (``SIGINT``, ``SIGTERM``)
3. lexer: ``argv`` -> token stream
4. parser: tokens -> parsed args (command + flags)
5. CLI dispatcher: route to command handler
6. return exit code

clean, minimal entry point. no global state beyond the error chain.

dependency graph
^^^^^^^^^^^^^^^^

.. code-block:: text

   phosphor binary
   +-- toml-c v1.0.0 (MIT)          mandatory -- TOML manifest parsing
   +-- libgit2 v1.9.2 (GPL-2.0)     optional  -- remote git template cloning
   +-- libarchive v3.8.5 (BSD)       optional  -- tar.gz/tar.zst/zip extraction
   +-- pcre2 v10.45 (BSD)            optional  -- regex filters in manifests
   +-- curl v8.12.1 (MIT-like)       optional  -- ACME HTTP requests
   +-- system: Security, CoreFoundation, iconv (macOS, for libgit2)
   +-- system: zlib, zstd (for libarchive)
   +-- system: SystemConfiguration (macOS, for curl proxy detection)

all optional dependencies are compile-time toggles:

- ``-Dlibgit2=true`` (default on)
- ``-Dlibarchive=true`` (default on)
- ``-Dpcre2=true`` (default on)
- ``-Dlibcurl=true`` (default on)
- ``-Dscript_fallback=true`` (default off)


code quality
------------

strengths
^^^^^^^^^

**memory safety**

- consistent ``ph_alloc()`` / ``ph_free()`` / ``ph_realloc()`` / ``ph_calloc()``
  wrappers -- no direct ``malloc``/``free`` in application code
- debug mode with ``0xCAFEBABE`` canary bytes for heap overflow detection
  (``alloc.c``)
- arena allocator for bulk allocations with single-free cleanup
  (``arena.c``)

**error handling**

- first-error-wins pattern with ``ph_error_t`` chain structure
- consistent ``PH_OK``/``PH_ERR`` return codes (920+ occurrences)
- error propagation with category, subcode, message, and cause tracking
- 8 exit codes with formal taxonomy (documented in
  ``reference/exit-codes-and-logging.rst``)

**naming conventions**

- ``ph_`` prefix for all public API symbols
- ``snake_case`` throughout (types, functions, variables)
- static functions properly file-scoped
- header guards with uppercase ``PHOSPHOR_`` prefix

**include organization**

- all public headers use ``#include "phosphor/..."`` format
- standard library includes appear after project headers
- single convention violation: ``src/certs/acme_json.h`` (see findings)

**logging**

- 200+ structured log calls (``ph_log_error``, ``ph_log_warn``,
  ``ph_log_info``, ``ph_log_debug``)
- no direct ``printf``/``fprintf`` for error reporting in application code
- configurable log levels with ``--verbose`` flag support

**build system**

- deterministic builds with Meson custom_target for code generation
- all subprojects vendored with ``.wrap`` files and ``.wraplock``
- compile flags: ``-pedantic -Wall -Wextra`` with zero warnings

concerns
^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 10 30 60

   * - severity
     - issue
     - details
   * - MEDIUM
     - hardcoded ``/tmp/`` paths
     - 5 temporary files in ``acme_jws.c`` (input, sig, modout, jwk, hash),
       2 in ``acme_finalize.c`` (csr, cnf), 1 in ``glow_cmd.c`` (temp dir).
       should use ``mkdtemp()`` or platform tempdir abstraction for
       cross-platform safety and parallel execution safety
   * - MEDIUM
     - naive JSON parsing
     - ``acme_json.c`` uses ``strstr()`` + manual character scanning.
       no escape sequence handling (``\"`` breaks extraction), no Unicode
       support, no nested object traversal, silent data loss on malformed
       input. acceptable for constrained ACME responses but fragile.
       remediation plan exists: ``soc-audit-json-consolidation.[DRAFT ○].rst``
   * - LOW
     - header placement violation
     - ``src/certs/acme_json.h`` is the only internal header in ``src/``.
       every other module exposes API through ``include/phosphor/``
       exclusively. remediation plan exists: same SoC audit
   * - LOW
     - sparse logging in args-parser
     - 1,146 lines across 6 files with 0 log calls. error messages
       are returned but not logged, making parse failures hard to diagnose
       without ``--verbose``
   * - LOW
     - unprefixed function names
     - ``json_extract_string`` and ``json_extract_string_array`` in
       ``acme_json.c`` lack the ``ph_`` prefix. risk of symbol collision
       with vendored libraries
   * - INFO
     - POSIX-only platform support
     - ``mkdtemp()``, ``fork()``, POSIX signal handling. no Windows support.
       expected for current scope but worth noting for future portability


test coverage
-------------

unit tests
^^^^^^^^^^

531 tests across 38 test files via Ceedling 1.0.1 (Unity + CMock + CException).

top 15 most heavily tested modules:

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - test file
     - tests
     - source module
   * - ``test_regex.c``
     - 61
     - PCRE2 regex wrapper (compile/match/destroy, Unicode, lookahead)
   * - ``test_path_norm.c``
     - 30
     - path normalization (relative, absolute, edge cases)
   * - ``test_manifest.c``
     - 25
     - TOML manifest parsing (variables, ops, filters)
   * - ``test_writer.c``
     - 24
     - template writer (atomic write, copy, render, permissions)
   * - ``test_git_fetch.c``
     - 21
     - URL parsing, libgit2 clone cleanup
   * - ``test_kvp.c``
     - 18
     - nested key-value parser (brace nesting, typed scalars)
   * - ``test_var_merge.c``
     - 17
     - variable merge pipeline (CLI -> env -> config -> defaults)
   * - ``test_acme_json.c``
     - 16
     - ACME JSON string/array extraction
   * - ``test_validate.c``
     - 15
     - argument semantic validation (UX001-UX007)
   * - ``test_spawn.c``
     - 15
     - process spawning (argv builder, exec, signal safety)
   * - ``test_create_cmd.c``
     - 15
     - create command pipeline
   * - ``test_archive.c``
     - 15
     - archive extraction (tar.gz, tar.zst, zip, path traversal)
   * - ``test_wait.c``
     - 14
     - process wait and exit code mapping
   * - ``test_certs_config.c``
     - 14
     - certificate config parsing + SAN handling
   * - ``test_bytes.c``
     - 14
     - byte buffer operations

integration tests
^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 10 55

   * - script
     - cases
     - coverage
   * - ``test_create_golden.sh``
     - 7
     - golden output, determinism, dry-run, force overwrite, missing
       ``--name`` (exit 2), missing template (exit 4), invalid manifest
       (exit 3)
   * - ``test_glow_golden.sh``
     - 5
     - golden output (30+ files), name substitution in
       ``package.json``/``index.html``/``Header.tsx``, dry-run, force
       overwrite, missing ``--name`` (exit 2), verbose output

test gaps
^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 10 55

   * - source file
     - lines
     - status
   * - ``src/certs/acme_jws.c``
     - 436
     - no unit tests (JWS signing, OpenSSL wrapper)
   * - ``src/certs/acme_finalize.c``
     - 294
     - no unit tests (CSR finalization, cert retrieval)
   * - ``src/certs/acme_challenge.c``
     - 286
     - no unit tests (ACME HTTP-01 challenge)
   * - ``src/certs/acme_http.c``
     - 248
     - no unit tests (libcurl HTTP requests)
   * - ``src/certs/acme_account.c``
     - 208
     - no unit tests (account registration)
   * - ``src/certs/acme_order.c``
     - 143
     - no unit tests (order lifecycle)
   * - ``src/commands/glow_cmd.c``
     - 397
     - integration test only; no unit tests
   * - ``src/template/embedded_registry.c``
     - 39
     - no tests (simple lookup table)
   * - ``src/platform/posix/clock_posix.c``
     - 62
     - no unit tests (thin POSIX wrapper)

**total untested ACME code**: 1,615 lines across 6 modules. these are
difficult to unit-test in isolation (require OpenSSL, libcurl, network
mocking). the certs command has been production-tested against Let's Encrypt
staging and production endpoints but lacks automated regression coverage.


CI/CD pipeline
--------------

5-job GitHub Actions workflow:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - job
     - platform
     - what it does
   * - ``build``
     - macOS + Ubuntu
     - Meson setup, Ninja compile, smoke test (``phosphor version``)
   * - ``unit-tests``
     - Ubuntu
     - Ceedling ``test:all`` (531 tests). includes PCRE2 static link
       workaround (thin archive repack + ``--whole-archive``)
   * - ``integration``
     - macOS
     - ``test_create_golden.sh``, ``test_glow_golden.sh``
   * - ``release-build``
     - 3 targets
     - macos-arm64, linux-x86_64, linux-arm64. release ``-O2``
       compilation with static linking verification. packages as
       ``.tar.gz`` with SHA256 checksums
   * - ``release``
     - (depends all)
     - GitHub Release creation via ``git-cliff`` release notes.
       uploads tarballs + checksums as release artifacts.
       triggered on ``refs/tags/v*``

**test distribution**: unit tests run on Linux (Ubuntu), integration tests
run on macOS (where primary development happens). cross-platform compilation
verified on both.


dependency audit
----------------

.. list-table::
   :header-rows: 1
   :widths: 18 12 12 12 46

   * - library
     - version
     - license
     - size
     - notes
   * - toml-c
     - 1.0.0
     - MIT
     - ~3 KLOC
     - mandatory. vendored in ``subprojects/toml-c/`` with custom
       ``meson.build``. TOML manifest parsing
   * - libgit2
     - 1.9.2
     - GPL-2.0 + linking exception
     - ~200 KLOC
     - optional. ``.wrap`` file, shallow clone. remote git template
       cloning. macOS needs Security, CoreFoundation, iconv
   * - libarchive
     - 3.8.5
     - BSD-2-Clause
     - ~150 KLOC
     - optional. ``.wrap`` file. tar.gz, tar.zst, zip extraction.
       needs system zlib + zstd
   * - pcre2
     - 10.45
     - BSD-3-Clause
     - ~120 KLOC
     - optional. ``.wrap`` file. regex filters in template manifests
   * - curl
     - 8.12.1
     - curl license (MIT-like)
     - ~250 KLOC
     - optional. ``.wrap`` file. ACME HTTP requests for Let's Encrypt.
       **note**: not yet listed in CLAUDE.md dependencies section

**license compliance**: all vendored libraries are permissive (MIT, BSD) or
have linking exceptions (libgit2). compatible with Apache 2.0 project license.

**observation**: curl dependency is present in ``subprojects/curl.wrap`` and
used by the ACME module but is not documented in CLAUDE.md's Dependencies
section. this should be corrected.


documentation
-------------

**sphinx configuration**: neon-wave theme, C primary domain, MyST parser
for Markdown support. extensions: intersphinx, viewcode, copybutton,
sphinx_design.

reference documents (6 files, 1,682 lines total):

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - document
     - content
   * - ``cli-grammar.rst``
     - ISO 14977 EBNF grammar with 5 disambiguation rules
   * - ``exit-codes-and-logging.rst``
     - exit code taxonomy (0-8), diagnostic subcodes, logging conventions
   * - ``template-manifest-schema.rst``
     - ``template.phosphor.toml`` v1 schema (variables, ops, filters, hooks)
   * - ``process-management.rst``
     - proc layer API: ``ph_env_t``, ``ph_argv_builder_t``, ``ph_proc_exec``
   * - ``coverage.rst``
     - gcov/gcovr HTML report reference, thresholds
   * - ``changelog.rst``
     - git-cliff generated changelog

**plan system**: 35 task files across 6 phases + 4 feature plans. custom
status markers in filenames (``[COMPLETED ✓]``, ``[ACTIVE ▸]``, ``[DRAFT ○]``,
``[DEFERRED ⋯]``). all plan files use RST with ``.. meta::`` frontmatter.

**coverage reports**: gcovr HTML reports in ``docs/source/coverage/`` (48
files). thresholds: 90% high, 75% medium.


configuration consistency
-------------------------

.. list-table::
   :header-rows: 1
   :widths: 35 20 15 30

   * - location
     - variable
     - value
     - status
   * - ``meson.build``
     - ``phosphor_version``
     - ``0.0.1-021``
     - authoritative source
   * - ``meson.build``
     - ``project(version)``
     - ``0.0.1``
     - OK (no build suffix per Meson convention)
   * - ``docs/source/conf.py``
     - ``release``
     - ``0.0.1-021``
     - in sync
   * - ``project.yml``
     - compile flags
     - ``0.0.1-021``
     - in sync
   * - ``justfile``
     - ``VERSION``
     - ``0.0.1-021``
     - in sync

**verdict**: all 5 version locations are synchronized. no drift detected.


security considerations
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 15 25 60

   * - severity
     - area
     - details
   * - MEDIUM
     - ACME temp files
     - 7 hardcoded ``/tmp/`` paths in ACME code (``acme_jws.c``,
       ``acme_finalize.c``). predictable filenames in shared ``/tmp/``
       create symlink attack surface. should use ``mkdtemp()`` with
       random directory names
   * - MEDIUM
     - JSON parsing
     - hand-rolled ``strstr()`` JSON extraction in ``acme_json.c``
       processes ACME server responses. no escape handling means a
       crafted response containing ``\"`` could cause incorrect
       parsing. mitigated by TLS transport and Let's Encrypt's
       well-formed responses
   * - LOW
     - string operations
     - 56 instances of ``memcpy``/``strcpy``/``sprintf`` across
       codebase. most use ``snprintf`` with bounds checking. no
       obvious buffer overflow paths identified but warrants periodic
       review
   * - LOW
     - path traversal
     - template extraction includes zip/tar slip prevention in
       ``archive.c``. create command validates paths. ``rm`` command
       rejects ``..`` and absolute paths. these guards appear
       comprehensive
   * - OK
     - memory safety
     - ``ph_alloc`` wrappers with debug canary mode. arena allocator
       for bulk ops. no raw ``malloc``/``free`` in application code.
       ASan/Valgrind compatible
   * - OK
     - process isolation
     - ``spawn.c`` uses ``setpgid`` for child process group isolation.
       SIGINT forwarding. environment sanitization in ``env.c``


findings and recommendations
-----------------------------

priority 1 -- commit uncommitted work
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

19 files modified/added in the working tree implementing the glow command
(tasks 1-3), verbose wiring for create/glow, plan status updates, and the
glow integration test. this is a significant feature that should be committed
before further work.

priority 2 -- ACME temp file hardening
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

replace 7 hardcoded ``/tmp/`` paths with ``mkdtemp()``-based temporary
directories. this eliminates the symlink attack surface and makes the code
safe for parallel execution. estimated effort: small (one file, pattern
replacement).

priority 3 -- JSON library consolidation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

vendor cJSON and replace the naive ``strstr()`` JSON extraction. this fixes
the escape handling vulnerability, the unprefixed function names, and the
convention violation (header in ``src/``). plan exists:
``soc-audit-json-consolidation.[DRAFT ○].rst``.

priority 4 -- ACME module test coverage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1,615 lines across 6 ACME modules have no unit tests. while difficult to
test in isolation (OpenSSL, libcurl dependencies), the following are
testable without mocking:

- JWS header construction and base64url encoding (already partially tested)
- CSR attribute building
- challenge token validation
- order URL parsing

consider extracting pure-logic functions from the ACME modules into testable
units.

priority 5 -- verbose flag completion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``certs`` and ``doctor`` commands set ``PH_LOG_DEBUG`` level but emit zero
debug messages. ``build``, ``clean``, and ``rm`` have partial coverage.
plan exists: ``verbose-flag-implementation.[DRAFT ○].rst``.

priority 6 -- documentation updates
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- add curl v8.12.1 to CLAUDE.md Dependencies section
- update ``docs/source/index.rst`` CLI contract to include ``rm``, ``certs``,
  ``glow`` commands
- update CLAUDE.md project structure to reflect ``src/certs/`` directory
  and new files (``glow_cmd.c``, ``embedded_registry.c``, ``embedded.h``)
- fix ``.clangd`` file which contains a hardcoded absolute path

priority 7 -- embedded build toolchain (milestone 1.0.0-000)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

replace external esbuild dependency. still at brainstorming stage with 4
approaches under evaluation. plan exists:
``embedded-build-toolchain.[DRAFT ○].rst``.


appendix: file inventory
------------------------

.. code-block:: text

   src/
   +-- main.c                                    55 lines
   +-- cli/
   |   +-- cli_dispatch.c                        30 lines
   |   +-- cli_help.c                           110 lines
   |   +-- cli_version.c                         14 lines
   +-- commands/
   |   +-- phosphor_commands.c                  (argspec tables, CLI config)
   |   +-- create_cmd.c                         (template scaffolding)
   |   +-- build_cmd.c                          (esbuild invocation, deploy)
   |   +-- clean_cmd.c                          (artifact cleanup)
   |   +-- rm_cmd.c                             (targeted file removal)
   |   +-- doctor_cmd.c                         (project diagnostics)
   |   +-- glow_cmd.c                           (embedded template scaffolding)
   |   +-- certs_cmd.c                          (certificate orchestration)
   +-- args-parser/
   |   +-- lexer.c                              157 lines
   |   +-- parser.c                             188 lines
   |   +-- kvp.c                                214 lines
   |   +-- spec.c                               222 lines
   |   +-- validate.c                           185 lines
   |   +-- args_helpers.c                       180 lines
   +-- core/
   |   +-- alloc.c                              161 lines
   |   +-- arena.c                               88 lines
   |   +-- str.c                                176 lines
   |   +-- vec.c                                143 lines
   |   +-- bytes.c                               78 lines
   |   +-- error.c                              148 lines
   |   +-- config.c                             165 lines
   |   +-- log.c                                111 lines
   |   +-- color.c                              123 lines
   |   +-- regex.c                              102 lines
   +-- crypto/
   |   +-- sha256.c                             228 lines
   +-- io/
   |   +-- fs_readwrite.c                        81 lines
   |   +-- fs_copytree.c                        135 lines
   |   +-- fs_atomic.c                           87 lines
   |   +-- path_norm.c                          137 lines
   |   +-- metadata_filter.c                     88 lines
   |   +-- git_fetch.c                          425 lines
   |   +-- archive.c                            301 lines
   +-- proc/
   |   +-- spawn.c                              213 lines
   |   +-- wait.c                                84 lines
   |   +-- env.c                                 10 lines
   +-- platform/
   |   +-- signal.c                              31 lines
   |   +-- common/platform_common.c              27 lines
   |   +-- posix/fs_posix.c                     237 lines
   |   +-- posix/proc_posix.c                    98 lines
   |   +-- posix/clock_posix.c                   62 lines
   +-- template/
   |   +-- manifest_load.c                      625 lines
   |   +-- var_merge.c                          218 lines
   |   +-- planner.c                            211 lines
   |   +-- staging.c                            193 lines
   |   +-- renderer.c                           110 lines
   |   +-- transform.c                           96 lines
   |   +-- writer.c                             635 lines
   |   +-- embedded_registry.c                   39 lines
   +-- certs/
       +-- certs_cmd.c                          (orchestrator)
       +-- certs_config.c                       246 lines
       +-- certs_san.c                          123 lines
       +-- certs_ca.c                           209 lines
       +-- certs_leaf.c                         251 lines
       +-- acme_account.c                       208 lines
       +-- acme_order.c                         143 lines
       +-- acme_challenge.c                     286 lines
       +-- acme_finalize.c                      294 lines
       +-- acme_http.c                          248 lines
       +-- acme_jws.c                           436 lines
       +-- acme_json.c                           93 lines
       +-- acme_json.h                           25 lines  (convention violation)

   include/phosphor/                            26 headers
   tests/unit/                                  38 test files, 531 tests
   tests/integration/                           2 scripts, 12 test cases
   templates/cathode-landing/                   42 template files
   subprojects/                                 5 vendored dependencies

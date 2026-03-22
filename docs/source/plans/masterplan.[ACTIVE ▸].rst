.. meta::
   :title: phosphor masterplan
   :tags: #neonsignal, #phosphor
   :status: active
   :updated: 2026-02-20

.. index::
   single: masterplan
   single: specification
   single: architecture

planning phases and tasks
=========================

- organise the phases into separate directories.
- every phase as a task.
  - tasks are files into the phase directory.
  
directory structure
___________________

.. code-block:: text

   phosphor/ # this directory.
   ├── pre-promotion/                    # "kind of ready" work (phases 0-3)
   │     ├── phase-0-[phase_name]/
   │     │     ├── task-N-[task_name].rst
   │     │     └── deliverable-task-N-[name].rst
   │     ├── phase-1-[phase_name]/
   │     │     └── task-N-[task_name].rst
   │     ├── phase-2-[phase_name]/
   │     │     └── task-N-[task_name].rst
   │     └── phase-3-[phase_name]/
   │           └── task-N-[task_name].rst
   └── post-promotion/                   # after sibling repo move (phases 4-6)
         ├── phase-4-[phase_name]/
         │     └── task-N-[task_name].rst
         ├── phase-5-[phase_name]/
         │     └── task-N-[task_name].rst
         └── phase-6-[phase_name]/
               └── task-N-[task_name].rst

Phase/Task Breakdown with Priority
___________________________________

The critical path to M1 (Create MVP) drives priority: Phase 0 → Phase 1 → Phase 2.
Everything else follows.

Phase 0: discovery-and-contract-freeze
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: HIGHEST PRIORITY
   :class: important

   No code, pure spec. Unblocks everything.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Blocks
   * - ``task-1-script-inventory``
     - Inventory existing scripts: ``build.sh``, ``clean.sh``, ``all.sh``,
       ``global_variables.sh``. Document expected inputs/outputs/side-effects.
     - Phase 3
   * - ``task-2-cli-grammar-freeze``
     - Freeze EBNF grammar for all commands (already drafted in masterplan,
       needs formal sign-off).
     - Phase 1
   * - ``task-3-exit-codes-and-logging``
     - Formalize exit code map (0-8), logging levels, UX diagnostic subcodes.
     - Phase 1
   * - ``task-4-variable-mapping``
     - Map ``global_variables.sh`` vars (``SNI``, ``SITE_OWNER``, ``TLD``, etc.)
       to phosphor flags.
     - Phase 2
   * - ``task-5-shell-compat-matrix``
     - Side-by-side behavior matrix: current scripts vs intended phosphor commands.
     - Phase 3


Phase 1: core-library-and-parser
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: HIGH PRIORITY
   :class: important

   Foundational C code.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Depends on
   * - ``task-1-project-scaffold``
     - ``meson.build``, directory tree, header stubs, compiler flags,
       sanitizer targets.
     - Phase 0 tasks 2, 3
   * - ``task-2-core-primitives``
     - ``alloc.c``, ``bytes.c``, ``str.c``, ``vec.c``, ``arena.c``, ``error.c``,
       ``log.c`` with ownership.
     - task-1
   * - ``task-3-platform-layer``
     - ``platform_common.c``, POSIX fs/proc/clock stubs, ``signal.c``.
     - task-2
   * - ``task-4-args-lexer-parser``
     - ``lexer.c``, ``parser.c`` -- tokenize and parse per frozen EBNF.
     - task-2
   * - ``task-5-argspec-registry``
     - ``spec.c`` -- typed flag registry, type resolution for
       ``string``/``int``/``bool``/``enum``/``path``/``url``.
     - task-4
   * - ``task-6-kvp-parser``
     - Nested KVP parsing with ``!``-prefix, ``=`` separator, brace nesting.
     - task-5
   * - ``task-7-args-validator``
     - ``validate.c`` -- semantic validation, diagnostics with UX subcodes.
     - task-5, task-6
   * - ``task-8-cli-dispatch``
     - ``cli_dispatch.c``, ``cli_help.c``, ``cli_version.c`` -- command routing
       skeleton.
     - task-7
   * - ``task-9-unit-tests-core-parser``
     - Unit tests for all core primitives + parser edge cases + KVP fuzzing.
     - task-8


Phase 2: create-command-mvp
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: HIGH PRIORITY
   :class: important

   First deliverable.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Depends on
   * - ``task-1-toml-integration``
     - Integrate ``toml-c`` dependency, build config, version lock.
     - Phase 1 task-1
   * - ``task-2-manifest-loader``
     - ``manifest_load.c`` -- parse ``template.phosphor.toml``, schema validation.
     - task-1
   * - ``task-3-variable-merge``
     - Variable schema, merge pipeline: CLI flags → env → config → defaults.
     - task-2
   * - ``task-4-path-and-fs``
     - ``path_norm.c``, ``fs_readwrite.c``, ``fs_copytree.c``, ``fs_atomic.c``,
       ``metadata_filter.c``.
     - Phase 1 task-3
   * - ``task-5-template-engine``
     - ``planner.c``, ``renderer.c`` (placeholder substitution), ``transform.c``,
       ``writer.c``.
     - task-3, task-4
   * - ``task-6-staging-strategy``
     - Staging directory lifecycle, atomic rename, ``EXDEV`` fallback, cleanup.
     - task-4
   * - ``task-7-create-command``
     - ``create_cmd.c`` -- full pipeline: args → manifest → plan → stage → commit.
     - task-5, task-6
   * - ``task-8-config-loader``
     - ``config.c`` -- project-local config file loading, precedence chain.
     - task-3
   * - ``task-9-integration-tests``
     - Golden output tests, ASan/Valgrind clean run, deterministic hashing.
     - task-7


Phase 3: build-compatibility-mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: MEDIUM PRIORITY

   Bridge existing scripts under ``phosphor build``.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Depends on
   * - ``task-1-proc-layer``
     - ``spawn.c``, ``env.c``, ``wait.c`` -- child process execution with exit
       code mapping.
     - Phase 1 task-3
   * - ``task-2-build-command``
     - ``build_cmd.c`` -- validate layout, invoke scripts via proc layer.
     - task-1
   * - ``task-3-deploy-and-hygiene``
     - ``--deploy-at`` flag, metadata cleanup, output standardization.
     - task-2


Phase 4: internalize-shell-behavior
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: LOWER PRIORITY

   Replace shell scripts with C modules.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Depends on
   * - ``task-1-clean-command``
     - ``clean_cmd.c`` -- replace ``clean.sh`` with C.
     - Phase 3
   * - ``task-2-copy-deploy-in-c``
     - Move copy/deploy orchestration from bash to C modules.
     - task-1
   * - ``task-3-script-fallback-flag``
     - Feature flag for script fallback during transition.
     - task-2


Phase 5: remote-templates
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: DEFERRED

   Remote template sources and archive support.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Depends on
   * - ``task-1-libgit2-source``
     - Optional ``libgit2`` integration for remote template fetch.
     - Phase 2
   * - ``task-2-archive-support``
     - ``libarchive`` integration, checksum verification.
     - task-1
   * - ``task-3-extended-schema``
     - Richer manifest schema, advanced filters.
     - task-2


Phase 6: stabilization
^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: DEFERRED

   CI, packaging, and documentation.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Depends on
   * - ``task-1-ci-matrix``
     - Full CI matrix: macOS + Linux, sanitizer jobs.
     - Phase 4
   * - ``task-2-packaging``
     - Homebrew formula, deb/rpm artifacts.
     - task-1
   * - ``task-3-docs-playbook``
     - Template author documentation and playbook.
     - Phase 2


Phase init: init command
^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: DRAFT

   Interactive scaffolding from hardcoded C buffers.

.. list-table::
   :header-rows: 1
   :widths: 25 55 20

   * - Task
     - Description
     - Depends on
   * - ``task-1-template-preparation``
     - Edit template files: replace build-time globals with ``<<placeholder>>``
       syntax, create ``template.phosphor.toml`` manifest.
     - template/ copied
   * - ``task-2-c-buffer-embedding``
     - Convert template files to C static byte arrays, registry header,
       meson integration.
     - task-1
   * - ``task-3-init-command``
     - ``init_cmd.c`` with interactive stdin prompts, prompt module
       (``prompt.h``/``prompt.c``), CLI integration.
     - task-2
   * - ``task-4-remote-yaml-templates``
     - Remote template fetch via libgit2/libarchive, YAML syntax tree
       format, JSX/TS/CSS code generation.
     - task-3, Phase 5


Priority Summary
^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 15 60 25

   * - Priority
     - Scope
     - Milestone
   * - **IMMEDIATE**
     - Phase 0 (spec-only, no code, unblocks everything)
     - --
   * - **NEXT**
     - Phase 1 (foundational C -- core + parser)
     - --
   * - **NEXT**
     - Phase 2 (create MVP -- first user-facing deliverable)
     - M1
   * - **LATER**
     - Phase 3 (build bridge)
     - M2
   * - **LATER**
     - Phase 4 (script elimination)
     - M3
   * - **DEFERRED**
     - Phase 5, 6 (remote + hardening)
     - M4
   * - **DRAFT**
     - Phase init (interactive scaffolding from C buffers)
     - --

.. note::

   Phase 0 is pure documentation work with no dependencies -- it should start
   immediately and can be completed in parallel with project scaffolding.

phosphor pure c cli masterplan
==============================

context
_______

this repository is a template/boilerplate for cathode + neonsignal projects.
the current operational surface is script-centric (bash + npm scripts), which works,
but makes the toolchain behavior distributed across multiple files and shell semantics.

the proposed direction is a system-installed executable named ``phosphor`` written in pure c,
focused on low-level control, explicit memory ownership, and byte-precise file/process
operations.

this plan assumes:

1. ``phosphor`` will be installable on system path.
2. ``phosphor create`` is the first high-value command.
3. ``phosphor build`` and script integration follow as staged milestones.
4. external dependencies are allowed when they are high quality and justified.


vision
______

build a deterministic, low-level, production-grade cli that can:

1. bootstrap cathode/neonsignal projects from templates with precise customization.
2. replace fragile shell orchestration with one cohesive binary and stable command contract.
3. preserve transparency: every filesystem/process mutation is explicit and auditable.
4. scale from simple project creation to full lifecycle orchestration (build, lint, validate,
   package, deploy pipeline hooks).


core engineering principles
___________________________

1. pure c runtime core.
2. explicit ownership and lifecycle for every heap allocation.
3. byte-by-byte correctness for io transformations.
4. deterministic outputs (same input + flags => same filesystem tree and metadata policy).
5. stable cli contract with versioned behavior.
6. strong separation of concerns (soc) across parser, core, commands, adapters, and platform.
7. minimal hidden behavior and no magical fallback that changes semantics silently.


scope boundaries
________________

in scope (v1-v2):

1. ``phosphor create`` from local template and optional remote source.
2. ``phosphor build`` orchestrating current scripts as transition layer.
3. configurable variable injection (name, tld, owner fields, site links, deploy target).
4. metadata hygiene rules (block/clean ``.ds_store`` and ``._*``).
5. robust error taxonomy and deterministic exit codes.

out of scope initially (future stages):

1. replacing node/esbuild runtime internals.
2. full remote registry ecosystem.
3. plugin abi for third-party compiled modules.
4. full package manager replacement.


top-level cli contract (draft)
______________________________

executable:

- ``phosphor``

core commands:

1. ``phosphor create``
2. ``phosphor build``
3. ``phosphor clean``
4. ``phosphor doctor``
5. ``phosphor version``
6. ``phosphor help``
7. ``phosphor init``

init command examples:

1. ``phosphor init --name=my-website``
2. ``phosphor init --name=my-website --tld=.com --owner="alice"``
3. ``phosphor init --name=my-website --output=./projects --yes``
4. ``phosphor init --name=my-website --template=https://github.com/user/repo``

high-value create command examples:

1. ``phosphor create --name=my-website``
2. ``phosphor create --name=my-website --template=./template``
3. ``phosphor create --name=my-website --tld=.com --owner="alice"``
4. ``phosphor create --name=my-website --output=./projects --force``

build command examples:

1. ``phosphor build --deploy-at=public/``
2. ``phosphor build --project=./my-website --tld=.com``
3. ``phosphor build --clean-first --deploy-at=./public/site``

exit code policy (stable):

1. ``0`` success
2. ``1`` general/unmapped error (including unmapped child process exit codes)
3. ``2`` invalid args/usage
4. ``3`` config/template parse error
5. ``4`` filesystem error
6. ``5`` process execution failure
7. ``6`` validation/guardrail failure
8. ``7`` internal invariant violation
9. ``8`` interrupted by signal (sigint/sigterm)

note: child process exit codes that do not map to a specific phosphor category are
translated to exit code ``1``.


argument parser contract (strict typed assignment model)
________________________________________________________

objective:

1. define a deterministic, strongly-typed cli grammar with no ambiguous flag semantics.
2. ensure parser output is data-only and command execution remains side-effect free until
   dispatch.
3. make invalid usage fail fast and consistently with exit code ``2``.

core parsing rules:

1. v1 accepts long flags only (``--flag`` style).
2. valued flags must use assignment form ``--flag=value``.
3. space-separated valued form (``--flag value``) is rejected in strict mode.
4. boolean flags use a two-category model:

   **action modifiers**: bare boolean flags. presence = true, absence = false.
   naming: short imperative verbs/phrases. no ``--enable-``/``--disable-`` prefix.
   examples: ``--force``, ``--dry-run``, ``--yes``, ``--allow-hooks``, ``--clean-first``,
   ``--strict``, ``--verbose``

   **feature toggles**: explicit polarity ``--enable-x``/``--disable-x`` for features where
   both on and off states are meaningful and the default may vary by context.
   examples (future): ``--enable-color``/``--disable-color``,
   ``--enable-cache``/``--disable-cache``

   rules:
   a. action modifiers must not accept ``=value`` (reject ``--force=true``).
   b. feature toggles must appear as polarity pair in spec (both forms documented).
   c. conflicting polarity flags (``--enable-x`` and ``--disable-x``) are hard errors.
   d. repeated bare boolean flags are hard errors.

5. repeated scalar flags are hard errors.
7. unknown flags are hard errors.

typed flag contract:

1. every valued flag maps to an ``argspec`` entry with an explicit type.
2. supported v1 types:
   a. ``string``
   b. ``int`` (integer only)
   c. ``bool``
   d. ``enum``
   e. ``path``
   f. ``url``
   g. ``kvp`` (custom key-value object)
3. type mismatches are hard usage errors (exit code ``2``).
4. ``enum`` values require exact match against declared choices.
5. ``path`` values may enforce relative-only constraints in command-specific validators.
6. ``url`` values require explicit scheme (``https://``, ``http://``) for strict mode.

custom kvp value contract (``!``-prefixed):

1. kvp values must begin with ``!``.
2. pair separator is ``|``.
3. pair syntax is ``key:value``.
4. nested kvp objects are expressed with braces:
   a. ``--flag=!x:34|y:65``
   b. ``--flag=!meta:{a:1|b:2}|enabled:true``
5. scalar value types inside kvp:
   a. string (quoted or unquoted token)
   b. number
   c. bool (``true``/``false``)
6. duplicate keys at the same object depth are rejected.
7. missing key, missing value, malformed nesting, or unbalanced braces are rejected.

parser grammar (v1 draft ebnf):

.. code-block:: text

   command_line   = "phosphor" command { arg } ;
   command        = "create" | "build" | "clean" | "doctor" | "version" | "help" ;
   arg            = long_flag ;
   long_flag      = valued_flag | bool_action | bool_switch ;
   valued_flag    = "--" ident "=" typed_value ;
   bool_action    = "--" ident ;
   bool_switch    = "--enable-" ident | "--disable-" ident ;
   typed_value    = scalar | kvp ;
   scalar         = quoted_string | bare_token | number_lit | bool_lit ;
   kvp            = "!" kvp_pairs ;
   kvp_pairs      = kvp_pair { "|" kvp_pair } ;
   kvp_pair       = kvp_key ":" kvp_atom ;
   kvp_atom       = scalar | "{" kvp_pairs "}" ;
   kvp_key        = ident ;

   ident          = letter { letter | digit | "-" | "_" } ;
   quoted_string  = '"' { any_char - '"' | '\\"' } '"' ;
   bare_token     = visible_char { visible_char } ;   (* no whitespace, no '|', no '{', no '}' *)
   number_lit     = [ "-" ] digit { digit } ;
   bool_lit       = "true" | "false" ;
   letter         = "a"-"z" | "a"-"z" ;
   digit          = "0"-"9" ;

note: type validation (``enum`` choice matching, ``path`` constraint checking, ``url`` scheme
validation) is performed in the semantic validation phase after parsing, not during tokenization.
the grammar produces untyped ``scalar`` tokens; the ``argspec`` registry resolves them to their
declared types.

parser diagnostics (stable subcodes under usage category):

1. ``ux001`` unknown flag
2. ``ux002`` missing assignment/value
3. ``ux003`` duplicate flag
4. ``ux004`` enable/disable conflict
5. ``ux005`` typed value mismatch
6. ``ux006`` malformed kvp payload
7. ``ux007`` enum choice violation

output guarantee:

1. ``args-parser`` returns typed structs only.
2. no filesystem/process/network side effects are allowed in parse/validate stage.
3. diagnostics include flag name, expected type, received token, and source token index.


separation of concerns (soc) architecture
_________________________________________

repository layout for ``phosphor`` source:

.. code-block:: text

   phosphor/
   ├── include/
   │   └── phosphor/
   │       ├── types.h
   │       ├── error.h
   │       ├── alloc.h
   │       ├── bytes.h
   │       ├── str.h
   │       ├── vec.h
   │       ├── arena.h
   │       ├── path.h
   │       ├── fs.h
   │       ├── proc.h
   │       ├── log.h
   │       ├── config.h
   │       ├── manifest.h
   │       ├── template.h
   │       ├── render.h
   │       ├── args.h
   │       ├── cli.h
   │       ├── platform.h
   │       └── signal.h
   ├── src/
   │   ├── main.c
   │   ├── cli/
   │   │   ├── cli_dispatch.c
   │   │   ├── cli_help.c
   │   │   └── cli_version.c
   │   ├── args-parser/
   │   │   ├── lexer.c
   │   │   ├── parser.c
   │   │   ├── spec.c
   │   │   └── validate.c
   │   ├── core/
   │   │   ├── alloc.c
   │   │   ├── bytes.c
   │   │   ├── str.c
   │   │   ├── vec.c
   │   │   ├── arena.c
   │   │   ├── error.c
   │   │   ├── config.c
   │   │   └── log.c
   │   ├── io/
   │   │   ├── fs_readwrite.c
   │   │   ├── fs_copytree.c
   │   │   ├── fs_atomic.c
   │   │   ├── path_norm.c
   │   │   └── metadata_filter.c
   │   ├── proc/
   │   │   ├── spawn.c
   │   │   ├── env.c
   │   │   └── wait.c
   │   ├── template/
   │   │   ├── manifest_load.c
   │   │   ├── planner.c
   │   │   ├── renderer.c
   │   │   ├── transform.c
   │   │   └── writer.c
   │   ├── commands/
   │   │   ├── create_cmd.c
   │   │   ├── build_cmd.c
   │   │   ├── clean_cmd.c
   │   │   └── doctor_cmd.c
   │   └── platform/
   │       ├── posix/
   │       │   ├── fs_posix.c
   │       │   ├── proc_posix.c
   │       │   └── clock_posix.c
   │       ├── signal.c
   │       └── common/
   │           └── platform_common.c
   ├── tests/
   │   ├── unit/
   │   ├── integration/
   │   ├── golden/
   │   └── fixtures/
   ├── docs/                         # sphinx documentation
   │   ├── Makefile
   │   ├── requirements.txt
   │   └── source/
   │       ├── conf.py
   │       ├── index.rst
   │       └── reference/            # frozen spec documents
   │           ├── cli-grammar.rst
   │           └── exit-codes-and-logging.rst
   ├── meson.build
   └── readme.rst

header-to-source mapping:

- ``types.h`` -> header-only (type definitions, no .c)
- ``error.h`` -> core/error.c
- ``alloc.h`` -> core/alloc.c
- ``bytes.h`` -> core/bytes.c
- ``str.h`` -> core/str.c
- ``vec.h`` -> core/vec.c
- ``arena.h`` -> core/arena.c
- ``path.h`` -> io/path_norm.c
- ``fs.h`` -> io/fs_readwrite.c, io/fs_copytree.c, io/fs_atomic.c, io/metadata_filter.c
- ``proc.h`` -> proc/spawn.c, proc/env.c, proc/wait.c
- ``log.h`` -> core/log.c
- ``config.h`` -> core/config.c
- ``manifest.h`` -> template/manifest_load.c
- ``template.h`` -> template/ (manifest_load.c, planner.c, writer.c)
- ``render.h`` -> template/renderer.c, template/transform.c
- ``args.h`` -> args-parser/ (lexer.c, parser.c, spec.c, validate.c)
- ``cli.h`` -> cli/cli_dispatch.c
- ``platform.h`` -> platform/posix/\*.c or platform/win32/\*.c (compile-time selection)
- ``signal.h`` -> platform/signal.c

module responsibilities:

1. ``args-parser``: parse cli tokens into typed command structs.
2. ``core``: allocator, container, bytes/string primitives, errors, logging.
3. ``io``: byte-safe filesystem traversal/copy/write plus metadata policy.
4. ``proc``: child process execution abstraction with deterministic env and exit mapping.
5. ``template``: template manifest model + render/copy plan.
6. ``commands``: command business logic with no direct syscall details.
7. ``platform``: posix/os abstraction boundary.
8. ``cli``: top-level routing and ux output formatting.

hard soc rule:

- ``commands/*`` may call ``template``, ``io``, ``proc``, ``core``.
- ``template/*`` cannot call ``proc``.
- ``core/*`` cannot import higher-level modules.
- ``args-parser/*`` returns data-only structs, no side effects.


memory management strategy (pure c, explicit ownership)
________________________________________________________

memory model:

1. default allocator interface wrapper around ``malloc/calloc/realloc/free``.
2. optional debug allocator mode with canary/poison/leak accounting.
3. arena allocators for short-lived command lifecycle objects.
4. heap allocations for long-lived objects and returned buffers.

ownership policy:

1. every struct documents owner for each pointer field.
2. constructors return either fully-owned object or error, never partial ownership leakage.
3. destructors are idempotent and null-safe.
4. no hidden global mutable ownership except controlled process-wide config singleton.

failure discipline:

1. every allocation checked.
2. on failure, command returns deterministic error code and context.
3. cleanup-on-failure paths are mandatory and reviewed.

memory correctness instrumentation:

1. addresssanitizer and undefinedbehaviorsanitizer build targets.
2. valgrind ci job for linux profile.
3. optional custom heap event tracing in debug builds.
4. macos ci uses addresssanitizer and ``leaks`` command-line tool (xcode instruments)
   as valgrind has limited apple silicon support.


byte-to-byte precision policy
_____________________________

objective:

- preserve binary and text data exactly unless transformation is explicitly requested.

rules:

1. use byte slices (``uint8_t* + size_t``), not c string assumptions, in file processing pipeline.
2. separate text transform path from raw binary copy path.
3. detect binary candidates by nul-byte heuristic and extension override table.
4. apply placeholder substitution only to declared text targets.
5. never normalize line endings unless ``--normalize-eol`` is explicitly provided.
6. atomic writes for generated files: write temp file, ``fsync`` (when needed), rename.
7. preserve executable bit where template indicates executability.
8. apply a deterministic ignore/deny filter for metadata and transient files.

metadata deny defaults:

1. ``.ds_store``
2. ``._*``
3. ``thumbs.db``
4. ``.spotlight-v100``
5. ``.trashes``


dependency strategy (multiple dependencies allowed)
___________________________________________________

dependency acceptance criteria:

1. mature, actively maintained, permissive license.
2. small api surface and stable release cadence.
3. proven use in production tooling.
4. good security posture and cve responsiveness.

suggested dependency tiers:

tier 1 (likely include):

1. ``toml-c`` (arp242 fork) for toml 1.1 config + manifest parsing.
   rationale: ``tomlc99`` (cktan) is officially obsolete; its author redirects to ``tomlc17``.
   ``toml-c`` is a maintained fork that passes the full toml 1.1 test suite, fixes known errors
   in ``tomlc99``, supports header-only or compiled-library usage, and has an active contributor
   base. ``tomlc17`` (cktan, r260131 release) is also viable but ``toml-c`` has a broader
   contributor surface and longer patch history. either is acceptable; ``toml-c`` is the primary
   recommendation.
   fallback: ``tomlc17`` if ``toml-c`` maintenance stalls.
2. ``libarchive`` for optional template pack import/export workflows.

posix ``<regex.h>`` (``regcomp``/``regexec`` with ``reg_extended``) is used for variable
``pattern`` field validation and slug matching. this is a system api on macos and linux,
not an external dependency. ere (extended regular expressions) is the supported syntax.
on future windows targets, a lightweight ``<regex.h>`` shim or ``pcre2`` will be required.

tier 1b (deferred from tier 1, use builtin for v1):

1. ``fnmatch`` (posix) for include/exclude/filter glob rules in v1.
   rationale: ``pcre2`` is a heavy dependency for glob-style path matching. posix ``fnmatch(3)``
   covers ``*``, ``?``, ``[...]`` patterns which handle 95%+ of template filter use cases.
   a small custom glob matcher can be added if ``fnmatch`` proves insufficient before reaching
   for ``pcre2``. ``pcre2`` moves to tier 2 as an optional advanced filter backend.

tier 2 (optional by build flag):

1. ``libgit2`` for fetching templates from git repos without shelling out.
2. ``zstd`` for efficient template bundle compression.
3. ``pcre2`` for advanced include/exclude/filter rules beyond glob patterns.

tier 3 (diagnostic/dev only):

1. ``cmocka`` for c unit tests.
2. ``criterion`` as alternative testing framework.

dependency governance:

1. lock versions in build config and changelog.
2. track each dependency in ``docs/source/reference/dependencies.rst`` with reason and risk notes.
3. provide compile-time toggles to disable optional deps.


configuration and template model
________________________________

template manifest file (draft):

- ``template.phosphor.toml``

purpose:

1. declare variables and validation constraints.
2. declare file operations (copy, transform, skip, chmod).
3. declare post-create steps (optional and explicit).

config precedence:

1. cli flags.
2. environment variables (prefixed ``phosphor_``).
3. project-local config file.
4. manifest defaults.

validation examples:

1. ``name`` matches slug pattern (posix ere).
2. ``tld`` starts with ``.`` and length constraints.
3. destination path not inside template source path.


draft ``template.phosphor.toml`` schema
_______________________________________

schema goals:

1. keep parser and runtime deterministic and easy to validate in c.
2. separate declaration (what) from execution order (how).
3. allow rich templates without introducing hidden behavior.

versioning policy:

1. ``manifest.schema`` is a required positive integer. each value represents a distinct,
   self-contained schema version.
2. additive, non-breaking field additions within the same schema integer are allowed
   (the cli ignores unknown keys with a warning).
3. any change that removes, renames, or changes the semantics of an existing field
   requires incrementing the schema integer.

top-level structure:

1. ``[manifest]`` metadata and schema version.
2. ``[template]`` template identity and source layout.
3. ``[defaults]`` static default values for variables.
4. ``[[variables]]`` typed user-facing variables.
5. ``[filters]`` global include/exclude/deny rules.
6. ``[[ops]]`` ordered file operations.
7. ``[[hooks]]`` optional explicit post-create commands.

required keys:

1. ``manifest.schema`` (integer, currently ``1``).
2. ``manifest.id`` (stable slug for template family).
3. ``manifest.version`` (semantic version string).
4. ``template.source_root`` (relative path inside template source).
5. at least one ``[[ops]]`` entry.

recommended keys:

1. ``template.name`` human-readable display name.
2. ``template.description`` short summary.
3. ``template.min_phosphor`` minimum cli version.
4. ``template.license`` spdx identifier.

version comparison for ``min_phosphor`` uses semver precedence rules (major.minor.patch).
the cli embeds its own version at compile time. if the cli version is lower than
``min_phosphor``, the command fails with exit code ``6`` (validation failure) and prints
the required minimum.

variable table schema (``[[variables]]``):

1. ``name``: variable identifier (``[a-z][a-z0-9_]*``).
2. ``type``: ``string`` | ``bool`` | ``int`` | ``enum`` | ``path`` | ``url``.
3. ``required``: boolean.
4. ``default``: typed default value.
5. ``env``: optional environment variable fallback (``phosphor_*``).
6. ``prompt``: optional human prompt.
7. ``pattern``: posix ere string for string/path types.
8. ``min`` / ``max``: numeric bounds for ``int``.
9. ``choices``: array for ``enum``.
10. ``secret``: boolean, prevents echo in interactive mode.

filter schema (``[filters]``):

1. ``exclude``: path-glob list skipped from copy/render plan.
2. ``deny``: path-glob list that causes validation failure if encountered.
3. ``binary_ext``: extension allowlist for binary-safe copy mode.
4. ``text_ext``: extension allowlist for text transform mode.

operation schema (``[[ops]]``):

1. ``id``: stable operation id.
2. ``kind``: ``mkdir`` | ``copy`` | ``render`` | ``chmod`` | ``remove``.
3. ``from``: source path (required for copy/render).
4. ``to``: destination path (required for mkdir/copy/render/remove).
5. ``mode``: octal permission string for chmod or create defaults (ex: ``\"0755\"``).
6. ``overwrite``: boolean.
7. ``if``: optional condition expression over variables.
8. ``atomic``: boolean (default true for render/copy targets).
9. ``preserve_exec``: boolean.
10. ``newline``: ``keep`` | ``lf`` | ``crlf`` (default ``keep``).

remove operation safety:

1. ``remove`` operations are restricted to paths within the destination root.
2. ``remove`` cannot target paths outside the ``to`` root established by the create workflow.
3. ``remove`` operations are listed explicitly in ``--dry-run`` output with a warning marker.
4. ``remove`` is not allowed in ``--force`` mode without ``--yes`` confirmation.
5. symlink targets of ``remove`` are never followed; only the link itself is removed.

hook schema (``[[hooks]]``):

1. ``when``: ``post_create`` only for schema v1.
2. ``run``: argv array, no shell interpolation.
3. ``cwd``: relative working directory.
4. ``if``: conditional expression over variables.
5. ``allow_failure``: boolean.

hook security policy:

1. hooks execute arbitrary commands and represent a significant security surface when templates
   come from untrusted sources.
2. by default, hooks are displayed but not executed. the user must explicitly opt in with
   ``--allow-hooks`` or confirm interactively when prompted.
3. ``--yes`` alone does not enable hooks; ``--allow-hooks`` is required independently.
4. ``phosphor create --dry-run`` lists hooks without executing them.
5. future: an allowlist model (``~/.config/phosphor/trusted-templates.toml``) can auto-approve
   hooks from known template sources.

condition expression constraints (v1):

1. grammar kept intentionally small for deterministic c parser.
2. supported operators: ``==``, ``!=``, ``&&``, ``||``, unary ``!``.
3. literals: strings, integers, booleans.
4. variable references use ``var.<name>``.

condition expression grammar (v1):

.. code-block:: text

   cond_expr    = cond_or ;
   cond_or      = cond_and { "||" cond_and } ;
   cond_and     = cond_unary { "&&" cond_unary } ;
   cond_unary   = "!" cond_unary | cond_primary ;
   cond_primary = cond_cmp | "(" cond_expr ")" ;
   cond_cmp     = cond_atom [ ( "==" | "!=" ) cond_atom ] ;
   cond_atom    = var_ref | string_lit | int_lit | bool_lit ;
   var_ref      = "var." ident ;

templating constraints:

1. placeholder syntax: ``<<var_name>>``.
   rationale: ``${var_name}`` collides with shell variable syntax in ``.env`` files, shell
   scripts, and other common template targets. ``<<var_name>>`` is visually distinct, unlikely
   to appear in any source language, and trivial to scan for in a byte stream. the ``<<``/``>>``
   delimiters have no special meaning in toml, html, css, js/ts, or shell contexts that would
   cause accidental expansion.
   escape sequence: ``\<<`` emits a literal ``<<`` (backslash consumed).
2. placeholder expansion is applied only to ``render`` operations.
3. undefined placeholders are hard errors unless variable declares ``required = false`` and empty fallback.
4. binary files are never placeholder-expanded.

path safety constraints:

1. ``from`` and ``to`` are normalized and must not escape root.
2. absolute destination paths are rejected unless explicit global flag allows them.
3. symlink handling must be explicit per operation (future v2 extension).

minimal manifest example (v1):

.. code-block:: toml

   [manifest]
   schema = 1
   id = "cathode-landing"
   version = "1.0.0"

   [template]
   name = "cathode landing"
   source_root = "."
   min_phosphor = "0.1.0"

   [defaults]
   tld = ".host"
   owner = "site owner"

   [[variables]]
   name = "project_name"
   type = "string"
   required = true
   pattern = "^[a-z0-9-]{2,64}$"
   prompt = "project name"

   [[variables]]
   name = "tld"
   type = "enum"
   required = true
   choices = [".host", ".com", ".io"]
   default = ".host"

   [filters]
   exclude = [".git/**", "node_modules/**"]
   deny = [".ds_store", "._*"]
   text_ext = [".ts", ".tsx", ".js", ".css", ".html", ".md", ".toml"]

   [[ops]]
   id = "copy-static"
   kind = "copy"
   from = "src/static"
   to = "src/static"

   [[ops]]
   id = "render-config"
   kind = "render"
   from = "template/.env.example"
   to = ".env.example"
   atomic = true
   overwrite = false
   if = "var.project_name != \"\""

   [[hooks]]
   when = "post_create"
   run = ["npm", "install"]
   cwd = "."
   allow_failure = false

validation order (runtime contract):

1. parse toml -> syntax validation.
2. schema validation -> required keys and type checks.
3. semantic validation -> path safety, duplicate ids, unknown refs.
4. operation plan validation -> cycle/conflict/overwrite checks.
5. execution preflight -> destination permissions and collisions.


detailed ``create`` workflow
____________________________

pipeline:

1. parse args -> validate command contract.
2. resolve template source (local path / archive / git source).
3. load manifest and variable schema.
4. merge variable values from flags/env/defaults.
5. build in-memory file operation plan.
6. run preflight checks (collisions, permissions, forbidden paths).
7. execute copy/render plan into staging directory.
8. validate staging directory integrity.
9. commit staging by rename/move into destination.
10. emit deterministic summary report.

staging directory strategy:

1. create staging directory as ``.phosphor-staging-<pid>-<timestamp>`` in the destination's
   parent directory.
2. all copy/render operations write into the staging directory first.
3. on success: atomic rename of staging directory to final destination path.
4. on failure or interrupt: remove staging directory entirely during cleanup.
5. on ``sigint``/``sigterm``: signal handler sets a flag; the operation loop checks the flag
   between steps and triggers cleanup (see signal handling section).
6. if a stale staging directory is detected from a prior crash, ``phosphor doctor`` reports it
   and ``phosphor clean --stale`` removes it.
7. if ``rename(2)`` fails with ``exdev`` (cross-device), fall back to recursive copy
   from staging to destination followed by staging removal. this fallback is logged
   at ``warn`` level.

safety checks:

1. path traversal prevention (reject ``..`` escape cases).
2. symlink policy explicit (copy link vs resolve target flag).
3. reject writing outside destination root.
4. enforce metadata deny list.

create output report:

1. total files copied.
2. total files transformed.
3. total bytes written.
4. skipped files and reasons.
5. next commands suggestion.


detailed ``build`` workflow (transition first)
______________________________________________

stage a (compatibility mode):

1. ``phosphor build`` validates environment and project layout.
2. executes existing scripts deterministically through ``proc`` layer.
3. maps script exits to stable ``phosphor`` exit codes.
4. adds metadata cleanup and deploy mirror guardrails.

stage b (internalized mode):

1. move clean/deploy/copy logic into c modules.
2. keep esbuild invocation external but controlled.
3. replace shell orchestration completely.

stage c (advanced):

1. optional direct js toolchain adapters.
2. optional reusable build cache metadata.

build flags draft:

1. ``--project=<path>``
2. ``--deploy-at=<path>``
3. ``--clean-first``
4. ``--tld=<.com|.host|...>``
5. ``--strict`` (fail on warnings)
6. ``--toml``


error handling and observability
________________________________

error object fields:

1. category
2. subcode
3. message
4. path/command context
5. cause chain id

logging levels:

1. error
2. warn
3. info
4. debug
5. trace

output modes:

1. human-readable colored output.
2. plain output for ci.
3. toml report output for automation.

diagnostics command:

- ``phosphor doctor`` validates compiler/toolchain/filesystem/project assumptions and reports
  actionable remediation.


security and hardening
______________________

1. no shell interpolation for process args.
2. use argv arrays and explicit escaping boundaries.
3. sanitize env passthrough (allowlist model).
4. prevent zip/tar slip when extracting template archives.
5. optional checksum verification for remote templates.
6. optional signature verification model for trusted template sources.

file write hardening:

1. refuse to overwrite outside destination root.
2. ``--force`` must be explicit for destructive overwrite behavior.
3. default safe mode preserves existing destination unless ``--force``.


signal handling
_______________

objective:

- ensure graceful cleanup on interrupt to avoid leaving partial filesystem state.

strategy:

1. install handlers for ``sigint`` and ``sigterm`` at process startup.
2. handlers set a volatile ``sig_atomic_t`` flag; no heap work in signal context.
3. long-running loops (file copy, render plan execution) check the flag between iterations.
4. when the flag is detected, the current command aborts with cleanup:
   a. remove staging directory if one exists.
   b. close open file descriptors.
   c. log the interruption reason to stderr.
   d. exit with code ``8`` (interrupted by signal).
5. ``sigpipe`` is ignored (``sig_ign``) to prevent silent crashes on broken pipes;
   write errors are handled through return codes instead.
6. child processes spawned via ``proc`` inherit default signal disposition; ``phosphor``
   forwards ``sigint`` to the child process group when interrupted during ``build``.


concurrency model
_________________

v1 is explicitly single-threaded.

rationale:

1. simplifies memory ownership — no locks, no atomic reference counts, no thread-local arenas.
2. template creation and build orchestration are io-bound, not cpu-bound; parallelism gains
   are marginal for typical project sizes.
3. the architecture does not preclude future parallelism: the ``io`` and ``template`` modules
   operate on independent file entries and could be dispatched to a thread pool without
   restructuring the ownership model, provided each thread gets its own arena.

future consideration (v2+):

1. parallel file copy/render with per-thread arena allocators.
2. concurrent child process execution for independent build steps.
3. thread pool size controlled by ``--jobs=<n>`` flag (default: 1).


cross-platform strategy
_______________________

initial target:

1. macos
2. linux

platform abstraction boundaries:

1. filesystem operations
2. path normalization
3. process spawn/wait
4. high-resolution timing

later windows support:

1. add ``platform/win32`` implementation.
2. maintain same command contract and exit code taxonomy.

platform-specific api dependencies requiring wrappers for windows:

1. ``fnmatch(3)`` -> custom glob matcher or bundled ``fnmatch`` implementation.
2. ``fsync`` -> ``flushfilebuffers``.
3. ``sigint``/``sigterm``/``sigpipe`` -> ``setconsolectrlhandler`` for console events.
4. ``sig_atomic_t`` -> ``volatile long`` with ``interlockedexchange``.
5. ``chmod`` with octal mode -> map to windows acls or skip with warning.
6. ``regcomp``/``regexec`` -> bundled ``<regex.h>`` shim or ``pcre2``.

these are not blocking for v1 (macos + linux only) but must be addressed before
windows support ships.


build system and distribution
_____________________________

recommended build system:

1. meson for modern, fast, and declarative build configuration.
2. ninja backend for speed (meson default).

compiler baseline:

1. c17 standard.
2. ``-wall -wextra -wpedantic`` required.
3. treat warnings as errors in ci.

distribution options:

1. ``meson install -C build`` to system path.
2. homebrew tap formula (macos).
3. deb/rpm artifacts (later).

versioning:

1. semantic versioning.
2. embed build metadata in ``phosphor version``.


testing strategy
________________

unit tests:

1. args parser edge cases.
2. memory ownership and destructor idempotence.
3. path sanitizer and traversal rejection.
4. text transform correctness and binary safety.
5. strict assignment parsing (reject ``--flag value`` in strict mode).
6. enable/disable switch conflicts.
7. duplicate scalar flag rejection.
8. typed mismatch diagnostics for ``string/number/bool/enum/path/url``.
9. kvp parser cases including nested objects.

integration tests:

1. create command golden output comparisons.
2. build command compatibility mode across sample templates.
3. metadata deny list enforcement.
4. failure injection scenarios (permission denied, disk full simulation).
5. args -> command struct -> execution pipeline parity for create/build/clean.

property/fuzz tests:

1. argument parser fuzzing.
2. manifest parser fuzzing.
3. path normalization fuzzing.
4. nested kvp parser fuzzing.

regression suite:

1. real cathode template clone and expected tree assertions.
2. deterministic output hashing with stable timestamps excluded.


migration plan (phased)
_______________________

phase 0: discovery and contract freeze

1. inventory existing scripts and expected behavior matrix.
2. freeze cli grammar draft for create/build/clean.
3. define error code map and logging conventions.
4. freeze strict args parser grammar for assignment mode and kvp nesting.
5. publish parser diagnostics contract (``ux001``..``ux007``).
6. produce compatibility matrix against current shell contracts:
   a. ``scripts/_default/build.sh``
   b. ``scripts/_default/clean.sh``
   c. ``scripts/_default/all.sh``
   d. ``scripts/global_variables.sh``
7. produce variable mapping table from ``scripts/global_variables.sh`` to phosphor flags:
   a. ``sni`` -> ``--name``
   b. ``site_owner`` / ``site_owner_slug_rev`` -> ``--owner`` / ``--owner-slug``
   c. ``tld`` -> ``--tld``
   d. ``data_dir`` / ``certs_*`` -> determine if these belong in create or build flags
   e. variables with no flag mapping -> document deprecation or add new flags

phase 1: core library and parser skeleton

1. implement ``core`` primitives.
2. implement ``args-parser`` and command dispatch.
3. add unit tests for parsing and core ownership.
4. implement typed ``argspec`` registry and validator dispatch.
5. implement nested kvp parser with deterministic error locations.

phase 2: create command mvp

1. local template source only.
2. manifest parsing + variable substitution.
3. atomic write and collision guardrails.
4. produce first stable release candidate.
5. bind typed parser outputs directly into variable merge pipeline.
6. reject unsupported manifest operation kinds with explicit config errors.

phase 3: build compatibility mode

1. integrate existing script invocation under ``phosphor build``.
2. standardize output and exit behavior.
3. add ``--deploy-at`` and metadata hygiene controls.

phase 4: internalize shell behavior

1. move clean/copy/deploy orchestration into c modules.
2. remove dependency on bash for standard workflows.
3. keep script fallback behind feature flag during transition.

phase 5: remote templates and dependency expansion

1. add optional ``libgit2`` source retrieval.
2. add archive support and checksum verification.
3. add richer manifest schema and filters.

phase 6: stabilization and ecosystem packaging

1. full ci matrix and sanitizer jobs.
2. package manager integrations.
3. publish docs/playbook for template authors.


milestones and deliverables
___________________________

m1 (create mvp):

1. ``phosphor create`` local template operational.
2. deterministic file generation with byte-safe handling.
3. memory sanitizer clean run.

m2 (build bridge):

1. ``phosphor build --deploy-at`` operational.
2. consistent exit code mapping.
3. metadata artifacts denied and cleaned.

m3 (script reduction):

1. major shell scripts replaced by c orchestration.
2. compatibility wrappers kept for a deprecation window.

m4 (production hardening):

1. dependency governance docs.
2. security validation for remote sources.
3. stable release ``v1.0.0``.


operational policies for template safety
________________________________________

1. default deny list for metadata and editor artifacts.
2. optional ``--allow-hidden`` flag for explicit exceptions.
3. preflight summary before writes unless ``--yes``.
4. optional ``--dry-run`` for plan-only execution.
5. structured audit log output for ci provenance.


risk register and mitigations
_____________________________

risk: scope explosion from too many commands too early.
mitigation: freeze ``create`` first, defer advanced registry/plugin ideas.

risk: dependency bloat and fragile build portability.
mitigation: tier dependencies, keep optional deps behind feature flags.

risk: subtle file transform corruption.
mitigation: byte-slice apis, binary-safe path, golden tests, fuzzing.

risk: behavior drift from existing scripts.
mitigation: compatibility matrix and integration tests against current outputs.

risk: platform-specific filesystem quirks.
mitigation: strict platform adapter layer and per-os integration tests.

risk: parser ambiguity across assignment mode, typed values, and nested kvp payloads.
mitigation: freeze ebnf grammar, deterministic tokenization rules, and stable diagnostics
subcodes (``ux001``..``ux007``) before parser implementation.

risk: user confusion from conflicting boolean toggles and duplicate flags.
mitigation: strict hard-error policy for duplicates and ``--enable-x``/``--disable-x``
conflicts with actionable diagnostics.

risk: accidental compatibility break with existing shell pipelines during migration.
mitigation: keep build transition in wrapper mode first, validate parity against
``scripts/_default/*`` behavior, then internalize orchestration in later phase.

risk: template manifest injection from untrusted sources.
mitigation: enforce resource limits during manifest parsing and plan execution.
guardrails:

1. maximum ``[[ops]]`` count per manifest (default: 1024).
2. maximum total bytes written per create invocation (default: 512 mib, overridable).
3. maximum single file size (default: 64 mib).
4. maximum directory depth for ``to`` paths (default: 32 levels).
5. maximum variable count (default: 128).
6. maximum hook count (default: 16).
7. path safety checks already prevent traversal via ``to``/``from`` fields.
8. ``deny`` filter catches forbidden filenames; combined with path normalization this blocks
   attempts to smuggle metadata artifacts through unusual encoding.

risk: hook execution from untrusted templates runs arbitrary commands.
mitigation: hooks are not executed by default; ``--allow-hooks`` required (see hook security
policy). ``--dry-run`` displays hooks without execution for audit.


suggested immediate next steps
______________________________

1. create a dedicated ``phosphor/`` sibling repository or ``tools/phosphor/`` monorepo module.
2. write command spec document ``docs/source/reference/cli-contract.rst``.
3. implement ``core`` + ``args-parser`` skeleton with tests.
4. implement ``create`` local-template mvp with manifest subset.
5. define first dependency set (likely ``toml-c`` only at first), with room to expand in
   later phases if justified.


definition of done for first public iteration
_____________________________________________

1. ``phosphor create --name=sample`` produces a valid project from this template.
2. no leaks under asan/valgrind in normal create flow.
3. generated tree is deterministic and free from metadata artifacts.
4. command returns clear errors and stable exit codes.
5. documentation explains manifest schema, flags, and safety behavior.


appendix a: draft command flags (create)
________________________________________

required/primary:

1. ``--name=<slug>``
2. ``--template=<path|url>`` (defaults to canonical template source)

common optional:

1. ``--output=<dir>``
2. ``--tld=<.host|.com|...>``
3. ``--owner=<name>``
4. ``--owner-slug=<slug>``
5. ``--github=<url>``
6. ``--instagram=<url>``
7. ``--x=<url>``
8. ``--force``
9. ``--dry-run``
10. ``--toml``
11. ``--allow-hooks``
12. ``--yes``
13. ``--normalize-eol=<lf|crlf>`` (applies to text files only, per byte-precision policy)
14. ``--allow-hidden``
15. ``--verbose``

if ``--template`` receives a url value but remote source support is not compiled in
(``libgit2`` absent), the cli rejects the value at parse time with exit code ``2``
and a diagnostic message suggesting installation with remote support enabled.


appendix b: draft command flags (build)
_______________________________________

1. ``--project=<path>``
2. ``--deploy-at=<path>``
3. ``--clean-first``
4. ``--tld=<suffix>``
5. ``--strict``
6. ``--toml``
7. ``--verbose``
8. ``--normalize-eol=<lf|crlf>`` (applies to text files only, per byte-precision policy)


appendix c: draft command flags (clean)
________________________________________

1. ``--stale`` (action modifier: remove stale staging directories)
2. ``--project=<path>``
3. ``--dry-run``
4. ``--verbose``


appendix d: draft command flags (doctor)
_________________________________________

1. ``--project=<path>``
2. ``--verbose``
3. ``--toml`` (machine-readable output)


appendix e: version/help behavior
__________________________________

- ``phosphor version``: print ``phosphor <semver> (<build-hash> <build-date>)`` to stdout, exit 0.
- ``phosphor help``: print usage summary for all commands. ``phosphor help <command>`` prints
  command-specific usage.


repository lifecycle: "kind of ready" promotion
_________________________________________________

phosphor starts as a monorepo module at ``tools/phosphor/`` inside the
neonsignal.nutsloop.com.jsx repository. this keeps plans, scripts, and the
emerging C source in one place during early development.

once phosphor reaches the "kind of ready" threshold it graduates to a sibling
repository (e.g., ``/neonsignal/phosphor/`` or ``nutsloop/phosphor``).

kind of ready criteria:

1. ``phosphor create`` works end-to-end with a local template (milestone M1 core).
2. ``phosphor build`` can invoke existing scripts via the proc layer (phase 3 task 2).
3. unit tests pass under AddressSanitizer with zero leaks.
4. exit codes and diagnostics match the frozen contract (phase 0 deliverables).
5. a minimal ``README.rst`` documents build, install, and basic usage.

promotion steps:

1. move ``tools/phosphor/`` to its own repository.
2. update ``neonsignal.nutsloop.com.jsx`` to reference phosphor as an external
   tool (system PATH or git submodule, decided at promotion time).
3. archive the ``tools/phosphor/`` directory in the parent repo (or remove it
   with a forwarding note in the commit message).
4. update CI and npm scripts to invoke ``phosphor`` from PATH instead of
   ``tools/phosphor/``.

until promotion, ``tools/phosphor/`` has its own ``.git`` so history is
self-contained and the move is a clean ``git push`` to the new remote.


conclusion
__________

this plan keeps the spirit of low-level c engineering while still being pragmatic:
ship value early with ``create``, bridge current script workflows through ``build``, then
internalize orchestration in a carefully layered architecture with strong memory and
byte-precision guarantees.

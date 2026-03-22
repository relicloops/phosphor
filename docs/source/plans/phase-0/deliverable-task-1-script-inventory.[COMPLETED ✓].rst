.. meta::
   :title: script inventory deliverable
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; deliverable; script inventory
   single: shell scripts
   single: build.sh

script inventory and behavior matrix
======================================

this document fulfills the deliverables of phase 0 task 1. every shell script in
``scripts/`` has been inventoried against its actual source. no assumptions were
made without reading the corresponding file.


file manifest
-------------

.. code-block:: text

   scripts/
   ├── global_variables.sh               # centralized paths and config
   ├── lib/
   │   └── logging.sh                    # shared colored output functions
   ├── _default/
   │   ├── build.sh                      # esbuild bundle + asset copy + deploy
   │   ├── clean.sh                      # remove build + public dirs
   │   └── all.sh                        # pipeline orchestrator (clean + build)
   ├── reset-builds.sh                   # full environment reset
   ├── eslint/
   │   ├── lint.sh                       # eslint check
   │   └── lint-fix.sh                   # eslint auto-fix
   ├── typescript/
   │   └── tsc.sh                        # type check + watch mode
   └── certificates/
       └── issuer.sh                     # local CA + cert generation


npm script mappings (package.json)
----------------------------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - npm script
     - shell command
   * - ``clean:_default``
     - ``bash scripts/_default/clean.sh``
   * - ``build:_default``
     - ``bash scripts/_default/build.sh``
   * - ``all:_default``
     - ``bash scripts/_default/all.sh``
   * - ``ts-check``
     - ``bash scripts/typescript/tsc.sh --check-ts``
   * - ``watch:_default``
     - ``bash scripts/typescript/tsc.sh --watch``
   * - ``lint``
     - ``bash scripts/eslint/lint.sh``
   * - ``lint-fix``
     - ``bash scripts/eslint/lint-fix.sh``


deliverable 1: behavior matrix
-------------------------------

1. global_variables.sh
^^^^^^^^^^^^^^^^^^^^^^

:purpose: centralized repository paths and configuration variable definitions.
:shell mode: none (sourced, not executed directly).
:set flags: none (no ``set -euo pipefail``).

.. list-table::
   :header-rows: 1
   :widths: 25 25 25 25

   * - input
     - output
     - side-effect
     - exit code
   * - (sourced by another script)
     - defines 25 shell variables using ``${VAR:-default}`` pattern
     - none (no filesystem or process mutations)
     - 0 (implicit)

all variables use conditional defaults (``:``) so callers can override via
environment before sourcing.

2. lib/logging.sh
^^^^^^^^^^^^^^^^^

:purpose: shared colored terminal output functions.
:shell mode: none (sourced, not executed directly).

.. list-table::
   :header-rows: 1
   :widths: 25 25 25 25

   * - input
     - output
     - side-effect
     - exit code
   * - (sourced by another script)
     - defines 8 color constants, 6 icon constants, 7 print functions
     - none
     - 0 (implicit)

functions defined:

- ``print_header(title)`` → stderr: colored header with decorative border
- ``print_separator()`` → stderr: dimmed horizontal rule
- ``print_step(msg)`` → stdout: cyan arrow + bold message
- ``print_substep(msg)`` → stdout: dimmed bullet + message
- ``print_success(msg)`` → stdout: green check + message
- ``print_warning(msg)`` → stdout: yellow triangle + message
- ``print_error(msg)`` → stderr: red X + message

3. _default/build.sh
^^^^^^^^^^^^^^^^^^^^^

:purpose: default vhost build pipeline -- bundle tsx, copy static assets, deploy.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

**cli flags:**

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - flag
     - type
     - effect
   * - ``--build <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_BUILD_DIR``
   * - ``--public <dir>`` / ``--deploy <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_PUBLIC_DIR``
   * - ``--tld <suffix>``
     - valued (space-separated)
     - overrides ``TLD``
   * - ``--help`` / ``-h``
     - boolean
     - print usage, exit 0

**behavior matrix:**

.. list-table::
   :header-rows: 1
   :widths: 10 20 45 25

   * - step
     - input
     - side-effect
     - exit on failure
   * - 1
     - (none)
     - check ``npm`` in PATH
     - exit 1
   * - 2
     - (none)
     - check ``rsync`` in PATH
     - exit 1
   * - 3
     - ``$NODE_MODULES_DIR``, ``$ESBUILD_BIN``
     - if missing: ``npm install``
     - exit 1 (via ``set -e``)
   * - 4
     - ``$DEFAULT_BUILD_DIR``
     - ``rm -rf $DEFAULT_BUILD_DIR``
     - exit 1 (via ``set -e``)
   * - 5
     - ``$DEFAULT_BUILD_DIR``, ``$DEFAULT_PUBLIC_DIR``
     - ``mkdir -p`` both directories
     - exit 1 (via ``set -e``)
   * - 6
     - ``$DEFAULT_SOURCE_DIR/app.tsx``
     - ``npx esbuild`` bundle → ``$DEFAULT_BUILD_DIR/`` (esm, minified, code splitting)
     - exit 1 (via ``set -e``)
   * - 7
     - ``$DEFAULT_SOURCE_DIR/static/``
     - ``rsync -a`` (excludes ``.DS_Store``, ``._*``) → ``$DEFAULT_BUILD_DIR/``
     - exit 1 (via ``set -e``)
   * - 8
     - ``$DEFAULT_BUILD_DIR/``
     - ``rsync -a --delete`` (excludes ``.DS_Store``, ``._*``) → ``$DEFAULT_PUBLIC_DIR/``
     - exit 1 (via ``set -e``)
   * - 9
     - both output dirs
     - ``find -delete`` ``.DS_Store`` and ``._*`` from both dirs
     - exit 1 (via ``set -e``)

**esbuild defines injected at bundle time:**

- ``__TLD__`` ← ``$TLD``
- ``__SITE_OWNER__`` ← ``$SITE_OWNER``
- ``__SITE_OWNER_SLUG__`` ← ``$SITE_OWNER_SLUG``
- ``__SITE_GITHUB__`` ← ``$SITE_GITHUB``
- ``__SITE_INSTAGRAM__`` ← ``$SITE_INSTAGRAM``
- ``__SITE_X__`` ← ``$SITE_X``

**exports:** ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``, ``TLD``.

**exit codes:**

- 0: success
- 1: missing flag argument, unknown option, missing tool, any pipeline step failure

**note:** ``SNI_OVERRIDE`` is declared and checked but never set via any CLI flag.
this appears to be dead code (the ``--sni`` flag does not exist in the arg parser).

4. _default/clean.sh
^^^^^^^^^^^^^^^^^^^^^

:purpose: remove build and public directories.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

**cli flags:**

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - flag
     - type
     - effect
   * - ``--build <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_BUILD_DIR``
   * - ``--public <dir>`` / ``--deploy <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_PUBLIC_DIR``
   * - ``--help`` / ``-h``
     - boolean
     - print usage, exit 0

**behavior matrix:**

.. list-table::
   :header-rows: 1
   :widths: 10 20 45 25

   * - step
     - input
     - side-effect
     - exit on failure
   * - 1
     - ``$DEFAULT_PUBLIC_DIR``
     - ``rm -rf $DEFAULT_PUBLIC_DIR`` (if exists); log skip if absent
     - exit 1 (via ``set -e``)
   * - 2
     - ``$DEFAULT_BUILD_DIR``
     - ``rm -rf $DEFAULT_BUILD_DIR`` (if exists); log skip if absent
     - exit 1 (via ``set -e``)

**exports:** ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``.

**exit codes:**

- 0: success
- 1: missing flag argument, unknown option

5. _default/all.sh
^^^^^^^^^^^^^^^^^^^

:purpose: pipeline orchestrator. optional clean, then build. reports elapsed time.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

**cli flags:**

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - flag
     - type
     - effect
   * - ``--clean``
     - boolean
     - run ``clean.sh`` before ``build.sh``
   * - ``--build <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_BUILD_DIR``
   * - ``--public <dir>`` / ``--deploy <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_PUBLIC_DIR``
   * - ``--tld <suffix>``
     - valued (space-separated)
     - overrides ``TLD``
   * - ``--help`` / ``-h``
     - boolean
     - print usage, exit 0

**special behavior:** captures ``DEFAULT_PUBLIC_DIR`` and ``DEFAULT_BUILD_DIR``
*before* sourcing ``global_variables.sh`` (into ``ORIGINAL_PUBLIC_DIR`` and
``ORIGINAL_BUILD_DIR``). after sourcing, restores them unless a CLI flag override
was given. this preserves caller-provided env vars across the ``source`` call.

**behavior matrix:**

.. list-table::
   :header-rows: 1
   :widths: 10 20 45 25

   * - step
     - input
     - side-effect
     - exit on failure
   * - 1
     - ``$DO_CLEAN`` flag
     - if true: invokes ``$DEFAULT_CLEAN_SCRIPT`` as child process
     - exit 1 (propagated via ``set -e``)
   * - 2
     - (always)
     - invokes ``$DEFAULT_BUILD_SCRIPT`` as child process
     - exit 1 (propagated via ``set -e``)
   * - 3
     - (always)
     - prints elapsed time, source/build/public paths
     - (no failure path)

**exports:** ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``, ``TLD``.

**exit codes:**

- 0: success
- 1: missing flag argument, unknown option, child script failure (propagated)

6. reset-builds.sh
^^^^^^^^^^^^^^^^^^^

:purpose: full environment reset -- removes build, public, node_modules,
   package-lock.json, data, and certificates.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

**cli flags:**

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - flag
     - type
     - effect
   * - ``--build <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_BUILD_DIR``
   * - ``--public <dir>`` / ``--deploy <dir>``
     - valued (space-separated)
     - overrides ``DEFAULT_PUBLIC_DIR``
   * - ``--certs <dir>``
     - valued (space-separated)
     - overrides ``CERTS_DIR`` (and ``CERTS_CA_DIR``)
   * - ``--data <dir>``
     - valued (space-separated)
     - overrides ``DATA_DIR``
   * - ``--help`` / ``-h``
     - boolean
     - print usage, exit 0

**behavior matrix:**

.. list-table::
   :header-rows: 1
   :widths: 10 20 45 25

   * - step
     - input
     - side-effect
     - exit on failure
   * - 1
     - ``$DEFAULT_PUBLIC_DIR``
     - ``rm -rf`` (if exists)
     - exit 1 (via ``set -e``)
   * - 2
     - ``$DEFAULT_BUILD_DIR``
     - ``rm -rf`` (if exists)
     - exit 1 (via ``set -e``)
   * - 3
     - ``$ROOT_DIR/node_modules``
     - ``rm -rf`` (if exists)
     - exit 1 (via ``set -e``)
   * - 4
     - ``$ROOT_DIR/package-lock.json``
     - ``rm -f`` (if exists)
     - exit 1 (via ``set -e``)
   * - 5
     - ``$DATA_DIR``
     - ``rm -rf`` (if exists)
     - exit 1 (via ``set -e``)
   * - 6
     - ``$CERTS_DIR``
     - ``rm -rf`` (if exists)
     - exit 1 (via ``set -e``)

**exports:** ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``, ``CERTS_DIR``,
``CERTS_CA_DIR``, ``DATA_DIR``.

**exit codes:**

- 0: success
- 1: missing flag argument, unknown option

7. eslint/lint.sh
^^^^^^^^^^^^^^^^^^

:purpose: run eslint on typescript/tsx source files.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

.. list-table::
   :header-rows: 1
   :widths: 25 25 25 25

   * - input
     - output
     - side-effect
     - exit code
   * - ``$DEFAULT_SOURCE_DIR/**/*.{ts,tsx}``
     - eslint diagnostic output to stdout
     - none (read-only check)
     - 0: clean, non-zero: lint errors

**cli flags:** none (no argument parsing).

**external tools:** ``npx eslint``.

8. eslint/lint-fix.sh
^^^^^^^^^^^^^^^^^^^^^^

:purpose: run eslint with ``--fix`` on typescript/tsx source files.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

.. list-table::
   :header-rows: 1
   :widths: 25 25 25 25

   * - input
     - output
     - side-effect
     - exit code
   * - ``$DEFAULT_SOURCE_DIR/**/*.{ts,tsx}``
     - eslint diagnostic output to stdout
     - modifies source files in-place (auto-fix)
     - 0: clean, non-zero: unfixable errors

**cli flags:** none (no argument parsing).

**external tools:** ``npx eslint --fix``.

9. typescript/tsc.sh
^^^^^^^^^^^^^^^^^^^^^

:purpose: typescript utilities -- type checking and file-watch rebuild loop.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

**cli flags:**

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - flag
     - type
     - effect
   * - ``--check-ts``
     - boolean
     - run ``tsc --noEmit`` against ``$DEFAULT_TSC_CONFIG``
   * - ``--watch``
     - boolean
     - start file watcher on ``$DEFAULT_SOURCE_DIR``
   * - ``--watch-run``
     - boolean
     - single watch iteration: tsc check → eslint fix → build
   * - ``--help`` / ``-h``
     - boolean
     - print usage, exit 0

**behavior matrix (--check-ts):**

.. list-table::
   :header-rows: 1
   :widths: 10 20 45 25

   * - step
     - input
     - side-effect
     - exit on failure
   * - 1
     - ``$DEFAULT_TSC_CONFIG``
     - verify config file exists
     - exit 1
   * - 2
     - (none)
     - resolve ``tsc`` binary (local → global → install globally)
     - exit 1
   * - 3
     - ``$DEFAULT_TSC_CONFIG``
     - ``tsc --noEmit --project ...`` (read-only type check)
     - exit 1 (via ``set -e``)

**behavior matrix (--watch-run):**

.. list-table::
   :header-rows: 1
   :widths: 10 20 45 25

   * - step
     - input
     - side-effect
     - exit on failure
   * - 1
     - ``$DEFAULT_TSC_CONFIG``
     - ``tsc --noEmit``
     - exit 1 (via ``set -e``)
   * - 2
     - ``$DEFAULT_SOURCE_DIR/**/*.{ts,tsx}``
     - ``npx eslint --fix`` (non-blocking, ``|| true``)
     - (swallowed)
   * - 3
     - (none)
     - invokes ``$DEFAULT_BUILD_SCRIPT`` as child process
     - exit 1 (via ``set -e``)

**watcher priority:** watchexec → fswatch → nodemon. if none found, prompts
interactively to install one.

**external tools:** ``tsc``, ``npx eslint``, optional watchers (watchexec,
fswatch, nodemon).

**exit codes:**

- 0: success
- 1: no action specified, missing config, tsc failure, build failure

10. certificates/issuer.sh
^^^^^^^^^^^^^^^^^^^^^^^^^^^

:purpose: local development TLS certificate management with self-signed CA.
:shell mode: ``set -euo pipefail``.
:sources: ``global_variables.sh``, ``logging.sh``.

**commands:**

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - command
     - description
   * - ``generate <host> [host ...]``
     - generate TLS certificates signed by local CA
   * - ``status <host> [host ...]``
     - show certificate subject, expiry, days remaining
   * - ``help`` / ``--help`` / ``-h``
     - print usage

**global flags (must precede command):**

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - flag
     - type
     - effect
   * - ``--certs <dir>``
     - valued (space-separated)
     - overrides ``CERTS_DIR`` (and ``CERTS_CA_DIR``)
   * - ``--drop-ca``
     - boolean
     - remove and regenerate CA before issuing
   * - ``--ca-cn <cn>``
     - valued (space-separated)
     - set CA common name (requires ``--drop-ca``)

**behavior matrix (generate):**

.. list-table::
   :header-rows: 1
   :widths: 10 20 45 25

   * - step
     - input
     - side-effect
     - exit on failure
   * - 1
     - ``$CERTS_CA_DIR``
     - if CA missing or ``--drop-ca``: generate RSA 4096 root key + self-signed cert
     - exit 1 (via ``set -e``)
   * - 2
     - each ``<host>``
     - per host: mkdir, generate RSA 2048 key, CSR, signed cert, cleanup CSR/CNF
     - exit 1 (via ``set -e``)

**filesystem mutations per host:**

- creates ``$CERTS_DIR/<host>/fullchain.pem``
- creates ``$CERTS_DIR/<host>/privkey.pem``
- temporary files created and removed: ``server.csr``, ``leaf.cnf``

**external tools:** ``openssl``.

**exit codes:**

- 0: success
- 1: missing hostname, ``--ca-cn`` without ``--drop-ca``, unknown command


deliverable 2: dependency graph
-------------------------------

script call hierarchy:

.. code-block:: text

   global_variables.sh ◄── sourced by ALL executable scripts
   lib/logging.sh      ◄── sourced by ALL executable scripts (via $LOGGING_LIB_SCRIPT)

   _default/all.sh
   ├── (sources) global_variables.sh
   ├── (sources) lib/logging.sh
   ├── (calls)   _default/clean.sh      [conditional: --clean flag]
   └── (calls)   _default/build.sh      [always]

   _default/build.sh
   ├── (sources) global_variables.sh
   ├── (sources) lib/logging.sh
   ├── (exec)    npm install             [conditional: node_modules missing]
   ├── (exec)    npx esbuild             [always]
   └── (exec)    rsync                   [always, x2]

   _default/clean.sh
   ├── (sources) global_variables.sh
   └── (sources) lib/logging.sh

   typescript/tsc.sh
   ├── (sources) global_variables.sh
   ├── (sources) lib/logging.sh
   ├── (exec)    tsc --noEmit            [--check-ts or --watch-run]
   ├── (exec)    npx eslint --fix        [--watch-run only, non-blocking]
   ├── (calls)   _default/build.sh       [--watch-run only]
   └── (exec)    watchexec|fswatch|nodemon [--watch only]

   eslint/lint.sh
   ├── (sources) global_variables.sh
   ├── (sources) lib/logging.sh
   └── (exec)    npx eslint

   eslint/lint-fix.sh
   ├── (sources) global_variables.sh
   ├── (sources) lib/logging.sh
   └── (exec)    npx eslint --fix

   reset-builds.sh
   ├── (sources) global_variables.sh
   └── (sources) lib/logging.sh

   certificates/issuer.sh
   ├── (sources) global_variables.sh
   ├── (sources) lib/logging.sh
   └── (exec)    openssl

external tool dependency summary:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - tool
     - required by
   * - ``npm``
     - build.sh (install + npx), lint.sh, lint-fix.sh, tsc.sh
   * - ``npx``
     - build.sh (esbuild), lint.sh, lint-fix.sh, tsc.sh (eslint)
   * - ``esbuild``
     - build.sh (via npx, from node_modules)
   * - ``rsync``
     - build.sh (static copy + deploy)
   * - ``tsc``
     - tsc.sh (from node_modules or global)
   * - ``eslint``
     - lint.sh, lint-fix.sh, tsc.sh --watch-run (via npx)
   * - ``openssl``
     - issuer.sh
   * - ``watchexec`` / ``fswatch`` / ``nodemon``
     - tsc.sh --watch (optional, one required)


deliverable 3: environment variable catalog
--------------------------------------------

variables defined in global_variables.sh
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

all variables use the ``${VAR:-default}`` pattern (callers can pre-set via env).

**repository paths:**

.. list-table::
   :header-rows: 1
   :widths: 25 30 45

   * - variable
     - default
     - consumers
   * - ``ROOT_DIR``
     - computed: ``$(dirname $0)/..``
     - all scripts (base for all paths)
   * - ``SCRIPTS_DIR``
     - ``$ROOT_DIR/scripts``
     - all scripts (base for script paths)
   * - ``LOGGING_LIB_SCRIPT``
     - ``$SCRIPTS_DIR/lib/logging.sh``
     - all scripts (sourced for output functions)

**site identity:**

.. list-table::
   :header-rows: 1
   :widths: 25 20 55

   * - variable
     - default
     - consumers
   * - ``SNI``
     - ``unknown``
     - build.sh (used in ``DEFAULT_PUBLIC_DIR`` path: ``public/$SNI$TLD``)
   * - ``TLD``
     - ``.local``
     - build.sh (esbuild ``--define:__TLD__``), all.sh
   * - ``SITE_OWNER``
     - ``Unknown``
     - build.sh (esbuild ``--define:__SITE_OWNER__``)
   * - ``SITE_OWNER_SLUG``
     - ``unknown``
     - build.sh (esbuild ``--define:__SITE_OWNER_SLUG__``)
   * - ``SITE_OWNER_SLUG_REV``
     - ``unknown``
     - (not consumed by any script -- candidate for deprecation review)
   * - ``SITE_GITHUB``
     - ``/#``
     - build.sh (esbuild ``--define:__SITE_GITHUB__``)
   * - ``SITE_INSTAGRAM``
     - ``/#``
     - build.sh (esbuild ``--define:__SITE_INSTAGRAM__``)
   * - ``SITE_X``
     - ``/#``
     - build.sh (esbuild ``--define:__SITE_X__``)

**build pipeline paths:**

.. list-table::
   :header-rows: 1
   :widths: 25 30 45

   * - variable
     - default
     - consumers
   * - ``DEFAULT_SOURCE_DIR``
     - ``$ROOT_DIR/src``
     - build.sh, lint.sh, lint-fix.sh, tsc.sh
   * - ``DEFAULT_BUILD_DIR``
     - ``$ROOT_DIR/build/src``
     - build.sh, clean.sh, all.sh, reset-builds.sh
   * - ``DEFAULT_PUBLIC_DIR``
     - ``$ROOT_DIR/public/$SNI$TLD``
     - build.sh, clean.sh, all.sh, reset-builds.sh

**script references:**

.. list-table::
   :header-rows: 1
   :widths: 25 40 35

   * - variable
     - default
     - consumers
   * - ``DEFAULT_ALL_SCRIPT``
     - ``$SCRIPTS_DIR/_default/all.sh``
     - (not consumed by any script -- available for external callers)
   * - ``DEFAULT_BUILD_SCRIPT``
     - ``$SCRIPTS_DIR/_default/build.sh``
     - all.sh, tsc.sh --watch-run
   * - ``DEFAULT_CLEAN_SCRIPT``
     - ``$SCRIPTS_DIR/_default/clean.sh``
     - all.sh
   * - ``DEFAULT_TSC_SCRIPT``
     - ``$SCRIPTS_DIR/typescript/tsc.sh``
     - tsc.sh --watch (self-reference for watcher callback)
   * - ``DEFAULT_TSC_CONFIG``
     - ``$ROOT_DIR/tsconfig.tsc.config``
     - tsc.sh
   * - ``ESLINT_LINT_SCRIPT``
     - ``$SCRIPTS_DIR/eslint/lint.sh``
     - (not consumed by any script -- available for external callers)
   * - ``ESLINT_LINT_FIX_SCRIPT``
     - ``$SCRIPTS_DIR/eslint/lint-fix.sh``
     - (not consumed by any script -- available for external callers)
   * - ``CERT_ISSUER_SCRIPT``
     - ``$SCRIPTS_DIR/certificates/issuer.sh``
     - (not consumed by any script -- available for external callers)

**infrastructure paths:**

.. list-table::
   :header-rows: 1
   :widths: 25 30 45

   * - variable
     - default
     - consumers
   * - ``CERTS_DIR``
     - ``$ROOT_DIR/certs``
     - issuer.sh, reset-builds.sh
   * - ``CERTS_CA_DIR``
     - ``$CERTS_DIR/ca``
     - issuer.sh, reset-builds.sh
   * - ``CERTS_CA_CN``
     - ``com.unknown.root.ca``
     - issuer.sh
   * - ``DATA_DIR``
     - ``$ROOT_DIR/data``
     - reset-builds.sh

variables exported by scripts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - script
     - exports
   * - build.sh
     - ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``, ``TLD``
   * - clean.sh
     - ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``
   * - all.sh
     - ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``, ``TLD``
   * - reset-builds.sh
     - ``DEFAULT_BUILD_DIR``, ``DEFAULT_PUBLIC_DIR``, ``CERTS_DIR``,
       ``CERTS_CA_DIR``, ``DATA_DIR``
   * - issuer.sh
     - ``CERTS_DIR``, ``CERTS_CA_DIR``


observations and anomalies
--------------------------

1. **dead code in build.sh**: ``SNI_OVERRIDE`` is declared and checked
   (``if [[ -n "$SNI_OVERRIDE" ]]``) but no ``--sni`` flag exists in the arg
   parser. the variable is never set.

2. **SITE_OWNER_SLUG_REV unused**: defined in ``global_variables.sh`` but not
   consumed by any script or injected as an esbuild define.

3. **script reference variables unused internally**: ``DEFAULT_ALL_SCRIPT``,
   ``ESLINT_LINT_SCRIPT``, ``ESLINT_LINT_FIX_SCRIPT``, and
   ``CERT_ISSUER_SCRIPT`` are defined in ``global_variables.sh`` but never
   consumed by other scripts. they exist for external callers (e.g., npm scripts
   or manual invocation).

4. **inconsistent metadata cleanup**: build.sh uses both ``rsync --exclude`` and
   ``find -delete`` for ``.DS_Store``/``._*``. the rsync exclude prevents
   copying them, while the find delete removes any that might already exist in
   the target. this is belt-and-suspenders but means the find step should
   theoretically find nothing.

5. **all.sh env preservation**: the ``ORIGINAL_*`` pattern in all.sh is a
   workaround for ``global_variables.sh`` unconditionally setting defaults. if
   ``global_variables.sh`` were refactored to truly conditional-only assignment,
   this workaround would be unnecessary.

6. **tsc.sh --watch-run swallows eslint errors**: the ``|| true`` after
   ``npx eslint --fix`` means lint errors do not block the build.

7. **issuer.sh status uses GNU date**: the ``date -d`` syntax in ``cmd_status``
   is GNU/Linux-specific and will fail on macOS (which uses ``date -j -f``).
   this is a portability bug.


acceptance criteria verification
--------------------------------

- ✓ all scripts in ``scripts/`` fully documented with input/output/side-effect tables.
- ✓ no undocumented variable or filesystem mutation remains.
- ✓ document reviewed against actual script source (not assumptions).

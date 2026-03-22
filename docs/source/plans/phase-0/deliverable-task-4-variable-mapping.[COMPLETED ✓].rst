.. meta::
   :title: variable mapping deliverable
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; deliverable; variable mapping
   single: environment variables
   single: PHOSPHOR_ prefix

variable mapping from global_variables.sh
==========================================

this document maps every variable in ``scripts/global_variables.sh`` to its
phosphor equivalent: a CLI flag, an environment variable, a config-file key,
or a deprecation note.

source file: ``scripts/global_variables.sh`` (46 lines, 25 variables).


deliverable 1: complete variable mapping table
------------------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 22 20 18 40

   * - shell variable
     - phosphor mapping
     - scope
     - notes
   * - ``ROOT_DIR``
     - ``--project=<path>``
     - build, clean, doctor
     - auto-detected from cwd in create. explicit via ``--project`` in
       build/clean/doctor. env: ``PHOSPHOR_PROJECT``.
   * - ``SCRIPTS_DIR``
     - deprecated
     - --
     - internal shell pipeline path. phosphor replaces the scripts entirely.
       during phase 3 compatibility mode, phosphor locates scripts relative to
       the project root internally (not user-configurable).
   * - ``LOGGING_LIB_SCRIPT``
     - deprecated
     - --
     - internal shell logging library. phosphor has its own ``log.c`` module.
   * - ``SNI``
     - ``--name=<slug>``
     - create
     - the site name identifier. required flag for create. env:
       ``PHOSPHOR_NAME``. also used to derive the default deploy path
       (``public/<name><tld>``).
   * - ``TLD``
     - ``--tld=<suffix>``
     - create, build
     - injected as ``__TLD__`` esbuild define. env: ``PHOSPHOR_TLD``.
       default: ``.host`` (from manifest defaults).
   * - ``SITE_OWNER``
     - ``--owner=<name>``
     - create
     - injected as ``__SITE_OWNER__`` esbuild define. env:
       ``PHOSPHOR_OWNER``.
   * - ``SITE_OWNER_SLUG``
     - ``--owner-slug=<slug>``
     - create
     - injected as ``__SITE_OWNER_SLUG__`` esbuild define. env:
       ``PHOSPHOR_OWNER_SLUG``. **note:** the masterplan draft mapped
       ``SITE_OWNER_SLUG_REV`` to ``--owner-slug`` but the actual esbuild
       define uses ``SITE_OWNER_SLUG`` (see DR-SLUG below).
   * - ``SITE_OWNER_SLUG_REV``
     - deprecated
     - --
     - never consumed by any script or esbuild define (confirmed in task-1
       inventory). the masterplan draft incorrectly mapped this to
       ``--owner-slug``. see DR-SLUG.
   * - ``SITE_GITHUB``
     - ``--github=<url>``
     - create
     - injected as ``__SITE_GITHUB__`` esbuild define. env:
       ``PHOSPHOR_GITHUB``. type: ``url``.
   * - ``SITE_INSTAGRAM``
     - ``--instagram=<url>``
     - create
     - injected as ``__SITE_INSTAGRAM__`` esbuild define. env:
       ``PHOSPHOR_INSTAGRAM``. type: ``url``.
   * - ``SITE_X``
     - ``--x=<url>``
     - create
     - injected as ``__SITE_X__`` esbuild define. env: ``PHOSPHOR_X``.
       type: ``url``.
   * - ``DEFAULT_SOURCE_DIR``
     - config-only
     - build
     - project layout convention: ``<project>/src``. not a CLI flag. may
       become a config-file key (``build.source_dir``) if customization is
       needed. for v1, hardcoded as ``src/`` relative to project root.
   * - ``DEFAULT_BUILD_DIR``
     - config-only
     - build
     - intermediate build output: ``<project>/build/src``. managed internally
       by phosphor build. not user-facing. may become a config-file key
       (``build.build_dir``) if customization is needed.
   * - ``DEFAULT_PUBLIC_DIR``
     - ``--deploy-at=<path>``
     - build
     - final deploy output. defaults to ``public/<name><tld>`` (derived from
       ``--name`` and ``--tld``). env: ``PHOSPHOR_DEPLOY_AT``.
   * - ``DEFAULT_ALL_SCRIPT``
     - deprecated
     - --
     - script reference. phosphor replaces the pipeline orchestrator.
   * - ``DEFAULT_BUILD_SCRIPT``
     - deprecated (phase 3: internal)
     - --
     - script reference. during phase 3 compatibility mode, phosphor locates
       this script internally. after phase 4 internalization, fully deprecated.
   * - ``DEFAULT_CLEAN_SCRIPT``
     - deprecated (phase 3: internal)
     - --
     - script reference. same lifecycle as ``DEFAULT_BUILD_SCRIPT``.
   * - ``DEFAULT_TSC_SCRIPT``
     - out of scope
     - --
     - typescript tooling. phosphor does not manage typescript compilation.
       remains as a standalone shell script or user-managed tool.
   * - ``DEFAULT_TSC_CONFIG``
     - out of scope
     - --
     - typescript config path. same rationale as ``DEFAULT_TSC_SCRIPT``.
   * - ``ESLINT_LINT_SCRIPT``
     - out of scope
     - --
     - eslint tooling. phosphor does not manage linting.
   * - ``ESLINT_LINT_FIX_SCRIPT``
     - out of scope
     - --
     - eslint tooling. same rationale.
   * - ``CERT_ISSUER_SCRIPT``
     - out of scope
     - --
     - certificate management. phosphor does not manage certificates.
   * - ``CERTS_DIR``
     - out of scope
     - --
     - certificate storage path. not in phosphor's domain.
   * - ``CERTS_CA_DIR``
     - out of scope
     - --
     - CA directory. not in phosphor's domain.
   * - ``CERTS_CA_CN``
     - out of scope
     - --
     - CA common name. not in phosphor's domain.
   * - ``DATA_DIR``
     - out of scope
     - --
     - data storage. only consumed by ``reset-builds.sh``. not in phosphor's
       domain. a future ``phosphor clean --all`` could be considered but is
       not planned for v1.


deliverable 2: decision records
--------------------------------

**DR-SLUG: SITE_OWNER_SLUG vs SITE_OWNER_SLUG_REV**

the masterplan migration plan (line 1303) maps ``site_owner_slug_rev`` to
``--owner-slug``. however:

- ``SITE_OWNER_SLUG_REV`` is defined in ``global_variables.sh`` but never
  consumed by any script (confirmed in task-1 deliverable, "observations
  and anomalies" item 2).
- ``SITE_OWNER_SLUG`` is the variable actually injected as
  ``__SITE_OWNER_SLUG__`` via esbuild define in ``build.sh`` (line 128).
- no ``__SITE_OWNER_SLUG_REV__`` esbuild define exists.

decision: ``SITE_OWNER_SLUG`` → ``--owner-slug``. ``SITE_OWNER_SLUG_REV`` is
deprecated with no replacement. the masterplan reference is a documentation
error that this mapping corrects.

**DR-SRC: DEFAULT_SOURCE_DIR is config-only, not a flag**

``DEFAULT_SOURCE_DIR`` defaults to ``$ROOT_DIR/src`` and is used by build.sh
(esbuild entry point), lint scripts (glob target), and tsc.sh (type check
target).

the source directory is a project layout convention, not a per-invocation
choice. making it a CLI flag would add noise to every ``phosphor build``
call for no practical benefit.

decision: not a CLI flag. for v1, hardcoded as ``src/`` relative to project
root. if customization is needed later, add a config-file key
``build.source_dir`` (not a flag).

**DR-BUILD: DEFAULT_BUILD_DIR is internal, not user-facing**

``DEFAULT_BUILD_DIR`` defaults to ``$ROOT_DIR/build/src`` and is the
intermediate esbuild output directory. the user cares about the final deploy
location (``--deploy-at``), not the intermediate build directory.

decision: managed internally by phosphor. not exposed as a CLI flag or
config key in v1. phosphor uses ``<project>/build/`` as the intermediate
directory unconditionally.

**DR-DEPLOY: DEFAULT_PUBLIC_DIR derives from --name and --tld**

``DEFAULT_PUBLIC_DIR`` defaults to ``$ROOT_DIR/public/$SNI$TLD``. this embeds
the site name and tld in the deploy path.

decision: ``--deploy-at=<path>`` provides explicit override. when omitted,
phosphor build derives the default as ``public/<name><tld>`` using the
``--name`` and ``--tld`` values (or their env/config equivalents).

**DR-CERTS: certificate and data variables are out of scope**

``CERTS_DIR``, ``CERTS_CA_DIR``, ``CERTS_CA_CN``, and ``DATA_DIR`` are used
by ``issuer.sh`` and ``reset-builds.sh`` respectively. neither script is in
phosphor's build/create/clean domain.

decision: out of scope. these scripts and variables remain independent of
phosphor. if a future ``phosphor doctor`` check wants to verify certificate
presence, it can read the project layout convention (``certs/``) without
needing a flag.

**DR-SCRIPTS: script reference variables deprecated in stages**

six variables (``DEFAULT_ALL_SCRIPT``, ``DEFAULT_BUILD_SCRIPT``,
``DEFAULT_CLEAN_SCRIPT``, ``DEFAULT_TSC_SCRIPT``, ``ESLINT_LINT_SCRIPT``,
``ESLINT_LINT_FIX_SCRIPT``) are paths to shell scripts.

decision:

- ``DEFAULT_BUILD_SCRIPT`` and ``DEFAULT_CLEAN_SCRIPT``: used internally by
  phosphor during phase 3 compatibility mode. phosphor locates them via
  project layout convention (``scripts/_default/``), not via user flag.
  fully deprecated after phase 4 internalization.
- ``DEFAULT_ALL_SCRIPT``: replaced by ``phosphor build --clean-first``.
  deprecated immediately.
- ``DEFAULT_TSC_SCRIPT``, ``ESLINT_LINT_SCRIPT``, ``ESLINT_LINT_FIX_SCRIPT``,
  ``CERT_ISSUER_SCRIPT``: out of phosphor's scope. remain as standalone tools.


deliverable 3: new flags assessment
-------------------------------------

no new flags are required beyond the current appendices (A-E).

all esbuild-injected defines have corresponding create flags:

.. list-table::
   :header-rows: 1
   :widths: 30 25 25

   * - esbuild define
     - shell variable
     - phosphor flag
   * - ``__TLD__``
     - ``TLD``
     - ``--tld``
   * - ``__SITE_OWNER__``
     - ``SITE_OWNER``
     - ``--owner``
   * - ``__SITE_OWNER_SLUG__``
     - ``SITE_OWNER_SLUG``
     - ``--owner-slug``
   * - ``__SITE_GITHUB__``
     - ``SITE_GITHUB``
     - ``--github``
   * - ``__SITE_INSTAGRAM__``
     - ``SITE_INSTAGRAM``
     - ``--instagram``
   * - ``__SITE_X__``
     - ``SITE_X``
     - ``--x``

all build.sh CLI flags have corresponding phosphor build flags:

.. list-table::
   :header-rows: 1
   :widths: 25 25

   * - build.sh flag
     - phosphor flag
   * - ``--build <dir>``
     - (internal, not exposed)
   * - ``--public <dir>`` / ``--deploy <dir>``
     - ``--deploy-at=<path>``
   * - ``--tld <suffix>``
     - ``--tld=<suffix>``

all clean.sh CLI flags have corresponding phosphor clean flags:

.. list-table::
   :header-rows: 1
   :widths: 25 25

   * - clean.sh flag
     - phosphor flag
   * - ``--build <dir>``
     - (internal, not exposed)
   * - ``--public <dir>`` / ``--deploy <dir>``
     - ``--project=<path>`` (phosphor clean operates on project, not individual dirs)

all.sh ``--clean`` maps to ``phosphor build --clean-first``.


environment variable prefix convention
----------------------------------------

all phosphor environment variables use the ``PHOSPHOR_`` prefix:

.. list-table::
   :header-rows: 1
   :widths: 25 25 50

   * - phosphor env var
     - maps to flag
     - shell variable origin
   * - ``PHOSPHOR_NAME``
     - ``--name``
     - ``SNI``
   * - ``PHOSPHOR_TLD``
     - ``--tld``
     - ``TLD``
   * - ``PHOSPHOR_OWNER``
     - ``--owner``
     - ``SITE_OWNER``
   * - ``PHOSPHOR_OWNER_SLUG``
     - ``--owner-slug``
     - ``SITE_OWNER_SLUG``
   * - ``PHOSPHOR_GITHUB``
     - ``--github``
     - ``SITE_GITHUB``
   * - ``PHOSPHOR_INSTAGRAM``
     - ``--instagram``
     - ``SITE_INSTAGRAM``
   * - ``PHOSPHOR_X``
     - ``--x``
     - ``SITE_X``
   * - ``PHOSPHOR_PROJECT``
     - ``--project``
     - ``ROOT_DIR``
   * - ``PHOSPHOR_DEPLOY_AT``
     - ``--deploy-at``
     - ``DEFAULT_PUBLIC_DIR``
   * - ``PHOSPHOR_LOG``
     - (no flag, trace-only)
     - (new, no shell origin)

precedence: CLI flags > env vars > project config > manifest defaults.


mapping summary by category
-----------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 10 75

   * - category
     - count
     - variables
   * - CLI flag
     - 9
     - SNI, TLD, SITE_OWNER, SITE_OWNER_SLUG, SITE_GITHUB, SITE_INSTAGRAM,
       SITE_X, DEFAULT_PUBLIC_DIR, ROOT_DIR
   * - config-only
     - 2
     - DEFAULT_SOURCE_DIR, DEFAULT_BUILD_DIR
   * - deprecated
     - 6
     - SCRIPTS_DIR, LOGGING_LIB_SCRIPT, SITE_OWNER_SLUG_REV,
       DEFAULT_ALL_SCRIPT, DEFAULT_BUILD_SCRIPT*, DEFAULT_CLEAN_SCRIPT*
   * - out of scope
     - 8
     - DEFAULT_TSC_SCRIPT, DEFAULT_TSC_CONFIG, ESLINT_LINT_SCRIPT,
       ESLINT_LINT_FIX_SCRIPT, CERT_ISSUER_SCRIPT, CERTS_DIR, CERTS_CA_DIR,
       CERTS_CA_CN, DATA_DIR

(*) deprecated after phase 4; internal-only during phase 3.


acceptance criteria verification
---------------------------------

- ✓ every exported variable in global_variables.sh has a mapping entry
  (25 variables, all accounted for).
- ✓ no variable left as "tbd" -- each has a concrete decision (flag, env,
  config-only, deprecated, or out of scope).
- ✓ new flags proposed: none needed. all existing appendix flags cover the
  mapped variables. the typed flag contract is consistent.

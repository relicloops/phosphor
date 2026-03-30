.. meta::
   :title: glow command -- embedded cathode-landing template
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-03-29

.. index::
   single: glow command
   single: embedded template
   single: cathode-landing
   pair: command; glow

glow command -- embedded cathode-landing template
==================================================

.. note::

   this plan supersedes ``phase-init/``. the ``init`` command described in
   phase-init is now ``glow``. the phase-init task files remain as reference
   but the implementation follows this plan.

problem
-------

phosphor currently requires an external template directory or remote URL to
scaffold projects via ``phosphor create``. the cathode-landing template lives
in ``templates/cathode-landing/`` (40 files, ~60 KB) and must be fetched or
pointed to explicitly. users should be able to scaffold a Cathode landing page
with a single command and zero external dependencies.

solution
--------

embed the cathode-landing template as C static byte arrays compiled directly
into the phosphor binary. add ``phosphor glow --name <project-name>`` as a
dedicated command that scaffolds from the embedded template using the existing
create pipeline (manifest loading, variable merge, staging, rendering, writer).

the ``glow`` name fits the phosphor metaphor -- phosphor coating produces the
visible glow from a neon tube.

CLI contract
------------

.. code-block:: text

   phosphor glow --name=<project-name> [--output=<path>] [--force] [--dry-run] [--verbose]

flags:

.. list-table::
   :header-rows: 1
   :widths: 20 12 10 58

   * - flag
     - type
     - form
     - description
   * - ``--name=``
     - string
     - valued
     - project name (required, maps to ``<<name>>`` variable)
   * - ``--output=``
     - path
     - valued
     - output directory (default: ``./<name>``)
   * - ``--description=``
     - string
     - valued
     - project description (maps to ``<<project_description>>``)
   * - ``--github-url=``
     - url
     - valued
     - GitHub URL (maps to ``<<github_url>>``)
   * - ``--force``
     - bool
     - action
     - overwrite existing destination
   * - ``--dry-run``
     - bool
     - action
     - show plan without writing
   * - ``--verbose``
     - bool
     - action
     - verbose output

variables come from the embedded ``template.phosphor.toml`` manifest. the
``--name=`` flag is required; other variables use defaults from the manifest
unless overridden by flags.

depends on
----------

- phase 2 complete (create command pipeline: manifest, var_merge, staging,
  planner, renderer, writer)
- phase 4 complete (native build/clean commands)
- phase 5 complete (filters: pcre2 regex, glob excludes)

deliverables
------------

1. code generation tool that reads ``templates/cathode-landing/`` and outputs
   C source files with embedded content
2. embedded file registry (path-to-buffer lookup API)
3. ``PHOSPHOR_CMD_GLOW`` added to command enum
4. ``glow_cmd.c`` command handler wiring embedded buffers into create pipeline
5. argspec array and dispatch case
6. unit tests for embedded registry and glow command

tasks
-----

task 1 -- template preparation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

edit cathode-landing template files to ensure all site-specific values use
``<<placeholder>>`` syntax. verify the existing ``template.phosphor.toml``
manifest covers all variables, operations, and filters.

current template variables (from ``template.phosphor.toml``):

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - variable
     - type
     - default
   * - ``name``
     - string
     - (required, no default)
   * - ``project_description``
     - string
     - ``A Cathode JSX website``
   * - ``github_url``
     - url
     - ``https://github.com``

files to embed (40 files, ~60 KB total):

- 10 TSX components (``src/components/``, ``src/pages/``)
- 3 TypeScript scripts (``src/scripts/``)
- 14 CSS files (``src/static/css/``)
- 2 HTML files (``src/static/index.html``, ``notfound.html``)
- 1 SVG (``src/static/favicon.svg``)
- config files: ``package.json``, ``justfile``, ``eslint.config.js``,
  ``tsconfig.json``, ``tsconfig.tsc.config``, ``.cathode``
- build scripts: ``scripts/_default/`` (4 files)
- ``template.phosphor.toml`` (embedded manifest)

acceptance criteria:

- [x] all ``<<placeholder>>`` references resolve against manifest variables
- [x] ``phosphor create --source=templates/cathode-landing --name=test``
      produces identical output to what ``phosphor glow --name=test`` will produce
- [x] no hardcoded site-specific values remain in template files

task 2 -- C buffer embedding
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

convert template files into ``static const unsigned char[]`` arrays compiled
into the phosphor binary. build-time code generation reads
``templates/cathode-landing/`` and emits C source + header files.

generated file structure:

.. code-block:: text

   src/template/
     embedded_registry.h      -- path-to-buffer lookup API
     embedded_registry.c      -- lookup implementation + file table
     embedded_data.c           -- all byte arrays (generated)

registry API:

.. code-block:: c

   typedef struct {
       const char           *path;       /* relative path, e.g. "src/app.tsx" */
       const unsigned char  *data;       /* buffer pointer */
       size_t                size;       /* buffer size in bytes */
       bool                  is_binary;  /* true for SVG, images */
   } ph_embedded_file_t;

   const ph_embedded_file_t *ph_embedded_lookup(const char *path);
   size_t                    ph_embedded_count(void);
   const ph_embedded_file_t *ph_embedded_list(void);

approach: Python script as meson ``custom_target``. reads
``templates/cathode-landing/``, classifies files as text/binary based on
``[filters].binary_ext`` from the manifest, emits ``embedded_data.c`` with one
``static const unsigned char[]`` per file and ``embedded_registry.c`` with the
lookup table.

``templates/cathode-landing/`` remains in the repo as the editable source of
truth. the custom_target regenerates the C byte arrays on every build when
template files change -- edit the template normally, rebuild, and the binary
picks up the changes automatically.

acceptance criteria:

- [x] every template file has a corresponding C buffer
- [x] buffer registry provides lookup by relative path
- [x] binary files (SVG) embedded as raw bytes, text files preserve exact content
- [x] code generation is deterministic (sorted output, reproducible)
- [x] meson custom_target integrates generation step
- [x] ``phosphor version`` still works after embedding (smoke test)
- [x] binary size increase documented (~60 KB expected)

task 3 -- glow command implementation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

add ``phosphor glow`` to the CLI. the command reads the embedded manifest and
file buffers, feeds them into the existing create pipeline (var_merge, planner,
renderer, writer).

new files:

- ``src/commands/glow_cmd.c`` -- glow command handler
- ``include/phosphor/embedded.h`` -- public header for embedded registry

modified files:

- ``include/phosphor/commands.h`` -- add ``PHOSPHOR_CMD_GLOW``
- ``src/commands/phosphor_commands.c`` -- add ``glow_specs[]`` and table entry
- ``src/cli/cli_dispatch.c`` -- add case for ``PHOSPHOR_CMD_GLOW``
- ``src/cli/cli_help.c`` -- add glow command help text
- ``meson.build`` -- add new source files + custom_target

pipeline:

1. parse args and validate ``--name=`` (required)
2. load embedded ``template.phosphor.toml`` from registry
3. build variable map: ``--name=`` + ``--description=`` + ``--github-url=`` + defaults
4. feed manifest + variables into existing var_merge pipeline
5. planner builds operation list from embedded ``[[ops]]``
6. for each operation, renderer reads file content from embedded buffers
   (instead of filesystem)
7. writer commits rendered files to ``--output=`` / ``./<name>``
8. if ``[hooks]`` present: run ``npm install`` post-create (same as create)

the key integration point is teaching the renderer/writer to read from
``ph_embedded_file_t`` buffers instead of ``ph_fs_read()``. options:

a. **virtual source adapter**: abstract the source read behind a function pointer
   (``read_fn``). create command uses fs read, glow uses buffer read. minimal
   disruption to existing code
b. **write embedded to temp dir, then run create**: simpler but wastes I/O.
   not recommended for a tool that values efficiency

option (a) is preferred.

acceptance criteria:

- [x] ``phosphor glow --name=mysite`` scaffolds a Cathode landing page
- [x] output matches ``phosphor create --source templates/cathode-landing --name mysite``
- [x] ``--dry-run`` shows planned operations without writing
- [x] ``--force`` overwrites existing destination
- [x] ``--verbose`` shows per-file operations
- [x] ``<<name>>``, ``<<project_description>>``, ``<<github_url>>`` replaced in output
- [x] binary files (SVG) copied without rendering
- [x] ``[hooks]`` (npm install) executed post-create
- [x] missing ``--name=`` exits with code 2 (``PH_ERR_USAGE``)
- [x] destination exists without ``--force`` exits with code 4 (``PH_ERR_FS``)

task 4 -- tests and cleanup
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- unit tests for embedded registry (lookup, count, list, missing path)
- unit tests for glow command (flag parsing, variable resolution)
- integration test: ``phosphor glow --name=test-glow`` golden output comparison
- update ``doctor`` command to report embedded template version

acceptance criteria:

- [ ] embedded registry unit tests pass
- [ ] glow command unit tests pass
- [ ] integration test matches golden output
- [ ] ``phosphor doctor`` shows embedded template info

references
----------

- ``phase-init/`` task files (superseded reference material)
- ``templates/cathode-landing/template.phosphor.toml`` (source manifest)
- ``docs/source/reference/template-manifest-schema.rst`` (manifest schema)
- masterplan section: configuration and template model

.. meta::
   :title: init command with interactive shell
   :tags: #neonsignal, #phosphor
   :status: draft
   :updated: 2026-02-20

.. index::
   triple: phase init; task; init command
   single: interactive prompting
   single: stdin
   pair: command; init

task 3 -- init command with interactive shell
===============================================

.. note::

   parent plan: `../masterplan.[ACTIVE ▸].rst`

objective
---------

add the ``phosphor init`` command to the CLI. the command scaffolds a Cathode
project from the embedded C buffer template, prompting the user interactively
for variable values via stdin. this is the first interactive command in phosphor.

depends on
----------

- task-2-c-buffer-embedding (buffers available)
- phase-5 complete (remote template infrastructure, reused for task 4)

deliverables
------------

1. ``PHOSPHOR_CMD_INIT`` added to command enum (ID 7)
2. ``init_specs[]`` argspec array
3. dispatch case in ``cli_dispatch.c``
4. ``init_cmd.c`` -- full interactive scaffolding pipeline
5. interactive prompt module (``src/io/prompt.c`` + ``include/phosphor/prompt.h``)
6. prompt flow driven by embedded ``template.phosphor.toml`` variables

flags
-----

.. list-table::
   :header-rows: 1
   :widths: 25 15 10 50

   * - flag
     - type
     - form
     - description
   * - ``--name``
     - string
     - valued
     - project name (required)
   * - ``--output``
     - path
     - valued
     - output directory (default: current dir)
   * - ``--tld``
     - enum
     - valued
     - domain TLD (``.host``, ``.com``, ``.io``)
   * - ``--owner``
     - string
     - valued
     - site owner name
   * - ``--github``
     - url
     - valued
     - GitHub URL
   * - ``--instagram``
     - url
     - valued
     - Instagram URL
   * - ``--x``
     - url
     - valued
     - X/Twitter URL
   * - ``--force``
     - bool
     - action
     - overwrite existing destination
   * - ``--dry-run``
     - bool
     - action
     - show plan without writing
   * - ``--yes``
     - bool
     - action
     - skip interactive prompts, use defaults
   * - ``--verbose``
     - bool
     - action
     - verbose output

acceptance criteria
-------------------

- [ ] ``phosphor init --name=mysite`` scaffolds a project interactively
- [ ] each variable prompts with name, type, default value
- [ ] enum variables show available choices
- [ ] secret variables mask input
- [ ] ``--yes`` skips all prompts, uses defaults + CLI flag values
- [ ] all required variables without defaults must be provided (prompt or flag)
- [ ] ``--dry-run`` shows planned operations without writing
- [ ] ``--force`` overwrites existing destination
- [ ] output project contains rendered template content from C buffers
- [ ] ``<<placeholder>>`` values replaced in rendered output
- [ ] binary files (SVG) copied without rendering
- [ ] empty input at prompt accepts default value
- [ ] ctrl-D (EOF) at prompt aborts gracefully

implementation
--------------

new files:

- ``include/phosphor/prompt.h`` -- interactive prompt API
- ``src/io/prompt.c`` -- stdin prompt implementation
- ``src/commands/init_cmd.c`` -- init command handler

modified files:

- ``include/phosphor/commands.h`` -- add ``PHOSPHOR_CMD_INIT``
- ``src/commands/phosphor_commands.c`` -- add ``init_specs[]`` and table entry
- ``src/cli/cli_dispatch.c`` -- add case for ``PHOSPHOR_CMD_INIT``
- ``meson.build`` -- add new source files

prompt module API (draft):

.. code-block:: c

   /* prompt user for a string value, return heap-allocated result */
   char *ph_prompt_string(const char *prompt, const char *default_val);

   /* prompt user for an enum choice */
   char *ph_prompt_enum(const char *prompt, const char *default_val,
                        const char *const *choices, size_t choice_count);

   /* prompt with masked input (for secrets) */
   char *ph_prompt_secret(const char *prompt);

   /* prompt yes/no */
   bool ph_prompt_confirm(const char *prompt, bool default_val);

init pipeline:

1. parse args and validate flags
2. load embedded manifest from C buffer
3. for each ``[[variables]]`` entry:

   a. if value provided via CLI flag → use it
   b. else if ``--yes`` and default exists → use default
   c. else if ``--yes`` and required with no default → error
   d. else → prompt interactively

4. build operation plan from embedded manifest ``[[ops]]``
5. resolve paths relative to ``--output`` / current directory
6. preflight checks (destination collision, permissions)
7. if ``--dry-run`` → print plan and exit
8. execute plan: create dirs, render text files, copy binary files
9. print summary report

error code mapping:

.. list-table::
   :header-rows: 1
   :widths: 40 10 20

   * - scenario
     - exit
     - category
   * - missing ``--name``
     - 2
     - ``PH_ERR_USAGE``
   * - required variable unresolved
     - 2
     - ``PH_ERR_USAGE``
   * - destination exists (no ``--force``)
     - 4
     - ``PH_ERR_FS``
   * - stdin EOF during prompt
     - 8
     - ``PH_ERR_SIGNAL``
   * - write failure
     - 4
     - ``PH_ERR_FS``

references
----------

- masterplan section: top-level CLI contract
- masterplan section: argument parser contract
- docs/source/reference/template-manifest-schema.rst

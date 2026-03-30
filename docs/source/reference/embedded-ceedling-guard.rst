.. meta::
   :title: MUST READ -- embedded template Ceedling guard
   :tags: #neonsignal, #phosphor, #ceedling, #embedded, #must-read
   :status: active
   :updated: 2026-03-21

.. index::
   single: embedded; Ceedling guard
   single: MUST READ

MUST READ -- embedded template Ceedling guard
=============================================

.. warning::

   **MUST READ before editing glow command, embedded registry, cli_dispatch,
   or phosphor_commands.** Failure to follow these rules will break CI unit
   tests.

problem
-------

The ``phosphor glow`` command depends on embedded template data generated at
build time by a meson ``custom_target``. The Python script
``scripts/embed_template.py`` walks ``templates/cathode-landing/`` and
produces ``embedded_data.c`` -- a C file containing static byte arrays for
every template file. This file only exists after ``meson setup build &&
ninja -C build``.

Ceedling (the unit test framework) does **not** run meson. It discovers
source files via the ``src/**`` glob in ``project.yml`` and compiles them
directly with gcc. Since ``embedded_data.c`` is never generated during a
Ceedling build, any symbol from that file (or from files that depend on it)
causes an ``undefined reference`` linker error.

The affected symbols are:

- ``ph_cmd_glow()`` -- defined in ``src/commands/glow_cmd.c``
- ``ph_embedded_write_to_dir()`` -- defined in ``src/template/embedded_registry.c``
- ``ph_embedded_file_count()`` -- defined in ``src/template/embedded_registry.c``
- ``ph_embedded_files`` / ``ph_embedded_data_*`` -- defined in the generated
  ``embedded_data.c``

solution
--------

All glow/embedded code is guarded behind the ``PHOSPHOR_HAS_EMBEDDED``
preprocessor flag. Meson sets ``-DPHOSPHOR_HAS_EMBEDDED=1`` in the c_args
alongside the embedded codegen custom_target. Ceedling does not define this
flag, so all guarded code is compiled out during unit tests.

guarded locations
^^^^^^^^^^^^^^^^^

1. **include/phosphor/commands.h** -- the ``ph_cmd_glow()`` declaration is
   wrapped in ``#ifdef PHOSPHOR_HAS_EMBEDDED`` / ``#endif``.

2. **src/cli/cli_dispatch.c** -- the ``PHOSPHOR_CMD_GLOW`` case in the
   dispatch switch is wrapped in ``#ifdef PHOSPHOR_HAS_EMBEDDED`` / ``#endif``.

3. **src/commands/phosphor_commands.c** -- both the ``glow_specs[]`` argspec
   array and the ``{"glow", ...}`` entry in the command table are wrapped
   in ``#ifdef PHOSPHOR_HAS_EMBEDDED`` / ``#endif``.

4. **project.yml** -- ``glow_cmd.c`` and ``embedded_registry.c`` are listed
   under ``:source_exclude:`` so Ceedling does not attempt to compile them.

5. **meson.build** -- ``phosphor_c_args += '-DPHOSPHOR_HAS_EMBEDDED=1'`` is
   set immediately before the embedded codegen section.

rules for future edits
----------------------

.. list-table::
   :header-rows: 1
   :widths: 60 40

   * - If you are...
     - Then you must...
   * - Adding a new embedded command (like glow)
     - Guard all references behind ``#ifdef PHOSPHOR_HAS_EMBEDDED``, exclude
       the source file from ``project.yml :source_exclude:``, and ensure the
       meson.build sets the define.
   * - Adding a new function to ``embedded_registry.c``
     - Guard the declaration in the header with ``#ifdef PHOSPHOR_HAS_EMBEDDED``.
   * - Modifying ``cli_dispatch.c`` to add a new command
     - If the command depends on meson codegen, guard the dispatch case.
   * - Modifying ``phosphor_commands.c`` to add argspecs
     - If the argspecs reference a guarded command, guard them too.
   * - Adding a new ``#include "phosphor/embedded.h"``
     - Ensure the including file is either guarded or excluded from Ceedling.
   * - Running ``ceedling test:all`` locally and seeing linker errors
       about ``ph_cmd_glow`` or ``ph_embedded_*``
     - Check that the guards are in place. Do **not** remove the exclusions.

why not just mock it?
---------------------

Ceedling's CMock can generate mocks for headers, but the embedded data is not
a simple function interface -- it includes generated static arrays, file count
constants, and registry lookup tables. Mocking all of this would be fragile
and would not test anything meaningful. The preprocessor guard approach is
simpler, more reliable, and matches how other optional features (libgit2,
libarchive, PCRE2) are handled in the codebase.

related files
-------------

- ``scripts/embed_template.py`` -- generates ``embedded_data.c``
- ``src/commands/glow_cmd.c`` -- glow command implementation
- ``src/template/embedded_registry.c`` -- embedded file lookup
- ``include/phosphor/embedded.h`` -- embedded API header
- ``meson.build`` -- codegen custom_target and ``PHOSPHOR_HAS_EMBEDDED`` define
- ``project.yml`` -- Ceedling source exclusions

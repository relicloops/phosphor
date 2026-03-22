.. meta::
   :title: abstract args-parser into generic cli library
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-15

.. index::
   single: args-parser
   single: abstraction
   single: modules; parser
   single: refactoring

abstract args-parser into a generic, data-driven CLI library
=============================================================

context
-------

the args-parser (lexer, parser, spec, validator) is currently hardcoded to
phosphor's 6 commands and 32 flag specs. the core parsing machinery
(tokenization, flag conflict detection, type system, spec structs) is already
~85% generic. the coupling points are: a hardcoded ``ph_command_t`` enum,
``match_command()`` strcmp chain, ``ph_command_name()`` switch, a
``PH_CMD_HELP`` special case, ``"phosphor"`` in error strings, and static spec
tables baked into ``spec.c``.

**goal:** make the args-parser fully data-driven so any CLI tool can reuse it
by providing a command table and spec arrays at init time. phosphor-specific
definitions move to application code.

**other modules:** core primitives (alloc, bytes, str, vec, arena, error, log)
and platform layer (fs, proc, clock, signal) are already 95-100% generic. no
changes needed.


objective
---------

decouple the args-parser from phosphor-specific command definitions. introduce
two new configuration types (``ph_cmd_def_t``, ``ph_cli_config_t``) that let
any application provide its own command table and flag spec arrays. the parser,
spec registry, and validator become fully data-driven.


files to modify
----------------

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - File
     - Action
   * - ``include/phosphor/args.h``
     - remove ``ph_command_t`` enum, add ``ph_cmd_def_t`` + ``ph_cli_config_t``,
       update API signatures
   * - ``src/args-parser/parser.c``
     - config-driven command matching, tool name in errors, generic positional
       handling
   * - ``src/args-parser/spec.c``
     - remove phosphor spec tables, replace with config-driven lookup functions
   * - ``include/phosphor/commands.h``
     - **new** -- phosphor command IDs + extern config declaration
   * - ``src/commands/phosphor_commands.c``
     - **new** -- phosphor command table, flag spec arrays, exported config
   * - ``meson.build``
     - add ``src/commands/phosphor_commands.c`` to source list
   * - ``src/args-parser/lexer.c``
     - no changes (already 100% generic)
   * - ``src/args-parser/validate.c``
     - no changes (still a stub)


new types (added to args.h)
----------------------------

ph_cmd_def_t -- static command definition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const char          *name;
       int                  id;
       const ph_argspec_t  *specs;
       size_t               spec_count;
       bool                 accepts_positional;
   } ph_cmd_def_t;

ph_cli_config_t -- parser configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef struct {
       const char          *tool_name;
       const ph_cmd_def_t  *commands;
       size_t               command_count;
   } ph_cli_config_t;

all pointers reference static const data -- zero heap allocation for the
registry.


changes to existing types
--------------------------

ph_parsed_args_t
~~~~~~~~~~~~~~~~

::

   - ph_command_t command     -->  int command_id
   - char *help_target        -->  char *positional   (generic name)

removed from args.h
~~~~~~~~~~~~~~~~~~~

- ``ph_command_t`` enum (moves to ``commands.h`` as phosphor-specific constants)
- ``ph_command_name()`` declaration
- ``ph_argspec_for_command()`` declaration
- ``ph_argspec_lookup()`` declaration

new declarations in args.h
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   ph_result_t ph_parser_parse(const ph_cli_config_t *config,
                                const ph_token_stream_t *tokens,
                                ph_parsed_args_t *out, ph_error_t **err);

   const char *ph_cmd_def_name(const ph_cli_config_t *config, int command_id);

   const ph_argspec_t *ph_cmd_def_specs(const ph_cli_config_t *config,
                                         int command_id, size_t *count);

   const ph_argspec_t *ph_cmd_def_spec_lookup(const ph_cli_config_t *config,
                                               int command_id,
                                               const char *flag_name);


implementation steps
---------------------

1. **update args.h** -- add ``ph_cmd_def_t`` and ``ph_cli_config_t`` structs.
   remove ``ph_command_t`` enum and old function declarations. change
   ``ph_parsed_args_t``: ``command`` to ``command_id`` (int), ``help_target`` to
   ``positional``. add new function declarations.

2. **create include/phosphor/commands.h** -- define phosphor command ID
   constants: ``PHOSPHOR_CMD_CREATE=1``, ``BUILD=2``, ``CLEAN=3``,
   ``DOCTOR=4``, ``VERSION=5``, ``HELP=6``. declare
   ``extern const ph_cli_config_t phosphor_cli_config``.

3. **create src/commands/phosphor_commands.c** -- move from ``spec.c``:
   ``eol_choices[]``, ``create_specs[]``, ``build_specs[]``,
   ``clean_specs[]``, ``doctor_specs[]``. define ``phosphor_commands[]`` command
   table (6 entries, help has ``accepts_positional=true``). define and export
   ``phosphor_cli_config``.

4. **refactor parser.c** -- ``ph_parser_parse()`` gains
   ``const ph_cli_config_t *config`` as first parameter.
   ``match_command()`` becomes linear scan over ``config->commands``. replace
   ``ph_command_name()`` with ``ph_cmd_def_name()``. replace ``PH_CMD_HELP``
   check with ``def->accepts_positional``. replace ``"phosphor"`` in error
   strings with ``config->tool_name``. rename ``help_target`` to ``positional``.

5. **refactor spec.c** -- remove all phosphor-specific arrays and
   ``eol_choices[]``. remove ``ph_argspec_for_command()`` and
   ``ph_argspec_lookup()``. add ``ph_cmd_def_specs()`` and
   ``ph_cmd_def_spec_lookup()``. keep ``ph_arg_type_name()``.

6. **update meson.build** -- add ``src/commands/phosphor_commands.c`` to
   ``phosphor_sources``.

7. **update main.c** -- include ``phosphor/commands.h``. pass
   ``&phosphor_cli_config`` when calling parser.


impact on pending tasks
------------------------

- **task 6 (KVP parser):** no impact -- operates on string values, no command
  knowledge
- **task 7 (validator):** ``ph_validate()`` will take
  ``const ph_cli_config_t *config`` and use ``ph_cmd_def_spec_lookup()``
  instead of ``ph_argspec_lookup()``. logic identical.
- **task 8 (CLI dispatch):** uses ``command_id`` (int) with
  ``PHOSPHOR_CMD_*`` constants. can iterate ``config->commands`` for
  auto-generated help. naturally benefits from abstraction.
- **task 9 (unit tests):** tests pass a test-specific ``ph_cli_config_t``
  with mock commands


deliverables
------------

1. ``include/phosphor/args.h`` -- generic parser API with ``ph_cmd_def_t`` and
   ``ph_cli_config_t``
2. ``include/phosphor/commands.h`` -- phosphor-specific command ID constants
3. ``src/commands/phosphor_commands.c`` -- phosphor command table and flag specs
4. ``src/args-parser/parser.c`` -- config-driven parser (no phosphor references)
5. ``src/args-parser/spec.c`` -- config-driven spec lookups (no phosphor
   references)


acceptance criteria
--------------------

- [ ] build with ``meson setup build && ninja -C build`` -- zero warnings
- [ ] args-parser source files (``lexer.c``, ``parser.c``, ``spec.c``) contain
  zero references to "create", "build", "clean", "doctor", "version", "help",
  or "phosphor"
- [ ] all phosphor-specific command/flag data lives only in
  ``src/commands/phosphor_commands.c`` and ``include/phosphor/commands.h``
- [ ] the binary still links and runs (``./build/phosphor`` exits 0)

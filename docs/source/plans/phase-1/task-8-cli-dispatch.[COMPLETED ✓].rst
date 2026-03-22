.. meta::
   :title: cli dispatch skeleton
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; cli dispatch
   single: modules; cli_dispatch
   pair: command; help
   pair: command; version

task 8 -- cli dispatch skeleton
=================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement cli_dispatch.c (command routing from parsed args to command
handlers), cli_help.c (usage output), cli_version.c (version string output).
this is the top-level entry point that wires parser output to command
execution.

depends on
----------

- task-7-args-validator

deliverables
------------

1. cli_dispatch.c -- route parsed command to appropriate handler (create_cmd, build_cmd, etc.)
2. cli_help.c -- usage summary for all commands; per-command help with ``phosphor help <command>``
3. cli_version.c -- print ``phosphor <semver> (<build-hash> <build-date>)``, exit 0
4. main.c -- entry point: parse -> validate -> dispatch -> exit code

dispatch contract
~~~~~~~~~~~~~~~~~

- parse args (data only) -> validate (data only) -> dispatch (side effects begin here)
- unknown command -> exit 2 with diagnostic
- ``phosphor help`` -> print all commands, exit 0
- ``phosphor help <command>`` -> print command-specific usage, exit 0
- ``phosphor version`` -> print version string, exit 0

acceptance criteria
-------------------

- [x] all six commands routable: create, build, clean, doctor, version, help
- [x] help and version produce correct output and exit 0
- [x] unknown command exits 2 with clear message
- [x] command handlers are stub functions that return exit code 0

implementation notes
--------------------

- ``src/cli/cli_dispatch.c`` -- switch on ``args->command_id``, stubs log
  "not yet implemented" and return 0 for create/build/clean/doctor
- ``src/cli/cli_version.c`` -- prints ``phosphor 0.1.0``
- ``src/cli/cli_help.c`` -- two modes: no topic (general usage summary listing
  all commands) and with topic (per-command flag table with type, form,
  required, default, choices). unknown topic returns exit 2
- ``src/main.c`` -- full pipeline: ``ph_signal_install()`` ->
  ``ph_lexer_tokenize()`` -> ``ph_parser_parse()`` -> ``ph_validate()`` ->
  signal check -> ``ph_cli_dispatch()`` -> cleanup
- ``include/phosphor/cli.h`` -- added ``ph_cli_dispatch``, ``ph_cli_help``,
  ``ph_cli_version`` declarations

references
----------

- masterplan lines 114-121 (core commands)
- masterplan lines 1247-1252 (appendix e: version/help)

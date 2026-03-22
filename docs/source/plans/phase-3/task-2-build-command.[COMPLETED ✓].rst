.. meta::
   :title: build command (compatibility mode)
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 3; task; build command
   single: modules; build_cmd
   pair: command; build
   single: compatibility mode

task 2 -- build command (compatibility mode)
=============================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement build_cmd.c -- stage a (compatibility mode). validates environment
and project layout, then executes existing build scripts deterministically
through the proc layer. maps script exit codes to stable phosphor exit codes.
this bridges current shell workflows into phosphor.

depends on
----------

- phase-3/task-1-proc-layer
- phase-2 complete

deliverables
------------

1. build_cmd.c -- validate project layout, invoke scripts via proc layer
2. project layout validation (check expected directories/files exist)
3. script invocation: scripts/_default/build.sh through proc layer
4. exit code mapping from script exits to phosphor stable codes
5. metadata cleanup after build (enforce deny list)

flags supported (appendix b)
-----------------------------

--project, --deploy-at, --clean-first, --tld, --strict, --toml, --verbose,
--normalize-eol

acceptance criteria
-------------------

- [x] ``phosphor build`` invokes existing scripts through proc layer
- [x] script exit codes mapped to phosphor exit codes via ph_proc_map_exit
- [x] --clean-first triggers clean before build (passes --clean to all.sh)
- [x] --deploy-at controls output directory (passes --public to all.sh)
- [x] --project allows specifying project path (resolved to absolute)
- [x] metadata artifacts cleaned from output (recursive cleanup_metadata)
- [x] --verbose sets log level to DEBUG for script output passthrough

implementation notes
--------------------

files modified:

- ``include/phosphor/commands.h`` -- added ph_cmd_build declaration
- ``src/cli/cli_dispatch.c`` -- wired PHOSPHOR_CMD_BUILD to ph_cmd_build
- ``src/commands/build_cmd.c`` -- full pipeline (~310 lines)
- ``src/args-parser/validate.c`` -- fixed PH_TYPE_PATH to reject absolute paths
- ``src/commands/phosphor_commands.c`` -- build specs: --project and --deploy-at
  changed from PH_TYPE_PATH to PH_TYPE_STRING (accept absolute paths)
- ``tests/unit/test_cli.c`` -- added build_cmd.c + proc layer deps, updated
  dispatch test for real build validation behavior

pipeline: flag extraction -> path resolution -> layout validation ->
argv build -> env sanitization -> signal check -> proc_exec -> signal check ->
metadata cleanup -> summary logging -> cascading cleanup.

reserved flags (--strict, --toml, --normalize-eol) log warnings when used.

bug fix: PH_TYPE_PATH validator was missing absolute-path rejection
(test_validate_path_absolute_rejected was failing). added the check and
adjusted build specs to use PH_TYPE_STRING where absolute paths are valid.

references
----------

- masterplan lines 797-826 (build workflow)
- masterplan lines 1217-1228 (appendix b flags)

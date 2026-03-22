.. meta::
   :title: verbose flag implementation across all commands
   :tags: #neonsignal, #phosphor
   :status: draft
   :updated: 2026-03-19

.. index::
   single: verbose
   single: logging
   single: debug output
   pair: flag; verbose

verbose flag implementation across all commands
================================================

problem
-------

every phosphor command accepts ``--verbose`` but the flag is a no-op or only
partially wired in most commands. the log system already supports
``PH_LOG_DEBUG`` and ``ph_log_set_level()`` -- the infrastructure exists but
commands do not consistently use it.

current state (audited 2026-03-19):

.. list-table::
   :header-rows: 1
   :widths: 12 12 12 64

   * - command
     - level set
     - debug calls
     - notes
   * - ``create``
     - YES
     - 7
     - [DONE] full pipeline: flags, template resolve, manifest, var merge
       (per-var), plan build, staging execute, staging commit
   * - ``glow``
     - YES
     - 7
     - [DONE] full pipeline: flags, embedded write, manifest, var merge
       (per-var), plan build, staging execute, staging commit
   * - ``build``
     - YES
     - 6
     - partial. has: manifest load, static asset copy (2), deploy path,
       metadata cleanup (2). missing: flag values, project resolve, esbuild
       argv, npm install, env setup, define injection, build report
   * - ``clean``
     - YES
     - 1
     - minimal. has: "skipping (not found)" for missing dirs. missing:
       project resolve, stale scan details, per-dir removal, report totals
   * - ``rm``
     - YES
     - 1
     - minimal. has: "removing" at final removal. missing: project resolve,
       manifest guard result, path validation, existence check
   * - ``certs``
     - YES
     - 0
     - empty. level set but zero debug messages emitted. missing: everything
       -- local CA steps, LE account, order, challenge, finalize, cert paths
   * - ``doctor``
     - YES
     - 0
     - empty. level set but zero debug messages emitted. missing: everything
       -- manifest check, tool checks, node check, build check, stale scan,
       cert expiry check

goal
----

when ``--verbose`` is passed, every command should:

1. call ``ph_log_set_level(PH_LOG_DEBUG)`` at the start
2. emit ``ph_log_debug()`` messages at key pipeline stages
3. produce output that helps a user diagnose what phosphor is doing

the output should be useful, not noisy. debug messages should answer "what is
phosphor doing right now?" and "what values did it resolve?"

deliverables
------------

1. ``--verbose`` wired in all commands via ``ph_log_set_level(PH_LOG_DEBUG)``
2. ``ph_log_debug()`` calls at key stages in each command pipeline
3. remove ``(reserved)`` from verbose flag descriptions in argspec tables
4. shared verbose-init pattern: consistent early setup across all commands

tasks
-----

task 1 -- wire verbose in create and glow [DONE]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

both commands share the same pipeline structure. added debug logging at:

- flag extraction: log resolved flag values
- template source resolution: log which source type (local/git/archive/embedded)
- manifest loading: log manifest path and parsed variable count
- variable merge: log each resolved variable name + value
- plan build: log operation count
- staging execute: log staging path
- staging commit: log staging path and final destination

files modified:

- ``src/commands/create_cmd.c`` -- 7 ``ph_log_debug()`` calls added
- ``src/commands/glow_cmd.c`` -- 7 ``ph_log_debug()`` calls added

acceptance criteria:

- [x] ``phosphor create --verbose --name=x --template=y`` shows debug output
      for every pipeline stage
- [x] ``phosphor glow --verbose --name=x`` shows debug output for embedded
      template extraction, manifest load, variable merge, and file operations
- [x] no debug output visible without ``--verbose``

task 2 -- improve verbose in build
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

build has 6 debug calls but misses the most diagnostic-useful stages.

existing coverage (keep):

- line 395: manifest loaded
- lines 769, 787: static asset copy / skip
- line 804: deploy destination
- lines 77, 103: metadata cleanup (directory open error, file removed)

missing coverage (add):

- flag extraction: log resolved ``--project``, ``--deploy-at``, ``--tld``,
  ``--clean-first``, ``--strict``, ``--toml`` values
- project root resolution: log resolved absolute path
- project layout validation: log ``src/`` and ``scripts/`` check results
- build/deploy path computation: log computed build dir, deploy dir, entry
  point, output path
- npm install: log whether esbuild found, npm install triggered
- esbuild invocation: log exact argv array and key environment variables
- define injection: log each ``--define:`` key=value pair
- build report: log summary stats before output

files to modify:

- ``src/commands/build_cmd.c``

acceptance criteria:

- [ ] ``phosphor build --verbose`` shows esbuild command line, defines, and
      per-file copy operations
- [ ] existing debug output preserved and augmented

task 3 -- improve verbose in clean, rm, certs, doctor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**clean** (1 existing debug call):

add:

- project root resolution: log resolved path
- stale scan: log each ``.phosphor-staging-*`` dir found
- per-dir removal: log each directory being removed with path
- report: log totals (dirs removed, bytes freed)

**rm** (1 existing debug call):

add:

- project root resolution: log resolved path
- manifest guard: log whether manifest found, ``--force`` override
- path validation: log rejection reason (absolute path, traversal)
- target resolution: log resolved absolute target path
- existence check: log stat result (exists, is_dir, is_file)

**certs** (0 debug calls -- needs full instrumentation):

add:

- flag extraction: log mode (local/letsencrypt), ``--domain``, ``--staging``
- local CA mode: log key generation, CSR, cert signing steps with paths
- LE account: log account key path, registration URL
- LE order: log domain, order URL, authorization URLs
- LE challenge: log challenge type, token, response URL
- LE finalize: log CSR submission, cert download path
- cert output: log final cert/key file paths

**doctor** (0 debug calls -- needs full instrumentation):

add:

- manifest check: log path checked, result
- tool checks: log each tool name before check, found/not-found + path
- node check: log package.json found, node_modules status
- build check: log build/ directory status
- stale scan: log each stale dir found
- cert check: log each cert file checked, expiry date

files to modify:

- ``src/commands/clean_cmd.c``
- ``src/commands/rm_cmd.c``
- ``src/commands/certs_cmd.c``
- ``src/commands/doctor_cmd.c``

acceptance criteria:

- [ ] ``phosphor clean --verbose`` shows each scanned/removed path
- [ ] ``phosphor rm --verbose --specific=x`` shows path resolution and stat
- [ ] ``phosphor certs --verbose --generate`` shows cert generation steps
- [ ] ``phosphor doctor --verbose`` shows each diagnostic check in progress

task 4 -- remove "(reserved)" from verbose descriptions [DONE]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

updated ``phosphor_commands.c`` argspec: changed
``"enable verbose output (reserved)"`` to ``"enable verbose output"`` for
create. other commands already had the correct description.

files modified:

- ``src/commands/phosphor_commands.c``

acceptance criteria:

- [x] ``phosphor help create`` shows ``enable verbose output`` without
      ``(reserved)``
- [x] all commands show consistent verbose flag description

implementation pattern
----------------------

every command handler should follow this pattern at the top:

.. code-block:: c

   bool verbose = ph_args_has_flag(args, "verbose");
   if (verbose)
     ph_log_set_level(PH_LOG_DEBUG);

then use ``ph_log_debug()`` for stage-level messages:

.. code-block:: c

   ph_log_debug("template source: %s (type: %s)", tmpl_val, source_type);
   ph_log_debug("manifest loaded: %zu variables, %zu ops",
                manifest.variable_count, manifest.op_count);
   ph_log_debug("rendering: %s -> %s", from_path, to_path);

guidelines:

- one debug line per major pipeline stage (not per loop iteration unless I/O)
- include resolved values (paths, counts, names) -- the point is diagnosis
- no debug output in the default (non-verbose) mode
- debug messages go to stderr (already handled by the log system)

references
----------

- ``include/phosphor/log.h`` -- log level enum and macros
- ``src/core/log.c`` -- log implementation, default level is ``PH_LOG_INFO``
- ``src/commands/build_cmd.c`` -- reference for partial verbose pattern

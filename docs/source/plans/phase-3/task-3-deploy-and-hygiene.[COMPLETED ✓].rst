.. meta::
   :title: deploy guardrails and metadata hygiene
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 3; task; deploy guardrails
   single: metadata hygiene
   single: output standardization

task 3 -- deploy guardrails and metadata hygiene
==================================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement deploy-at path validation, metadata artifact cleanup after build,
and output standardization. ensures build output is clean, deterministic, and
free of platform-specific artifacts.

depends on
----------

- phase-3/task-2-build-command

deliverables
------------

1. deploy-at path validation (within project root, no escape)
2. post-build metadata cleanup scan (apply deny list to output directory)
3. build summary report (files processed, artifacts cleaned, warnings)
4. --strict mode: warnings become errors
5. --toml output mode for ci automation

acceptance criteria
-------------------

- [x] deploy-at path validated (no root escape) via deploy_at_escapes_root()
- [x] .DS_Store, ._*, Thumbs.db cleaned from build output via cleanup_metadata()
- [x] --strict makes warnings into exit 6 (PH_ERR_VALIDATE) failures
- [x] --toml produces machine-readable TOML report via report_toml()
- [x] build output report consistent with create report format (report_plain)

implementation notes
--------------------

all changes in ``src/commands/build_cmd.c`` (~395 lines total):

- ``deploy_at_escapes_root()`` -- validates resolved --deploy-at path starts
  with project_root_abs followed by '/' or end-of-string. returns
  PH_ERR_VALIDATE (exit 6) on escape
- ``cleanup_result_t`` -- replaces bare size_t return from cleanup_metadata,
  now tracks both removed count and warning count (failed unlinks)
- ``build_report_t`` + ``report_plain()`` / ``report_toml()`` -- structured
  build summary output. plain mode uses ph_log_info/warn, TOML mode writes
  ``[build]`` table to stdout with status/exit_code/project/deploy_at/
  metadata_removed/warnings fields
- ``--strict``: if child succeeded (exit 0) but warnings > 0, overrides
  exit code to PH_ERR_VALIDATE (6)
- ``--toml``: suppresses pre-exec log line, emits TOML report instead of
  plain text summary
- ``--normalize-eol`` remains reserved (warns if set)

references
----------

- masterplan lines 800-805 (stage a)
- masterplan lines 847-851 (output modes)
- masterplan lines 445-451 (metadata deny)

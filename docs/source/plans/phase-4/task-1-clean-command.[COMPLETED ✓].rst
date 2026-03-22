.. meta::
   :title: clean command (native c)
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-15

.. index::
   triple: phase 4; task; clean command
   single: modules; clean_cmd
   pair: command; clean
   single: stale detection

task 1 -- clean command (native c)
====================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement clean_cmd.c -- replace scripts/_default/clean.sh with native c
implementation. supports --stale (remove leftover staging directories),
--project, --dry-run, --verbose.

depends on
----------

- phase-3 complete

deliverables
------------

1. clean_cmd.c -- native clean implementation
2. --stale: detect and remove .phosphor-staging-* directories
3. --dry-run: show what would be removed without removing
4. standard clean: remove build output directories
5. project discovery: --project flag or current directory

flags supported (appendix c)
-----------------------------

--stale, --project, --dry-run, --verbose

acceptance criteria
-------------------

- [x] ``phosphor clean`` removes build artifacts matching current clean.sh behavior
- [x] --stale finds and removes orphaned staging directories
- [x] --dry-run lists files without removing
- [x] parity with clean.sh output (from phase 0 compatibility matrix)

references
----------

- masterplan lines 1230-1236 (appendix c flags)
- masterplan lines 775-776 (stale staging detection)

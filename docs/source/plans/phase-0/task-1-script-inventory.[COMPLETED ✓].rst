.. meta::
   :title: script inventory and behavior matrix
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; task; script inventory
   single: shell scripts
   single: behavior matrix

script inventory and behavior matrix
=====================================

objective
---------

inventory all existing shell scripts and document their expected inputs, outputs,
side-effects, and exit behavior. this is the foundation for the compatibility matrix
and ensures phosphor faithfully replaces current behavior.

scope
-----

scripts to inventory:

1. ``scripts/_default/build.sh`` → document env vars, args, filesystem mutations, exit codes.
2. ``scripts/_default/clean.sh`` → document what gets removed, safety checks.
3. ``scripts/_default/all.sh`` → document orchestration order, error propagation.
4. ``scripts/global_variables.sh`` → document every exported variable, its source, and consumers.

deliverables
------------

1. behavior matrix table (input → output → side-effect → exit code) for each script.
2. dependency graph: which scripts call which, and in what order.
3. environment variable catalog: every var read, written, or exported.

acceptance criteria
-------------------

- □ all four scripts fully documented with input/output/side-effect tables.
- □ no undocumented variable or filesystem mutation remains.
- □ document reviewed against actual script source (not assumptions).

references
----------

- masterplan: migration plan, phase 0, item 1 and 6
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 1020, 1025-1029)

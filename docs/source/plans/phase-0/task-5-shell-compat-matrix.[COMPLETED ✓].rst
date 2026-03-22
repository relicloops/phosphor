.. meta::
   :title: shell compatibility matrix
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; task; compatibility matrix
   single: shell scripts
   single: behavioral parity

shell compatibility matrix
===========================

objective
---------

produce a side-by-side compatibility matrix showing current shell script behavior
versus intended phosphor command behavior. this ensures phase 3 (build compatibility
mode) faithfully reproduces existing behavior before internalization.

scope
-----

matrix columns:

1. operation (e.g., "clean build artifacts", "copy static assets", "inject tld").
2. current implementation (which script, which function/line).
3. phosphor equivalent (which command + flags).
4. behavioral differences (if any, and whether intentional).
5. verification method (how to confirm parity).

scripts covered:

- ``scripts/_default/build.sh`` vs ``phosphor build``
- ``scripts/_default/clean.sh`` vs ``phosphor clean``
- ``scripts/_default/all.sh`` vs ``phosphor build --clean-first`` (or equivalent)

deliverables
------------

1. compatibility matrix document.
2. list of intentional behavioral changes with rationale.
3. list of test scenarios for parity verification.

acceptance criteria
-------------------

- □ every observable behavior of each script has a corresponding phosphor entry.
- □ intentional differences are explicitly marked and justified.
- □ at least one verification method per operation.

references
----------

- masterplan: migration plan, phase 0, item 6
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 1025-1029)
- masterplan: detailed build workflow
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 797-826)

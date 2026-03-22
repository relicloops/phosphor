.. meta::
   :title: exit codes and logging conventions
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; task; exit codes
   single: logging
   single: diagnostics
   pair: error handling; taxonomy

define exit codes and logging conventions
==========================================

objective
---------

formalize the exit code taxonomy (0-8) and logging level conventions. produce a
reference document that phase 1 implementation will code against.

scope
-----

exit codes to formalize:

1. ``0`` → success
2. ``1`` → general/unmapped error
3. ``2`` → invalid args/usage
4. ``3`` → config/template parse error
5. ``4`` → filesystem error
6. ``5`` → process execution failure
7. ``6`` → validation/guardrail failure
8. ``7`` → internal invariant violation
9. ``8`` → interrupted by signal (sigint/sigterm)

logging levels: error, warn, info, debug, trace.

parser diagnostic subcodes: ``ux001`` through ``ux007``.

deliverables
------------

1. exit code reference with examples of what triggers each code.
2. logging level guidelines (what goes at each level).
3. diagnostic subcode reference (ux001-ux007) with example messages.

acceptance criteria
-------------------

- □ every exit code (0-8) has at least one concrete triggering scenario documented.
- □ child process exit code mapping rule documented.
- □ all seven ux diagnostic subcodes have example flag/value/message triples.
- □ logging levels have clear boundary rules (when to use warn vs error, etc.).

references
----------

- masterplan: exit code policy
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 136-149)
- masterplan: parser diagnostics
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 250-258)
- masterplan: error handling and observability
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 828-856)

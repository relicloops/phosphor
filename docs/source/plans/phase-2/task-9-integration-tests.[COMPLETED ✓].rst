.. meta::
   :title: integration tests and sanitizer validation
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; integration tests
   single: testing
   single: golden output
   single: sanitizers

task 9 -- integration tests and sanitizer validation
======================================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

write integration tests for the complete create command pipeline. verify golden
output, deterministic behavior, ASan/Valgrind clean runs. this is the quality
gate for M1 (create MVP milestone).

.. note::

   integration tests use **Ceedling** for C-level pipeline tests and
   shell-based golden tests for end-to-end output comparison.
   see `../testing-infrastructure-ceedling.rst` for framework setup.

depends on
----------

- task-7-create-command (all phase 2 code complete)

deliverables
------------

1. golden output tests: create from fixture template, compare output tree against expected
2. deterministic output hashing (stable timestamps excluded)
3. ASan clean run on full create pipeline
4. Valgrind clean run (linux) / leaks tool (macOS)
5. metadata deny list enforcement tests
6. failure injection: permission denied, missing template, invalid manifest

milestone gate (M1 definition of done)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``phosphor create --name=sample`` produces valid project from template
- no leaks under ASan/Valgrind in normal create flow
- generated tree is deterministic and free from metadata artifacts
- command returns clear errors and stable exit codes

acceptance criteria
-------------------

- [ ] golden output test passes (byte-exact tree comparison)
- [ ] ASan reports zero leaks/errors
- [ ] Valgrind (linux) or leaks (macOS) clean
- [ ] metadata artifacts (.DS_Store, ._*) never appear in output
- [ ] all error paths produce correct exit codes
- [ ] deterministic: same input -> same output tree

references
----------

- masterplan lines 994-1012 (integration/regression tests)
- masterplan lines 1176-1183 (definition of done)

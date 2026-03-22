.. meta::
   :title: full ci matrix and sanitizer jobs
   :tags: #neonsignal, #phosphor
   :status: deferred
   :updated: 2026-02-14

.. index::
   triple: phase 6; task; ci matrix
   single: continuous integration
   single: build matrix
   single: sanitizers

task 1 -- full ci matrix and sanitizer jobs
=============================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

set up comprehensive ci pipeline with matrix builds across macos + linux,
multiple compiler versions, all sanitizer configurations, and automated test
execution.

depends on
----------

- all prior phases

deliverables
------------

1. ci configuration (github actions or similar)
2. build matrix: macos (clang) + linux (gcc, clang)
3. sanitizer jobs: ASan, UBSan, valgrind (linux), leaks (macos)
4. automated test execution: unit, integration, golden output
5. warning-as-error enforcement in ci
6. artifact collection: binaries, test reports

acceptance criteria
-------------------

- [ ] ci runs on every push/pr
- [ ] macos and linux builds pass
- [ ] ASan and UBSan clean on both platforms
- [ ] valgrind clean on linux
- [ ] leaks clean on macos
- [ ] all tests pass across matrix

references
----------

- masterplan lines 420-424 (memory correctness)
- masterplan lines 961-965 (compiler baseline)

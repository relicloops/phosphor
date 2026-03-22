.. meta::
   :title: code coverage infrastructure
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-19

.. index::
   single: coverage
   single: gcov
   single: gcovr
   pair: testing; coverage

code coverage infrastructure
=============================

context
-------

phosphor uses Ceedling 1.0.1 (Unity + CMock) for unit testing. with 24+ test
files covering 40+ source files, the project needs visibility into which code
paths are exercised by the test suite.

this plan adds gcov-based coverage reporting via Ceedling's ``gcov`` plugin
and integrates coverage jobs into both CI pipelines (GitLab and GitHub Actions).

setup
-----

Ceedling ``project.yml`` configuration:

.. code-block:: yaml

   :plugins:
     :enabled:
       - gcov

   :gcov:
     :utilities:
       - gcovr
     :reports:
       - HtmlDetailed
     :gcovr:
       :report_root: "../../"
       :html_medium_threshold: 75
       :html_high_threshold: 90
       :exclude:
         - subprojects
         - tests

running coverage
----------------

.. code-block:: bash

   ceedling gcov:all                    # run all tests with coverage
   ceedling gcov:test_str               # run single test with coverage

reports are generated under ``build/ceedling/artifacts/gcov/``.

viewing reports
---------------

after running ``ceedling gcov:all``, open the HTML report:

.. code-block:: bash

   open build/ceedling/artifacts/gcov/GcovCoverageResults.html   # macOS
   xdg-open build/ceedling/artifacts/gcov/GcovCoverageResults.html  # Linux

the report shows per-file line and branch coverage with color-coded thresholds:

- green: >= 90% (high)
- yellow: >= 75% (medium)
- red: < 75% (low)

CI integration
--------------

both CI pipelines include an informative (non-blocking) coverage job:

- **GitLab CI**: ``coverage`` job in test stage, ``allow_failure: true``,
  uploads HTML report as artifact, parses percentage for badge
- **GitHub Actions**: ``coverage`` job, ``continue-on-error: true``,
  uploads HTML report as artifact

coverage is informative only -- it does not gate releases. the goal is
visibility, not enforcement.

current coverage map
--------------------

**fully covered (dedicated tests)**:

- core: alloc, str, vec, bytes, arena, error
- args-parser: lexer, parser, kvp, validate (spec covered via parser/validate)
- template: transform, config, manifest_load, var_merge, renderer, planner, staging
- io: path_norm, metadata_filter
- proc: spawn, wait, env
- cli: cli_dispatch, cli_help, cli_version

**indirectly covered only**:

- io: fs_readwrite, fs_copytree, fs_atomic (via test_staging.c)
- template: writer (via test_staging.c)

**acceptable gaps** (no dedicated tests):

- ``main.c`` -- entry point, not unit-testable
- ``clean_cmd.c`` -- implemented in Phase 4 (7 unit tests)
- ``doctor_cmd.c`` -- stub (Phase 4+ scope)
- ``platform_common.c`` -- platform dispatch layer
- ``clock_posix.c`` -- thin POSIX wrapper

dependencies
------------

- **gcovr**: Python package (``pip install gcovr``)
- **gcov**: provided by GCC toolchain
- Ceedling 1.0.1 with gcov plugin (bundled)

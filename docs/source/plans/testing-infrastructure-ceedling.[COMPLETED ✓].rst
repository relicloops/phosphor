.. meta::
   :title: testing infrastructure -- ceedling
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   single: testing
   single: ceedling
   single: unity
   single: cmock

testing infrastructure -- ceedling
===================================

.. note::

   parent plan: `phosphor-pure-c-cli-masterplan.rst`

   related tasks:

   - `phase-1/task-9-unit-tests-core-parser.rst` (unit tests)
   - `phase-2/task-9-integration-tests.rst` (integration tests)

overview
--------

this document specifies the test framework infrastructure for phosphor. it
replaces the original masterplan tier 3 candidates (cmocka and criterion) with a
single unified choice: **Ceedling 1.0.1** (Unity 2.6.1 + CMock 2.6.0 +
CException).

rationale for Ceedling:

- auto-discovers test files and generates test runners -- no manual runner
  boilerplate
- CMock generates mock implementations from headers -- dependency injection
  without hand-written fakes
- CException provides lightweight C exception handling for failure-path tests
- Ruby-based build system runs independently of Meson -- zero conflicts with the
  production build
- single ``ceedling test:all`` command replaces multiple manual steps


requirements
------------

- **Ruby 3+** (4.0.1 installed)
- **Ceedling 1.0.1**: ``gem install ceedling``
- no additional system dependencies beyond the C compiler already required by
  Meson


directory layout
----------------

Ceedling operates alongside the existing Meson build with no overlap:

.. code-block:: text

   tools/phosphor/
   ├── project.yml              # Ceedling configuration
   ├── build/
   │   ├── meson/               # Meson build output (existing)
   │   └── ceedling/            # Ceedling build output (new, gitignored)
   ├── src/                     # production source (shared with Meson)
   ├── include/                 # production headers (shared with Meson)
   └── tests/
       ├── unit/                # test files: test_<module>.c
       └── support/             # shared test helpers and fixtures

- ``tests/unit/`` -- test source files discovered by Ceedling
- ``tests/support/`` -- helper utilities, custom assertions, shared fixtures
- ``build/ceedling/`` -- all Ceedling build artifacts (gitignored)


project.yml configuration
--------------------------

key settings that align Ceedling with the existing Meson build:

.. code-block:: yaml

   :project:
     :build_root: build/ceedling
     :use_exceptions: TRUE
     :use_mocks: TRUE

   :paths:
     :source:
       - src/**
     :include:
       - include
     :test:
       - tests/unit/**
     :support:
       - tests/support

   :flags:
     :test:
       :compile:
         :*:
           - -std=c17
           - -pedantic
           - -Wall
           - -Wextra
           - -D_POSIX_C_SOURCE=200809L

compiler flags mirror the Meson configuration to ensure identical compilation
behavior between production and test builds.


test file naming
----------------

all test files follow the Ceedling convention:

- filename: ``test_<module>.c``
- test functions: ``void test_<description>(void)``

examples:

- ``test_alloc.c`` -- allocator lifecycle and failure paths
- ``test_str.c`` -- string primitives
- ``test_vec.c`` -- dynamic vector operations
- ``test_bytes.c`` -- byte buffer handling
- ``test_arena.c`` -- arena allocator lifecycle
- ``test_error.c`` -- error cause chains
- ``test_kvp.c`` -- KVP parser (nested objects, error cases)
- ``test_lexer.c`` -- args lexer tokenization
- ``test_parser.c`` -- args parser strict assignment mode
- ``test_validate.c`` -- validator with all 7 UX diagnostic subcodes


CMock strategy
--------------

CMock generates mock implementations from header files automatically. the
strategy for phosphor:

- **real allocator by default**: tests use the production allocator for
  realistic behavior. CMock-generated mocks replace allocator calls only in
  failure-path tests (OOM simulation, leak detection)
- **header-driven mocking**: any ``include/phosphor/*.h`` header can be mocked
  by adding ``#include "mock_<header>.h"`` in a test file
- **dependency injection**: modules under test receive dependencies through
  function parameters or struct fields, making mock substitution straightforward


modules to test (phase 1 scope)
--------------------------------

phase 1 task 9 covers unit tests for:

**core primitives** (``src/core/``):

- ``alloc`` -- allocation, reallocation, free, leak detection
- ``str`` -- creation, concatenation, slicing, comparison
- ``vec`` -- push, pop, grow, shrink, iteration
- ``bytes`` -- buffer operations, binary safety
- ``arena`` -- lifecycle (create, alloc, reset, destroy)
- ``error`` -- cause chain construction and traversal

**args-parser** (``src/args-parser/``):

- ``kvp`` -- nested object parsing, duplicate keys, error cases
- ``lexer`` -- tokenization edge cases
- ``parser`` -- strict assignment parsing (reject ``--flag value``)
- ``validate`` -- all seven UX diagnostic subcodes (ux001..ux007):
  enable/disable conflicts, duplicate scalars, typed mismatches


Meson coexistence
-----------------

Ceedling and Meson operate in complete isolation:

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - Concern
     - Meson
     - Ceedling
   * - Build root
     - ``build/`` (default)
     - ``build/ceedling/``
   * - Config file
     - ``meson.build``
     - ``project.yml``
   * - Source discovery
     - explicit ``sources:`` lists
     - glob ``src/**``
   * - Test discovery
     - manual
     - auto (``tests/unit/**``)
   * - Executor
     - ``ninja``
     - ``ceedling``

both systems read the same ``src/`` and ``include/`` trees. neither modifies
files belonging to the other.


CI integration
--------------

Ceedling test stage runs before the Meson build in CI pipelines:

.. code-block:: yaml

   # example CI stage
   test:
     script:
       - gem install ceedling
       - ceedling test:all

a failing test aborts the pipeline before any production build artifacts are
generated.


.gitignore
----------

the following entry is added to ``.gitignore``:

.. code-block:: text

   # ceedling build output
   build/ceedling/


running tests
-------------

.. code-block:: bash

   # run all tests
   ceedling test:all

   # run a single test file
   ceedling test:test_kvp

   # run tests with verbose output
   ceedling test:all --verbosity=obnoxious

   # clean Ceedling build artifacts
   ceedling clobber


references
----------

- masterplan lines 766-769 (tier 3 dependencies -- superseded by this plan)
- masterplan lines 1248-1281 (testing strategy)
- phase-1/task-9-unit-tests-core-parser.rst
- phase-2/task-9-integration-tests.rst

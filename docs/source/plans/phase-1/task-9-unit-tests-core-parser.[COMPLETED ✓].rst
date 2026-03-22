.. meta::
   :title: unit tests for core and parser
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; unit tests
   single: testing
   single: ceedling
   single: coverage

task 9 -- unit tests for core and parser
==========================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

write comprehensive unit tests covering all core primitives and parser edge
cases. this is the quality gate before phase 2 begins.

.. note::

   test framework: **Ceedling 1.0.1** (Unity + CMock + CException).
   see `../testing-infrastructure-ceedling.rst` for full setup details.

depends on
----------

- task-8-cli-dispatch (all phase 1 code complete)

deliverables
------------

1. unit tests for core: alloc (leak detection), bytes, str, vec, arena (lifecycle), error (cause chains), log
2. unit tests for parser: edge cases from masterplan testing strategy
3. unit tests for kvp: nested objects, error cases
4. unit tests for validator: all seven UX diagnostic subcodes
5. test framework: Ceedling 1.0.1 (Unity + CMock) -- ``project.yml`` configured,
   tests auto-discovered from ``tests/unit/``

test cases (from masterplan testing strategy)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- args parser edge cases
- memory ownership and destructor idempotence
- path sanitizer and traversal rejection
- strict assignment parsing (reject ``--flag value``)
- enable/disable switch conflicts
- duplicate scalar flag rejection
- typed mismatch diagnostics for string/int/bool/enum/path/url
- kvp parser cases including nested objects

acceptance criteria
-------------------

- [x] all core primitives have at least basic lifecycle tests
- [x] all seven UX diagnostic subcodes triggered in tests
- [x] kvp parser tested with valid nesting, invalid nesting, duplicate keys
- [ ] ASan clean on full test suite run (deferred to CI)
- [x] all tests pass on macOS

implementation notes
--------------------

11 test files total (1 existing + 10 new), 117 tests passing:

.. list-table::
   :header-rows: 1
   :widths: 30 10 40

   * - Test file
     - Tests
     - Source deps
   * - ``test_str.c`` (existing)
     - 5
     - str.c, alloc.c
   * - ``test_alloc.c``
     - 9
     - alloc.c
   * - ``test_vec.c``
     - 9
     - vec.c, alloc.c
   * - ``test_bytes.c``
     - 14
     - bytes.c, alloc.c
   * - ``test_arena.c``
     - 7
     - arena.c, alloc.c
   * - ``test_error.c``
     - 8
     - error.c, alloc.c
   * - ``test_lexer.c``
     - 13
     - lexer.c, alloc.c, error.c
   * - ``test_parser.c``
     - 12
     - parser.c, lexer.c, spec.c, phosphor_commands.c, alloc.c, error.c
   * - ``test_kvp.c``
     - 19
     - kvp.c, alloc.c, error.c
   * - ``test_validate.c``
     - 14
     - validate.c, parser.c, spec.c, kvp.c, phosphor_commands.c, alloc.c, error.c
   * - ``test_cli.c``
     - 12
     - cli_dispatch.c, cli_help.c, cli_version.c, phosphor_commands.c, spec.c, alloc.c, error.c, log.c

``test_validate.c`` uses a test-local ``ph_cli_config_t`` with INT and KVP
typed flags (no current phosphor command uses these types) to exercise
UX005/UX006 code paths. ``test_cli.c`` tests return codes only.

references
----------

- masterplan lines 979-1012 (testing strategy)

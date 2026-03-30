Ceedling Test Framework Setup
=============================

.. note::

   **Status: SKIP-STALE** -- Ceedling 1.0.1 was removed from the active build.
   This document preserves the full configuration for future reference when
   setting up a replacement test framework.

   **Backup location:** ``docs/source/reference/tests/`` -- contains the full
   ``tests/`` directory tree including all unit test source files, integration
   scripts, fixtures, and the ``project.yml`` Ceedling config.

Archived File Inventory
-----------------------

::

    tests/
    +-- ceedling-backup/
    |   +-- project.yml
    |   +-- support/.gitkeep
    |   +-- unit/
    |       +-- test_acme_base64url.c
    |       +-- test_acme_json.c
    |       +-- test_alloc.c
    |       +-- test_archive.c
    |       +-- test_arena.c
    |       +-- test_build_cmd.c
    |       +-- test_bytes.c
    |       +-- test_certs_config.c
    |       +-- test_certs_san.c
    |       +-- test_clean_cmd.c
    |       +-- test_cli.c
    |       +-- test_color.c
    |       +-- test_config.c
    |       +-- test_create_cmd.c
    |       +-- test_env.c
    |       +-- test_error.c
    |       +-- test_fs_atomic.c
    |       +-- test_git_fetch.c
    |       +-- test_kvp.c
    |       +-- test_lexer.c
    |       +-- test_manifest.c
    |       +-- test_metadata_filter.c
    |       +-- test_parser.c
    |       +-- test_path_norm.c
    |       +-- test_planner.c
    |       +-- test_regex.c
    |       +-- test_renderer.c
    |       +-- test_sha256.c
    |       +-- test_spawn.c
    |       +-- test_staging.c
    |       +-- test_str.c
    |       +-- test_toml_smoke.c
    |       +-- test_transform.c
    |       +-- test_validate.c
    |       +-- test_var_merge.c
    |       +-- test_vec.c
    |       +-- test_wait.c
    |       +-- test_writer.c
    +-- fixtures/
    |   +-- minimal-template/
    |       +-- template.phosphor.toml
    |       +-- src/main.c
    |       +-- src/config.h
    |       +-- src/README.md
    +-- integration/
    |   +-- test_create_golden.sh
    |   +-- test_glow_golden.sh
    +-- golden/

Overview
--------

Phosphor used Ceedling 1.0.1 (Unity + CMock + CException) for unit testing.
38 test modules covered the args parser, template engine, manifest loader,
process management, crypto, I/O, and all command handlers.

The framework was removed because Ceedling's implicit source discovery and
manual ``TEST_SOURCE_FILE()`` directives became brittle as the codebase grew.
Every new source file required updating multiple test files' link lists, and
the file-based ``source_exclude`` warnings were never resolved upstream.

Configuration: project.yml
--------------------------

::

    ---
    :project:
      :build_root: build/ceedling
      :use_exceptions: FALSE
      :use_mocks: TRUE
      :test_file_prefix: test_

    :paths:
      :source:
        - src/**
        - subprojects/toml-c
      :source_exclude:
        - src/commands/glow_cmd.c
        - src/template/embedded_registry.c
      :include:
        - include
        - subprojects/toml-c
        - build/subprojects/pcre2/__CMake_build
      :test:
        - tests/unit/**
      :support:
        - tests/support

    :flags:
      :test:
        :compile:
          :*:
            - -std=gnu17
            - -pedantic
            - -Wall
            - -Wextra
            - -D_POSIX_C_SOURCE=200809L
            - -DPHOSPHOR_VERSION=\"0.0.1-022\"
            - -DPHOSPHOR_HAS_PCRE2=1
        :link:
          :*:
            - -Lbuild/subprojects/pcre2
            - -lpcre2_8_static

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

Test Modules (38 files)
-----------------------

.. list-table::
   :header-rows: 1

   * - Module
     - Area
   * - test_alloc.c
     - core/alloc
   * - test_arena.c
     - core/arena
   * - test_bytes.c
     - core/bytes
   * - test_color.c
     - core/color
   * - test_config.c
     - core/config
   * - test_error.c
     - core/error
   * - test_str.c
     - core/str
   * - test_vec.c
     - core/vec
   * - test_lexer.c
     - args-parser/lexer
   * - test_parser.c
     - args-parser/parser
   * - test_kvp.c
     - args-parser/kvp
   * - test_validate.c
     - args-parser/validate
   * - test_path_norm.c
     - io/path_norm
   * - test_fs_atomic.c
     - io/fs_atomic
   * - test_metadata_filter.c
     - io/metadata_filter
   * - test_git_fetch.c
     - io/git_fetch
   * - test_archive.c
     - io/archive
   * - test_sha256.c
     - crypto/sha256
   * - test_regex.c
     - core/regex (PCRE2)
   * - test_manifest.c
     - template/manifest_load
   * - test_var_merge.c
     - template/var_merge
   * - test_planner.c
     - template/planner
   * - test_renderer.c
     - template/renderer
   * - test_staging.c
     - template/staging
   * - test_transform.c
     - template/transform
   * - test_writer.c
     - template/writer
   * - test_spawn.c
     - proc/spawn
   * - test_wait.c
     - proc/wait
   * - test_env.c
     - proc/env
   * - test_cli.c
     - cli dispatch + help + version
   * - test_build_cmd.c
     - commands/build
   * - test_clean_cmd.c
     - commands/clean
   * - test_create_cmd.c
     - commands/create
   * - test_certs_config.c
     - certs/config
   * - test_certs_san.c
     - certs/san
   * - test_acme_base64url.c
     - certs/acme base64url
   * - test_acme_json.c
     - certs/acme json
   * - test_toml_smoke.c
     - vendored toml-c smoke test

TEST_SOURCE_FILE Pattern
------------------------

Each test file listed its link dependencies explicitly via the
``TEST_SOURCE_FILE()`` build directive macro. For example, ``test_var_merge.c``::

    TEST_SOURCE_FILE("src/template/var_merge.c")
    TEST_SOURCE_FILE("src/args-parser/args_helpers.c")
    TEST_SOURCE_FILE("src/args-parser/spec.c")
    TEST_SOURCE_FILE("src/template/manifest_load.c")
    TEST_SOURCE_FILE("src/core/config.c")
    TEST_SOURCE_FILE("src/io/path_norm.c")
    TEST_SOURCE_FILE("src/core/alloc.c")
    TEST_SOURCE_FILE("src/core/error.c")
    TEST_SOURCE_FILE("src/core/log.c")
    TEST_SOURCE_FILE("src/core/color.c")
    TEST_SOURCE_FILE("src/platform/posix/fs_posix.c")
    TEST_SOURCE_FILE("subprojects/toml-c/toml.c")

This manual dependency tracking was the primary pain point: adding a new
source file (e.g., ``serve_cmd.c``) required updating every test file that
transitively linked it through ``cli_dispatch.c``.

Commands
--------

.. code-block:: shell

    ceedling test:all                    # run all 38 test modules
    ceedling test:test_kvp               # run single module
    ceedling gcov:all                    # coverage instrumentation
    ceedling clobber                     # clean all artifacts

Coverage
--------

- Tool: gcovr (HTML reports)
- Output: ``build/ceedling/artifacts/gcov/GcovCoverageResults.html``
- Thresholds: 75% medium, 90% high
- Excludes: ``subprojects/``, ``tests/``

Integration Tests (kept)
------------------------

Shell-based integration tests in ``tests/integration/`` are **not** part of
Ceedling and remain in the repository:

- ``test_create_golden.sh`` -- golden output for ``phosphor create``
- ``test_glow_golden.sh`` -- golden output for ``phosphor glow``

Known Issues at Removal
-----------------------

1. ``source_exclude`` with file paths emitted warnings (Ceedling 1.0.1
   expects directory paths for ``:paths`` sections).
2. UTF-8 encoding errors required ``LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8``
   environment prefix.
3. Every new command or source file required updating 5+ test files'
   ``TEST_SOURCE_FILE()`` lists to avoid undefined symbol linker errors.

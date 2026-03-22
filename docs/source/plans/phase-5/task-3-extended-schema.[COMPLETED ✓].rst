.. meta::
   :title: extended manifest schema and filters
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-20

.. index::
   triple: phase 5; task; extended schema
   single: schema versioning
   single: backward compatibility
   pair: dependency; pcre2

task 3 -- extended manifest schema and filters
================================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

extend the manifest schema with richer filter capabilities. integrate pcre2
for regex-based pattern matching alongside existing fnmatch glob patterns.
activate the filter infrastructure that was parsed but previously unused.

depends on
----------

- phase-5/task-2-archive-support

deliverables
------------

1. pcre2 vendored via meson cmake subproject (optional, compile-time toggle)
2. regex wrapper module (``regex.h`` / ``regex.c``)
3. manifest schema: ``exclude_regex`` and ``deny_regex`` filter arrays
4. filter infrastructure activation in ``writer.c`` (previously stubbed)
5. backward compatibility: schema v1 manifests work without modification

acceptance criteria
-------------------

- [✓] pcre2 integration optional (``-Dpcre2=true`` default, ``-Dpcre2=false`` to skip)
- [✓] ``exclude`` glob patterns silently skip matching files during copytree
- [✓] ``exclude_regex`` PCRE2 patterns silently skip matching files during copytree
- [✓] ``deny`` glob patterns produce exit 6 error if matched
- [✓] ``deny_regex`` PCRE2 patterns produce exit 6 error if matched
- [✓] manifest with regex filters but no pcre2 compiled -> exit 2 with diagnostic
- [✓] metadata deny list (``.DS_Store``, ``._*``, etc.) applied during copytree
- [✓] schema v1 manifests continue to work without modification
- [✓] CI: ``build-no-pcre2`` job verifies build without pcre2

implementation
--------------

new files:

- ``subprojects/pcre2.wrap`` -- vendored pcre2 v10.45 via ``wrap-git``
- ``include/phosphor/regex.h`` -- PCRE2 wrapper API (compile, match, destroy)
- ``src/core/regex.c`` -- implementation, guarded by ``#ifdef PHOSPHOR_HAS_PCRE2``
- ``tests/unit/test_regex.c`` -- compile/match/destroy tests

modified files:

- ``meson.options`` -- added ``pcre2`` boolean toggle (default true)
- ``meson.build`` -- added regex.c source, cmake subproject with static 8-bit config
- ``include/phosphor/manifest.h`` -- extended ``ph_filters_t`` with regex arrays
- ``src/template/manifest_load.c`` -- parse ``exclude_regex``/``deny_regex``, pcre2 check
- ``src/template/writer.c`` -- activated filter infrastructure:

  - ``exec_filter_cb()`` for copytree (metadata deny + glob exclude + regex exclude)
  - ``check_deny()`` before COPY/RENDER ops (glob deny + regex deny)
  - compiled regex patterns reused across operations, freed at end

- ``tests/unit/test_clean_cmd.c`` -- added ``TEST_SOURCE_FILE`` for ``regex.c``
- ``tests/unit/test_cli.c`` -- same addition
- ``.gitignore`` -- added ``subprojects/pcre2/``
- ``.github/workflows/pipeline.yaml`` -- added ``build-no-pcre2`` job

filter application:

.. list-table::
   :header-rows: 1
   :widths: 25 20 25 30

   * - filter
     - source
     - applied where
     - on match
   * - metadata deny
     - hard-coded
     - copytree callback
     - silent skip
   * - ``exclude`` (globs)
     - ``[filters]``
     - copytree callback
     - silent skip
   * - ``exclude_regex``
     - ``[filters]``
     - copytree callback
     - silent skip
   * - ``deny`` (globs)
     - ``[filters]``
     - check_deny before COPY/RENDER
     - error exit 6
   * - ``deny_regex``
     - ``[filters]``
     - check_deny before COPY/RENDER
     - error exit 6

references
----------

- masterplan lines 545-552 (versioning policy)
- masterplan lines 491-495 (tier 2 deps)

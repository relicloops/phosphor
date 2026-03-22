.. meta::
   :title: archive template support
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-20

.. index::
   triple: phase 5; task; archive support
   single: libarchive
   single: checksum verification
   pair: dependency; libarchive

task 2 -- archive template support
====================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

integrate libarchive for template pack import/export workflows. support
.tar.gz, .tar.zst, and .zip template archives with checksum verification.

depends on
----------

- phase-5/task-1-libgit2-source

deliverables
------------

1. libarchive integration (tier 1 dependency, already in masterplan)
2. archive extraction to temporary directory
3. checksum verification (sha256) for downloaded archives
4. zip/tar slip prevention (reject paths escaping archive root)
5. optional zstd compression support (tier 2: zstd dependency)

acceptance criteria
-------------------

- [✓] .tar.gz archive extracts and creates project successfully
- [✓] .tar.zst archive extracts and creates project successfully
- [✓] .zip archive extracts and creates project successfully
- [✓] zip slip attack rejected (path traversal in archive)
- [✓] absolute path in archive rejected (zip/tar slip)
- [✓] checksum mismatch -> exit 6 with clear message
- [✓] corrupted archive -> exit 3 with clear message
- [✓] archive temporary directory cleaned up
- [✓] graceful degradation when compiled without libarchive (exit 2)

implementation
--------------

new files:

- ``include/phosphor/sha256.h`` -- SHA256 hash API (always compiled, zero external deps)
- ``src/crypto/sha256.c`` -- public-domain SHA256, reads files in 8KB chunks
- ``include/phosphor/archive.h`` -- archive format detection + extraction API
- ``src/io/archive.c`` -- detection always compiled, extraction guarded by ``#ifdef PHOSPHOR_HAS_LIBARCHIVE``
- ``subprojects/libarchive.wrap`` -- vendored libarchive v3.8.5 via ``wrap-git``
- ``tests/unit/test_sha256.c`` -- 11 tests (known vectors, verify match/mismatch, NULL handling)
- ``tests/unit/test_archive.c`` -- 15 tests (format detection, cleanup, NULL/empty)

modified files:

- ``meson.options`` -- added ``libarchive`` boolean toggle (default true)
- ``meson.build`` -- added sources, cmake subproject with zlib/zstd config, macOS link deps
- ``src/commands/phosphor_commands.c`` -- added ``--checksum`` argspec to ``create_specs[]``
- ``src/commands/create_cmd.c`` -- archive branch in template resolution, ``extract_dir`` cleanup
- ``tests/unit/test_clean_cmd.c`` -- added ``TEST_SOURCE_FILE`` for ``archive.c``, ``sha256.c``
- ``tests/unit/test_cli.c`` -- same additions
- ``.gitignore`` -- added ``subprojects/libarchive/``
- ``.github/workflows/pipeline.yaml`` -- added ``zstd`` dep, added ``build-no-libarchive`` job
- ``docs/source/reference/exit-codes-and-logging.rst`` -- archive error scenarios

template resolution order in ``create_cmd.c``:

1. ``ph_git_is_url()`` -> git clone (existing)
2. ``ph_archive_detect() != PH_ARCHIVE_NONE`` -> archive extract (new)
3. else -> local directory (existing)

error code mapping:

.. list-table::
   :header-rows: 1
   :widths: 40 10 20

   * - scenario
     - exit
     - category
   * - libarchive not compiled in
     - 2
     - ``PH_ERR_USAGE``
   * - corrupted/unsupported archive
     - 3
     - ``PH_ERR_CONFIG``
   * - temp dir creation / write failure
     - 4
     - ``PH_ERR_FS``
   * - archive file not found
     - 4
     - ``PH_ERR_FS``
   * - checksum mismatch
     - 6
     - ``PH_ERR_VALIDATE``
   * - zip/tar slip (traversal or absolute path)
     - 6
     - ``PH_ERR_VALIDATE``
   * - signal interrupt during extraction
     - 8
     - ``PH_ERR_SIGNAL``

references
----------

- masterplan lines 476 (libarchive)
- masterplan lines 494 (zstd)
- masterplan lines 864 (zip/tar slip prevention)

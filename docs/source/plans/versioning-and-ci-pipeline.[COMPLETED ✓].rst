.. meta::
   :title: versioning system and CI/CD pipeline upgrade
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-15

.. index::
   single: versioning
   single: ci/cd
   single: gitlab
   pair: command; version

versioning system and CI/CD pipeline upgrade
=============================================

problem
-------

phosphor has version ``0.1.0`` hardcoded in 5+ places with no single source of
truth and no build suffix. the CI pipeline runs on all commits with no release
workflow. this plan introduces ``vMAJOR.MINOR.PATCH-BUILD`` versioning with
meson-driven propagation and a tag-driven CI/CD pipeline.

version format
--------------

``vMAJOR.MINOR.PATCH-BUILD``

- build suffix is zero-padded: ``-000`` to ``-999`` (3 digits while MINOR < 10)
- when MINOR reaches 10+, switch to 4-digit build width: ``-0000`` to ``-9999``
- when build reaches max, bump PATCH and reset build:
  ``v0.6.0-999`` -> ``v0.6.1-000``

single source of truth
----------------------

``meson.build`` defines ``phosphor_version = '0.0.0-006'`` and passes it via
``-DPHOSPHOR_VERSION`` to the executable. all other locations reference this
value:

.. list-table::
   :header-rows: 1
   :widths: 30 25 20 25

   * - File
     - Variable / Field
     - Format
     - Notes
   * - ``meson.build``
     - ``phosphor_version``
     - ``MAJOR.MINOR.PATCH-BUILD``
     - authoritative
   * - ``meson.build``
     - ``project(version)``
     - ``MAJOR.MINOR.PATCH``
     - meson convention
   * - ``docs/source/conf.py``
     - ``release``
     - ``MAJOR.MINOR.PATCH-BUILD``
     - keep in sync
   * - ``project.yml``
     - compile flags
     - ``-DPHOSPHOR_VERSION=...``
     - test-only copy

implementation steps
--------------------

1. ✓ ``meson.build``: add ``phosphor_version`` variable, pass ``-DPHOSPHOR_VERSION``
   to executable ``c_args``
2. ✓ ``cli_version.c``: replace hardcoded string with ``PHOSPHOR_VERSION`` macro
3. ✓ ``manifest_load.c``: replace hardcoded version comparison with
   ``sscanf(PHOSPHOR_VERSION, ...)``
4. ✓ ``project.yml``: add ``-DPHOSPHOR_VERSION`` to Ceedling compile flags
5. ✓ ``docs/source/conf.py``: sync ``release`` field
6. ✓ ``instructions/AI-AGENTS.md``: add version bumping rules and version
   locations reference
7. ✓ ``.gitlab-ci.yml``: full pipeline upgrade (tag-only workflow, build, test,
   docs, package, release, cleanup)
8. ✓ this plan documentation file

CI/CD pipeline design
---------------------

- ✓ **workflow**: tag-only ``^v\d+\.\d+\.\d+-\d+$`` (no branch pipelines)
- ✓ **build**: meson setup + compile + smoke test + ``meson install --strip``
- ✓ **unit-tests**: ceedling test:all
- ✓ **integration-tests**: uses build artifact, runs shell test script
- ✓ **pages**: sphinx docs (deploys with each release)
- ✓ **package-linux-arm64**: tar.gz + sha256 (GitLab, arm64)
- ✓ **package-linux-amd64**: tar.gz + sha256 (GitHub Actions, amd64)
- ✓ **release**: GitLab release API + GitHub softprops/action-gh-release
- ✓ **cleanup-failed-tag**: delete tag from remote on pipeline failure
- ✓ **coverage**: gcov/gcovr informative job (added v0.0.0-006)

dual CI platforms
~~~~~~~~~~~~~~~~~

- **GitLab CI** (``.gitlab-ci.yml``): arm64 runner, full pipeline
- **GitHub Actions** (``.github/workflows/pipeline.yaml``): amd64 ubuntu-latest

both trigger on tags matching ``vMAJOR.MINOR.PATCH-BUILD`` only.

verification
------------

.. code-block:: bash

   meson setup build && meson compile -C build && ./build/phosphor version
   # expected: phosphor 0.0.0-006

   ceedling test:all
   # expected: all tests pass (PHOSPHOR_VERSION defined)

   git tag v0.0.0-006 && git push --tags
   # expected: pipelines trigger on both GitLab and GitHub

files touched
-------------

- ``meson.build``
- ``src/cli/cli_version.c``
- ``src/template/manifest_load.c``
- ``project.yml``
- ``docs/source/conf.py``
- ``instructions/AI-AGENTS.md``
- ``.gitlab-ci.yml``
- ``.github/workflows/pipeline.yaml``
- ``docs/source/plans/versioning-and-ci-pipeline.[COMPLETED ✓].rst``
- ``docs/source/plans/index.[ACTIVE ▸].rst``

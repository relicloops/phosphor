.. meta::
   :title: toml-c dependency integration
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; toml-c integration
   single: toml
   single: vendoring
   pair: dependency; toml-c

task 1 -- toml-c dependency integration
=========================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

integrate the toml-c library (arp242 fork) into the build system. configure
meson.build for header-only or compiled-library usage. lock version in build
config. verify toml 1.1 parsing works with a minimal test.

depends on
----------

- phase-1/task-1-project-scaffold

deliverables
------------

1. toml-c integrated into meson.build (vendored or subproject/wrap)
2. version locked in build config
3. minimal smoke test parsing a sample toml file
4. docs/dependencies.rst entry with reason and risk notes

acceptance criteria
-------------------

- [ ] toml-c compiles as part of phosphor build
- [ ] sample toml file parsed successfully in a test
- [ ] version pinned and documented
- [ ] fallback to tomlc17 noted in docs if toml-c stalls

references
----------

- masterplan lines 466-475 (tier 1 dependencies)
- masterplan lines 502-506 (dependency governance)

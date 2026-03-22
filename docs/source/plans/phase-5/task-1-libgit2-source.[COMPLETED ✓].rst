.. meta::
   :title: libgit2 remote template source
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-20

.. index::
   triple: phase 5; task; libgit2 integration
   single: remote templates
   single: git clone
   pair: dependency; libgit2

task 1 -- libgit2 remote template source
==========================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

integrate optional libgit2 dependency for fetching templates from git
repositories without shelling out to git. this is a tier 2 dependency,
enabled by compile-time build flag.

depends on
----------

- phase-2 complete

deliverables
------------

1. libgit2 integration in meson.build (optional, compile-time toggle)
2. git clone/fetch into temporary directory
3. template source resolution: detect url -> fetch via libgit2 -> extract to
   local path -> proceed with existing create pipeline
4. branch/tag/commit ref support in template url
5. graceful degradation: if libgit2 not compiled in -> exit 2 with diagnostic

acceptance criteria
-------------------

- [✓] ``phosphor create --template=https://github.com/user/repo`` fetches and
  creates
- [✓] branch/tag ref supported (e.g., --template=url#branch)
- [✓] without libgit2: clear error message suggesting recompile with remote
  support
- [✓] fetched template passes same validation as local template
- [✓] temporary clone directory cleaned up on success and failure

references
----------

- masterplan lines 491-493 (tier 2: libgit2)
- masterplan lines 1212-1214 (url rejection without libgit2)

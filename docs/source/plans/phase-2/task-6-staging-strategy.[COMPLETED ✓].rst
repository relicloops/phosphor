.. meta::
   :title: staging directory lifecycle
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; staging strategy
   single: modules; staging
   single: atomic operations
   single: signal handling

task 6 -- staging directory lifecycle
=======================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement the staging directory strategy -- create
.phosphor-staging-<pid>-<timestamp> in destination parent, write all operations
into staging, atomic rename to final destination on success, full cleanup on
failure or signal interrupt.

depends on
----------

- task-4-path-and-fs

deliverables
------------

1. staging directory creation with unique naming (.phosphor-staging-<pid>-<timestamp>)
2. all write operations target staging directory
3. success path: atomic rename to final destination
4. failure path: full staging directory removal
5. signal interrupt path: check sig_atomic_t flag between loop iterations, trigger cleanup
6. EXDEV fallback: recursive copy + staging removal when rename(2) fails cross-device
7. stale detection: identify leftover staging dirs from prior crashes

acceptance criteria
-------------------

- [ ] staging dir created in destination parent directory
- [ ] successful create ends with atomic rename
- [ ] failed create removes staging directory entirely
- [ ] SIGINT during copy triggers cleanup (staging removed)
- [ ] EXDEV fallback works and logs at warn level
- [ ] stale staging detection works for phosphor doctor/clean --stale

references
----------

- masterplan lines 766-779 (staging strategy)
- masterplan lines 876-896 (signal handling)

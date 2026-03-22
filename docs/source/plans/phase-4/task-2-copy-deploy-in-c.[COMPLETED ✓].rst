.. meta::
   :title: internalize copy/deploy orchestration
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-15

.. index::
   triple: phase 4; task; internalize copy deploy
   single: script elimination
   single: native orchestration

task 2 -- internalize copy/deploy orchestration
=================================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

move the copy and deploy orchestration currently handled by build.sh into
native c modules. replace shell-based file copying, directory creation, and
asset deployment with the io module already built in phase 2.

depends on
----------

- phase-4/task-1-clean-command

deliverables
------------

1. native c implementation of build.sh copy/deploy logic
2. reuse io/fs_copytree.c and io/fs_atomic.c from phase 2
3. esbuild invocation remains external (through proc layer)
4. deterministic output matching previous shell behavior

acceptance criteria
-------------------

- [x] build output identical to shell-based build (byte comparison)
- [x] no bash dependency for standard build workflow
- [x] esbuild still invoked through proc layer
- [x] all integration tests from phase 3 still pass
- [x] metadata hygiene still enforced

references
----------

- masterplan lines 807-811 (stage b: internalized mode)

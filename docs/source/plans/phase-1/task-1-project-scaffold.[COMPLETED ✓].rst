.. meta::
   :title: project scaffolding
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; project scaffold
   single: meson
   single: build system
   single: sanitizers

task 1 -- project scaffolding
==============================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

set up the phosphor c project structure -- meson.build, directory tree
matching the masterplan source tree (include/phosphor/\*.h stubs, src/ subdirs,
tests/ subdirs), compiler flags (-Wall -Wextra -Wpedantic, C17), sanitizer
build targets (ASan, UBSan).

depends on
----------

- phase 0 task 2 (frozen grammar)
- phase 0 task 3 (exit codes)

deliverables
------------

1. meson.build with debug/release/sanitizer configurations
2. all header files as stubs with include guards
3. all .c files as stubs with minimal main()
4. project compiles with zero warnings on empty stubs
5. sanitizer targets build successfully

acceptance criteria
-------------------

- [ ] ``meson setup build`` succeeds
- [ ] ``ninja -C build`` compiles with zero warnings
- [ ] sanitizer build targets exist and link
- [ ] directory tree matches masterplan source tree (lines 274-352)

references
----------

- masterplan lines 274-352 (source tree)
- masterplan lines 953-976 (build system)

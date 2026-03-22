.. meta::
   :title: platform abstraction layer
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; platform layer
   single: modules; platform_common
   single: posix
   single: modules; signal

task 3 -- platform abstraction layer
======================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement platform_common.c and posix-specific stubs (fs_posix.c,
proc_posix.c, clock_posix.c) plus signal.c. the platform layer provides the
OS abstraction boundary for filesystem operations, path normalization, process
spawn/wait, timing, and signal handling.

depends on
----------

- task-2-core-primitives

deliverables
------------

1. platform.h/platform_common.c -- common platform detection and dispatch
2. posix/fs_posix.c -- POSIX filesystem primitives (open, read, write, stat, rename, fsync, chmod, fnmatch)
3. posix/proc_posix.c -- fork/exec/waitpid abstraction
4. posix/clock_posix.c -- high-resolution monotonic timing
5. signal.h/signal.c -- SIGINT/SIGTERM handler (volatile sig_atomic_t flag), SIGPIPE ignored

acceptance criteria
-------------------

- [ ] signal handler sets flag only, no heap work in signal context
- [ ] SIGPIPE is SIG_IGN
- [ ] all POSIX calls have error checking with meaningful error propagation
- [ ] platform.h exposes OS-agnostic API surface
- [ ] compiles on macOS and linux

references
----------

- masterplan lines 876-896 (signal handling)
- masterplan lines 920-950 (cross-platform)
- masterplan lines 337-343 (source tree platform/)

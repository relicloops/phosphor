.. meta::
   :title: process execution layer
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 3; task; process layer
   single: modules; spawn
   single: modules; env
   single: modules; wait
   single: fork exec

task 1 -- process execution layer
===================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement spawn.c (fork/exec child processes with explicit argv, no shell
interpolation), env.c (environment variable sanitization with allowlist model),
wait.c (waitpid with exit code mapping to phosphor exit codes 0-8). the proc
layer is the foundation for build command's script execution.

depends on
----------

- phase-1/task-3-platform-layer

deliverables
------------

1. proc.h -- process abstraction API (ph_env_t, ph_proc_opts_t,
   ph_argv_builder_t types, full function declarations)
2. spawn.c -- argv builder (init/push/pushf/finalize/free/destroy),
   ph_proc_exec orchestrator (env extraction, spawn, signal check, exit mapping)
3. env.c -- environment sanitization with 16-entry system allowlist,
   PHOSPHOR_* prefix match, caller extras (exact + prefix via trailing _),
   ph_env_set for key add/replace, ph_env_destroy
4. wait.c -- ph_proc_map_exit: 0->0, 1->1, 2-7->pass-through, 8-127->1
   (general), 128+->8 (signal), signaled->8, NULL->1

platform layer extensions:

5. platform.h -- ph_proc_spawn signature extended with ``envp`` parameter
6. proc_posix.c -- setpgid process group isolation (child + parent race
   avoidance), environ override when envp non-NULL, SIGINT forwarding to
   child process group on EINTR

exit code mapping
-----------------

- child exit 0 -> phosphor exit 0 (success)
- child exit 1 -> phosphor exit 1 (general)
- child exit 2-7 -> phosphor exit 2-7 (direct pass-through)
- child exit 8-127 -> phosphor exit 1 (general/unmapped)
- child exit 128+ -> phosphor exit 8 (signal)
- child signaled -> phosphor exit 8 (signal)
- NULL input -> phosphor exit 1 (general)

security
--------

- no shell interpolation for process args (argv arrays only)
- sanitized env passthrough (allowlist model)
- signal forwarding: SIGINT forwarded to child process group during build
- child process isolation via setpgid(0, 0)

unit tests
----------

- test_wait.c: 14 tests covering all exit code mapping branches
- test_env.c: 10 tests covering allowlist, PHOSPHOR_* prefix, extras
  (exact + prefix), set new/override, NULL safety
- test_spawn.c: 13 tests covering argv builder lifecycle and exec
  integration (echo success, false -> general error)

acceptance criteria
-------------------

- [x] child process spawned with explicit argv, no shell involvement
- [x] env allowlist prevents leaking unexpected variables
- [x] child exit codes correctly mapped to phosphor exit codes
- [x] SIGINT forwarded to child process group
- [x] child process timeout support (field declared, ignored in v1,
  logs warning if non-zero)
- [x] 37 unit tests across 3 test files

references
----------

- masterplan lines 322-325 (proc source tree)
- masterplan lines 859-867 (security)
- masterplan lines 893-896 (signal forwarding)

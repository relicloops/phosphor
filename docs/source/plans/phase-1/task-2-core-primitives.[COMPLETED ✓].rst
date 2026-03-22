.. meta::
   :title: core primitives implementation
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; core primitives
   single: modules; alloc
   single: modules; str
   single: modules; vec
   single: modules; bytes
   single: modules; arena
   single: modules; error

task 2 -- core primitives implementation
=========================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement the core module -- alloc.c (allocator wrapper with debug mode),
bytes.c (byte slice operations), str.c (string utilities), vec.c (dynamic
array), arena.c (arena allocator), error.c (error type and cause chains),
log.c (leveled logging). every struct must document ownership for each pointer
field. destructors must be idempotent and null-safe.

depends on
----------

- task-1-project-scaffold

deliverables
------------

1. alloc.h/alloc.c -- malloc/calloc/realloc/free wrappers, debug canary/poison mode
2. bytes.h/bytes.c -- uint8_t* + size_t slice operations
3. str.h/str.c -- owned string utilities (not C string assumptions)
4. vec.h/vec.c -- generic dynamic array with typed accessors
5. arena.h/arena.c -- bump allocator for short-lived command lifecycle
6. error.h/error.c -- error category, subcode, message, cause chain
7. log.h/log.c -- error/warn/info/debug/trace levels, stderr output

acceptance criteria
-------------------

- [ ] every allocation checked; failure returns error, no partial ownership leakage
- [ ] every destructor idempotent and null-safe
- [ ] every struct documents owner for each pointer field
- [ ] compiles clean under -Wall -Wextra -Wpedantic
- [ ] ASan clean (no leaks in basic usage)

references
----------

- masterplan lines 376-392 (module responsibilities)
- masterplan lines 395-424 (memory management)

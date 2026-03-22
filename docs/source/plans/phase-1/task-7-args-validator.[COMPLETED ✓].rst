.. meta::
   :title: args semantic validator
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; semantic validator
   single: modules; validate
   single: ux diagnostics
   single: type checking

task 7 -- args semantic validator
==================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement validate.c -- semantic validation layer that takes parsed command
struct + ArgSpec registry and performs type checking, constraint validation,
and conflict detection. emits diagnostics with UX subcodes.

depends on
----------

- task-5-argspec-registry
- task-6-kvp-parser

deliverables
------------

1. validate.c -- validation pass over parsed command struct
2. diagnostic emission with flag name, expected type, received token, source token index

validations performed
~~~~~~~~~~~~~~~~~~~~~

- type mismatch detection (UX005)
- enum choice validation (UX007)
- path constraint checking (relative-only if declared)
- url scheme validation (require explicit https:// or http://)
- enable/disable conflict detection (UX004)
- duplicate flag detection (UX003)
- action modifier =value rejection

acceptance criteria
-------------------

- [x] all seven UX diagnostic subcodes (UX001-UX007) can be triggered and tested
- [x] diagnostics include: flag name, expected type, received token, token index
- [x] validation is data-only pass, no side effects
- [x] exit code 2 set for all validation failures

implementation notes
--------------------

- ``src/args-parser/validate.c`` -- full implementation (180 lines)
- internal helpers: ``is_integer_string()``, ``is_valid_url()``, ``enum_matches()``
- step 1: per-flag spec lookup + type/form checks (UX001, UX002, UX005, UX006, UX007)
- step 2: iterate all specs, verify required flags present (UX002)
- first-error-wins pattern, matching parser convention
- KVP validation delegates to ``ph_kvp_parse()`` with error chaining
- bug fix: ``kvp.c`` -- replaced ``ph_kvp_destroy(&node, 1)`` on stack-allocated
  nodes with ``kvp_node_free_fields()`` helper to avoid freeing stack addresses

references
----------

- masterplan lines 245-248 (semantic validation note)
- masterplan lines 250-264 (diagnostics and output guarantee)

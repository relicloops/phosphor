.. meta::
   :title: template engine (planner, renderer, transform, writer)
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; template engine
   single: modules; planner
   single: modules; renderer
   single: modules; transform
   single: modules; writer
   single: placeholder substitution

task 5 -- template engine (planner, renderer, transform, writer)
=================================================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement the template processing pipeline -- planner.c (build in-memory
operation plan from manifest ops), renderer.c (placeholder substitution with
<<var_name>> syntax), transform.c (text transforms including newline
normalization), writer.c (execute plan into staging directory).

depends on
----------

- task-3-variable-merge
- task-4-path-and-fs

deliverables
------------

1. planner.c -- read [[ops]] from manifest, build ordered operation plan, validate (cycle/conflict/overwrite checks)
2. renderer.c -- scan text files for <<var_name>> placeholders, substitute resolved variable values
3. transform.c -- text transforms: newline normalization (--normalize-eol), binary detection (NUL-byte heuristic + extension table)
4. writer.c -- execute plan: mkdir, copy, render, chmod, remove operations into staging dir

placeholder rules
~~~~~~~~~~~~~~~~~

- syntax: ``<<var_name>>``
- escape: ``\<<`` emits literal ``<<``
- only applied to render operations
- undefined placeholder -> hard error (unless variable is required=false with empty fallback)
- binary files never expanded

operation kinds
~~~~~~~~~~~~~~~

mkdir, copy, render, chmod, remove

acceptance criteria
-------------------

- [ ] placeholder substitution produces correct output for all variable types
- [ ] escaped ``\<<`` emits literal ``<<``
- [ ] undefined placeholder in required variable -> hard error
- [ ] binary file detection prevents placeholder expansion on binary files
- [ ] remove operations enforce safety constraints (within dest root, no symlink follow)
- [ ] condition expressions (if field) evaluated correctly per grammar

references
----------

- masterplan lines 604-623 (ops + remove safety)
- masterplan lines 664-675 (templating)
- masterplan lines 651-662 (condition grammar)

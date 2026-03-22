.. meta::
   :title: argspec typed flag registry
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; argspec registry
   single: modules; spec
   single: type system
   single: flag registry

task 5 -- argspec typed flag registry
=======================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement spec.c -- the typed flag specification registry. each command
registers its accepted flags with types (string, int, bool, enum, path, url,
kvp). the registry is used by the validator to resolve untyped scalar tokens
into their declared types.

depends on
----------

- task-4-args-lexer-parser

deliverables
------------

1. spec.c -- ArgSpec struct definition, per-command flag registration
2. type enum: string, int, bool, enum, path, url, kvp
3. registration API for commands to declare their flags
4. lookup API for validator to query flag type and constraints

per-command flag specs (from appendices)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- create: 15 flags (appendix a)
- build: 8 flags (appendix b)
- clean: 4 flags (appendix c)
- doctor: 3 flags (appendix d)
- version/help: no flags

acceptance criteria
-------------------

- [ ] every flag in appendices a-d has a registered ArgSpec
- [ ] each spec declares: name, type, required/optional, default, constraints
- [ ] enum specs declare valid choices
- [ ] action modifier specs reject =value attempts
- [ ] feature toggle specs register both --enable-x and --disable-x forms

references
----------

- masterplan lines 187-201 (typed flag contract)
- masterplan lines 1186-1252 (appendices a-e)

.. meta::
   :title: variable schema and merge pipeline
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; variable merge
   single: modules; var_merge
   single: precedence chain
   single: regex

task 3 -- variable schema and merge pipeline
==============================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement the variable resolution pipeline: cli flags -> environment variables
-> project-local config -> manifest defaults. parse [[variables]] table from
manifest, enforce type/pattern/choices constraints.

depends on
----------

- task-2-manifest-loader

deliverables
------------

1. variable schema parser -- read [[variables]] entries from manifest
2. merge pipeline -- resolve variable values through precedence chain
3. pattern validation using posix regex.h (regcomp/regexec with REG_EXTENDED)
4. enum choice validation
5. required variable enforcement

variable schema fields
~~~~~~~~~~~~~~~~~~~~~~

- name, type (string|bool|int|enum|path|url), required, default, env, prompt, pattern (posix ere), min/max, choices, secret

config precedence
~~~~~~~~~~~~~~~~~

cli flags > env vars (PHOSPHOR_*) > project-local config > manifest defaults

acceptance criteria
-------------------

- [ ] cli flag overrides env var overrides config overrides default
- [ ] required variable with no value from any source -> exit 6
- [ ] pattern mismatch -> exit 6 with pattern and value shown
- [ ] enum value not in choices -> exit 6
- [ ] int outside min/max bounds -> exit 6

references
----------

- masterplan lines 522-533 (config precedence)
- masterplan lines 584-595 (variable schema)
- masterplan lines 478-481 (posix regex)

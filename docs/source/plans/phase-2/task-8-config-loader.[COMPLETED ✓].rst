.. meta::
   :title: project-local config loader
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; config loader
   single: modules; config
   single: precedence
   single: discovery

task 8 -- project-local config loader
=======================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement config.c -- load project-local phosphor configuration file. this sits
in the variable merge precedence chain between env vars and manifest defaults.

depends on
----------

- task-3-variable-merge

deliverables
------------

1. config.h/config.c -- load project-local config file (toml format)
2. config file discovery: search current directory and parents for .phosphor.toml or phosphor.toml
3. config values feed into variable merge pipeline at precedence level 3

config precedence reminder
~~~~~~~~~~~~~~~~~~~~~~~~~~

cli flags (1) > env vars (2) > project-local config (3) > manifest defaults (4)

acceptance criteria
-------------------

- [ ] config file discovered in current or parent directories
- [ ] config values override manifest defaults
- [ ] config values overridden by env vars and cli flags
- [ ] missing config file is not an error (optional)
- [ ] malformed config file -> exit 3

references
----------

- masterplan lines 509-527 (config and template model)
- masterplan lines 367 (header mapping: config.h -> core/config.c)

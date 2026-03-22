.. meta::
   :title: manifest loader
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; manifest loader
   single: modules; manifest_load
   single: schema validation
   single: template.phosphor.toml

task 2 -- manifest loader
==========================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement manifest_load.c -- parse template.phosphor.toml using toml-c, perform
schema validation (required keys, type checks), and produce an in-memory
manifest struct. this is the entry point for all template processing.

depends on
----------

- task-1-toml-integration

deliverables
------------

1. manifest.h -- manifest struct definition (manifest metadata, template metadata, defaults, variables, filters, ops, hooks)
2. manifest_load.c -- toml parsing -> manifest struct, with validation

validation order (from masterplan)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. parse toml -> syntax validation
2. schema validation -> required keys and type checks
3. semantic validation -> path safety, duplicate ids, unknown refs

required keys enforced
~~~~~~~~~~~~~~~~~~~~~~

- manifest.schema (positive integer)
- manifest.id (slug)
- manifest.version (semver string)
- template.source_root (relative path)
- at least one [[ops]] entry

acceptance criteria
-------------------

- [ ] valid manifest parses to complete struct
- [ ] missing required key -> exit code 3 with clear message
- [ ] invalid schema version -> exit code 3
- [ ] unknown keys produce warning but don't fail (per versioning policy)
- [ ] min_phosphor version check: if cli version < min_phosphor -> exit 6

references
----------

- masterplan lines 536-552 (schema)
- masterplan lines 564-582 (required/recommended keys)
- masterplan lines 741-747 (validation order)

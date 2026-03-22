.. meta::
   :title: create command implementation
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-11

.. index::
   triple: phase 2; task; create command
   single: modules; create_cmd
   pair: command; create
   single: pipeline

task 7 -- create command implementation
=========================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement create_cmd.c -- the full create command pipeline: parse args ->
resolve template source -> load manifest -> merge variables -> build operation
plan -> preflight checks -> execute into staging -> validate staging -> commit
to destination -> emit report.

depends on
----------

- task-5-template-engine
- task-6-staging-strategy

deliverables
------------

1. create_cmd.c -- full pipeline implementation
2. template source resolution (local path only for v1; url rejected if libgit2 absent -> exit 2)
3. preflight checks: collisions, permissions, forbidden paths
4. staging execution and commit
5. summary report: files copied, files transformed, bytes written, skipped files, next commands

pipeline (10 steps from masterplan)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. parse args -> validate command contract
2. resolve template source (local path)
3. load manifest and variable schema
4. merge variable values from flags/env/defaults
5. build in-memory file operation plan
6. run preflight checks
7. execute copy/render plan into staging
8. validate staging directory integrity
9. commit staging by rename/move
10. emit deterministic summary report

flags supported (appendix a)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

--name, --template, --out, --tld, --owner, --owner-slug, --github, --instagram,
--x, --force, --dry-run, --toml, --allow-hooks, --yes, --normalize-eol,
--allow-hidden, --verbose

acceptance criteria
-------------------

- [ ] ``phosphor create --name=sample`` produces valid project from template
- [ ] --dry-run shows plan without writing
- [ ] --force overwrites existing destination
- [ ] --verbose produces detailed output
- [ ] hooks displayed but not executed without --allow-hooks
- [ ] missing --name -> exit 2
- [ ] template url without libgit2 -> exit 2 with diagnostic

references
----------

- masterplan lines 750-794 (create workflow)
- masterplan lines 1186-1214 (appendix a flags)

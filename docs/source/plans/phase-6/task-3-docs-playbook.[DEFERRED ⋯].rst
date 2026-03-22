.. meta::
   :title: template author documentation and playbook
   :tags: #neonsignal, #phosphor
   :status: deferred
   :updated: 2026-02-14

.. index::
   triple: phase 6; task; documentation playbook
   single: template authoring
   single: user guide

task 3 -- template author documentation and playbook
======================================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

write comprehensive documentation for template authors -- how to create
template.phosphor.toml manifests, variable schema, operation kinds, hook
configuration, testing templates, and publishing.

depends on
----------

- phase-6/task-2-packaging

deliverables
------------

1. template authoring guide (docs/template-authoring.rst)
2. manifest reference (all fields, types, constraints)
3. variable reference (types, patterns, precedence)
4. operation reference (mkdir, copy, render, chmod, remove)
5. hook reference (security, --allow-hooks, trusted templates)
6. example templates (minimal, full-featured)
7. troubleshooting guide (common errors, diagnostics)

acceptance criteria
-------------------

- [ ] a new user can create a working template from the guide alone
- [ ] all manifest fields documented with examples
- [ ] all exit codes and diagnostics documented
- [ ] example templates compile and produce expected output
- [ ] security implications of hooks clearly explained

references
----------

- masterplan lines 1176-1183 (definition of done, item 5)
- masterplan lines 1072-1076 (phase 6)

.. meta::
   :title: phase init -- init command
   :tags: #neonsignal, #phosphor
   :status: deferred
   :updated: 2026-03-19

.. index::
   single: phase init
   single: init command

phase init -- init command
===========================

.. note::

   superseded by ``glow-command-embedded-template.[ACTIVE ▸].rst``. the ``init``
   command described here was implemented as ``glow`` instead. these task files
   remain as historical reference.

interactive project scaffolding from hardcoded C buffers. the ``init`` command
embeds template content directly in the binary and prompts users for variable
values via stdin. remote templates use a YAML syntax tree format.

.. toctree::
   :maxdepth: 1
   :caption: tasks

   task-1-template-preparation.[DRAFT ○]
   task-2-c-buffer-embedding.[DRAFT ○]
   task-3-init-command.[DRAFT ○]
   task-4-remote-yaml-templates.[DRAFT ○]

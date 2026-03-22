.. meta::
   :title: variable mapping from global_variables.sh
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; task; variable mapping
   single: environment variables
   single: flags

variable mapping from global_variables.sh
==========================================

objective
---------

produce a complete mapping table from every variable in ``scripts/global_variables.sh``
to its corresponding phosphor cli flag, environment variable, or deprecation note.

scope
-----

known mappings (from masterplan):

1. ``SNI`` → ``--name``
2. ``SITE_OWNER`` / ``SITE_OWNER_SLUG_REV`` → ``--owner`` / ``--owner-slug``
3. ``TLD`` → ``--tld``
4. ``DATA_DIR`` / ``CERTS_*`` → determine: create flags, build flags, or config-only
5. all remaining variables → document deprecation or propose new flags

deliverables
------------

1. complete variable mapping table: shell var → phosphor flag / env var / deprecated.
2. decision record for each variable that does not have an obvious mapping.
3. list of new flags to add (if any variables need new flags beyond current appendices).

acceptance criteria
-------------------

- □ every exported variable in global_variables.sh has a mapping entry.
- □ no variable left as "tbd" -- each has a concrete decision (flag, env, deprecated).
- □ any new flags proposed are consistent with the typed flag contract.

references
----------

- masterplan: migration plan, phase 0, item 7
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 1030-1035)
- masterplan: appendix a (create flags)
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 1186-1214)
- masterplan: appendix b (build flags)
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 1217-1228)

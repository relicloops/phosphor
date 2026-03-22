.. meta::
   :title: script fallback feature flag
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-15

.. index::
   triple: phase 4; task; script fallback
   single: feature flags
   single: deprecation

task 3 -- script fallback feature flag
========================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

add a feature flag (compile-time or runtime) that allows falling back to shell
script execution during the transition period. this provides a safety net
while native c implementations stabilize.

depends on
----------

- phase-4/task-2-copy-deploy-in-c

deliverables
------------

1. compile-time flag: PHOSPHOR_SCRIPT_FALLBACK (Meson build option)
2. runtime flag: --legacy-scripts (action modifier, hidden from help)
3. when enabled: use proc layer to invoke scripts instead of native c
4. deprecation warning when fallback is used
5. plan for removing fallback in later release

acceptance criteria
-------------------

- [x] compile-time flag toggles between native and script paths
- [x] runtime flag overrides to script path with deprecation warning
- [x] both paths produce identical output
- [x] fallback path documented with removal timeline

references
----------

- masterplan lines 1060-1064 (phase 4: internalize)
- masterplan lines 1094-1097 (M3: script reduction)

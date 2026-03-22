.. meta::
   :title: freeze cli grammar
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; task; cli grammar freeze
   single: ebnf
   single: grammar
   pair: specification; cli

freeze cli grammar
==================

objective
---------

formalize and freeze the ebnf grammar for all phosphor commands. the grammar is already
drafted in the masterplan; this task is the formal review and sign-off that locks it
for phase 1 implementation.

scope
-----

1. review the parser grammar (v1 draft ebnf) in the masterplan.
2. verify all non-terminals are defined and reachable.
3. verify the two-category boolean model (action modifiers + feature toggles) is
   consistently represented in the grammar.
4. verify kvp nesting rules are unambiguous.
5. produce a standalone ``docs/source/reference/cli-grammar.rst`` reference document
   (sphinx source tree).

deliverables
------------

1. frozen ebnf grammar document.
2. sign-off checklist confirming completeness.
3. list of any open questions or deferred decisions.

acceptance criteria
-------------------

- □ every non-terminal referenced in the grammar is defined.
- □ grammar covers: command_line, commands, valued flags, bool actions, bool switches,
  kvp with nesting, all terminal/literal productions.
- □ condition expression grammar included.
- □ no ambiguities identified or all ambiguities resolved with documented decisions.

references
----------

- masterplan: argument parser contract, parser grammar (v1 draft ebnf)
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 218-248)
- masterplan: condition expression grammar
  (``../phosphor-pure-c-cli-masterplan.rst`` lines 651-662)

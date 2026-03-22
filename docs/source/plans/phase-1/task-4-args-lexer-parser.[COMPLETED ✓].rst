.. meta::
   :title: args lexer and parser
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; lexer parser
   single: modules; lexer
   single: modules; parser
   single: tokenization

task 4 -- args lexer and parser
================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement lexer.c (tokenize raw argv into typed tokens) and parser.c (parse
token stream per the frozen EBNF grammar into a command struct). the parser
must handle: long flags only, assignment form (--flag=value), bare boolean
actions (--flag), polarity switches (--enable-x/--disable-x), and produce
data-only output with no side effects.

depends on
----------

- task-2-core-primitives

deliverables
------------

1. lexer.c -- tokenize argv into flag name, operator (=), value tokens
2. parser.c -- consume token stream, produce typed command struct
3. command struct definition in args.h

parsing rules enforced
~~~~~~~~~~~~~~~~~~~~~~

- valued flags require ``=`` (reject space-separated form)
- action modifiers: bare presence (no =value accepted)
- feature toggles: --enable-x / --disable-x polarity pairs
- repeated scalar flags -> hard error
- repeated bare boolean flags -> hard error
- conflicting polarity -> hard error
- unknown flags -> hard error

acceptance criteria
-------------------

- [ ] parser produces data-only structs, zero side effects
- [ ] all seven parsing rules enforced with correct UX diagnostic subcodes
- [ ] parser handles empty argv, single command, command with mixed flags
- [ ] edge cases: empty string values, quoted values, values containing special chars

references
----------

- masterplan lines 162-185 (core parsing rules)
- masterplan lines 218-248 (EBNF grammar)
- masterplan lines 250-258 (diagnostics)

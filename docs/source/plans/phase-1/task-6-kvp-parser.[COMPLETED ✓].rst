.. meta::
   :title: kvp nested object parser
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 1; task; kvp parser
   single: modules; kvp
   single: key-value pairs
   single: brace nesting

task 6 -- kvp nested object parser
====================================

.. note::

   parent plan: `../phosphor-pure-c-cli-masterplan.rst`

objective
---------

implement the kvp (key-value pair) parser for !-prefixed structured values.
handles pipe-separated pairs, colon key:value syntax, and brace-nested
sub-objects. produces typed kvp tree structure.

depends on
----------

- task-4-args-lexer-parser

deliverables
------------

1. kvp parser (in parser.c or separate kvp.c) -- parse !-prefixed kvp strings
2. kvp tree data structure (key-value nodes with nesting)

parsing rules
~~~~~~~~~~~~~

- values must begin with ``!``
- pair separator: ``|``
- pair syntax: ``key:value``
- nested objects: ``{kvp_pairs}``
- scalar types inside kvp: string (quoted/unquoted), number, bool
- duplicate keys at same depth -> rejected
- missing key/value, malformed nesting, unbalanced braces -> rejected

acceptance criteria
-------------------

- [ ] ``!x:34|y:65`` parses to flat kvp with two int entries
- [ ] ``!meta:{a:1|b:2}|enabled:true`` parses to nested kvp
- [ ] duplicate keys rejected with diagnostic
- [ ] unbalanced braces rejected with diagnostic showing position
- [ ] empty kvp ``!`` rejected
- [ ] deterministic error locations in all rejection cases

references
----------

- masterplan lines 203-216 (kvp contract)
- masterplan lines 231-235 (kvp grammar)

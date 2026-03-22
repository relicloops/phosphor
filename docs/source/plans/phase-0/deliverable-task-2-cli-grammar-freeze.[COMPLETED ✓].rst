.. meta::
   :title: cli grammar freeze sign-off
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; deliverable; grammar sign-off
   single: ebnf
   single: parser grammar
   single: disambiguation rules

cli grammar freeze sign-off
=============================

this document records the review findings and formal sign-off for the phosphor
cli grammar. the frozen grammar is published at
``tools/phosphor/docs/source/reference/cli-grammar.rst``.


non-terminal reachability audit
-------------------------------

every non-terminal in the frozen grammar is both defined and reachable from
``command_line``. the full reachability chain:

.. code-block:: text

   command_line
   ├── command           (terminal: 6 literal strings)
   └── arg
       └── long_flag
           ├── valued_flag
           │   ├── flag_ident → letter, digit
           │   └── typed_value
           │       ├── kvp
           │       │   └── kvp_pairs
           │       │       └── kvp_pair
           │       │           ├── kvp_key → ident → letter, digit
           │       │           └── kvp_value
           │       │               ├── kvp_pairs (recursive)
           │       │               └── kvp_scalar
           │       │                   ├── quoted_string → qs_char → any_char
           │       │                   ├── bool_lit
           │       │                   ├── number_lit → digit
           │       │                   └── kvp_bare_token → kbt_char → visible_char
           │       └── scalar
           │           ├── quoted_string → qs_char → any_char
           │           ├── bool_lit
           │           ├── number_lit → digit
           │           └── bare_token → bt_char → visible_char
           ├── bool_switch → flag_ident → letter, digit
           └── bool_action → flag_ident → letter, digit

condition expression reachability:

.. code-block:: text

   cond_expr
   └── cond_or
       └── cond_and
           └── cond_unary
               └── cond_primary
                   ├── cond_cmp
                   │   ├── cmp_op
                   │   └── cond_atom
                   │       ├── var_ref → ident → letter, digit
                   │       ├── quoted_string (shared)
                   │       ├── number_lit (shared)
                   │       └── bool_lit (shared)
                   └── cond_expr (recursive via parentheses)

result: ✓ all non-terminals defined and reachable. zero orphaned productions.


two-category boolean model verification
----------------------------------------

the grammar enforces the two-category boolean model:

1. **action modifiers** (``bool_action``): bare ``--flag`` presence.
   grammar: ``"--" flag_ident``.
   semantic rule: ``=value`` form rejected (ux002). repeated flags rejected (ux003).

2. **feature toggles** (``bool_switch``): explicit polarity pair.
   grammar: ``"--enable-" flag_ident | "--disable-" flag_ident``.
   semantic rule: conflicting polarity rejected (ux004).

disambiguation: DR-01 ensures ``--enable-*`` / ``--disable-*`` tokens are tried
before the generic ``bool_action`` path. without this rule, ``--enable-color``
would parse as a ``bool_action`` with ident ``enable-color``.

result: ✓ consistently represented. no overlap between categories.


kvp nesting rules verification
-------------------------------

the kvp grammar is unambiguous:

1. entry point: ``"!" kvp_pairs`` -- the ``!`` prefix is a mandatory, unambiguous
   lookahead (DR-04).

2. pair structure: ``kvp_key ":" kvp_value`` -- the ``:`` separator is excluded
   from ``kvp_bare_token`` characters (kbt_char), so there is no ambiguity about
   where the key ends and value begins.

3. nesting: ``"{" kvp_pairs "}"`` -- braces are excluded from ``kvp_bare_token``
   characters, so the parser can unambiguously detect nested objects.

4. pair separator: ``"|"`` -- excluded from ``kvp_bare_token``, so the boundary
   between adjacent pairs is unambiguous.

5. recursion terminates: ``kvp_value = "{" kvp_pairs "}" | kvp_scalar``. nesting
   depth is bounded by available input. implementation may enforce a max depth
   (recommended: 8 levels) to prevent stack overflow on malicious input.

result: ✓ kvp nesting rules unambiguous. no structural ambiguity in
key/value/separator/nesting boundaries.


issues found and resolved during review
-----------------------------------------

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - id
     - issue
     - resolution
   * - 1
     - ``letter`` range typo: ``"a"-"z" | "a"-"z"`` (duplicate lowercase)
     - fixed to ``"a"-"z" | "A"-"Z"``
   * - 2
     - ``any_char`` referenced but undefined
     - defined: any unicode codepoint except unescaped NUL
   * - 3
     - ``visible_char`` referenced but undefined
     - defined: any printable non-whitespace unicode codepoint
   * - 4
     - ``bare_token`` exclusion set incomplete (comment-only, missing ``"``, ``=``, ``!``)
     - formalized as ``bt_char`` with explicit exclusion set (DR-03)
   * - 5
     - no disambiguation between ``bool_action`` and ``bool_switch``
     - added DR-01: try ``bool_switch`` prefix first
   * - 6
     - ``scalar`` alternatives overlap (``bare_token`` matches everything)
     - added DR-02: precedence ordering (quoted_string > bool_lit > number_lit > bare_token)
   * - 7
     - condition grammar used ``string_lit``/``int_lit`` (names not in parser grammar)
     - unified to ``quoted_string``/``number_lit``
   * - 8
     - ``kvp_atom`` name conflicts conceptually with ``cond_atom``
     - renamed to ``kvp_value``; introduced ``kvp_scalar`` and ``kvp_bare_token``
   * - 9
     - no context-specific bare token rules for kvp leaf values
     - introduced ``kvp_bare_token`` with separate exclusion set (allows ``=`` and ``!``)


open questions (deferred)
--------------------------

1. **single-quote strings.** the grammar only supports double-quoted strings.
   single-quote support (``'...'``) could be added in a future schema version
   if user feedback warrants it. deferred to v2.

2. **escape sequences beyond ``\\"``.** the grammar only defines ``\\"`` inside
   quoted strings. sequences like ``\\n``, ``\\t``, ``\\\\`` are not specified.
   for v1, quoted strings are taken verbatim (no escape processing except
   ``\\"``). deferred to v2 if template authors need richer escaping.

3. **kvp max nesting depth.** the grammar allows unbounded nesting. implementation
   should enforce a reasonable limit (recommended: 8 levels) but the grammar
   itself does not specify this. this is a runtime guardrail, not a grammar
   constraint.

4. **float literals.** ``number_lit`` is integer-only (``["-"] digit { digit }``).
   if a future flag needs decimal values, a ``float_lit`` production would be
   added. deferred to v2.

5. **``help <command>`` sub-arguments.** the grammar allows ``phosphor help``
   followed by ``{ arg }`` (long flags only). the masterplan specifies
   ``phosphor help <command>`` with a positional argument. this is handled
   outside the grammar at the dispatch level: ``help`` accepts one optional
   positional token that matches the ``command`` production. no grammar change
   needed; dispatch logic handles it.


acceptance criteria verification
---------------------------------

- ✓ every non-terminal referenced in the grammar is defined.
- ✓ grammar covers: command_line, commands, valued flags, bool actions, bool
  switches, kvp with nesting, all terminal/literal productions.
- ✓ condition expression grammar included.
- ✓ all ambiguities resolved with documented decisions (DR-01 through DR-05).

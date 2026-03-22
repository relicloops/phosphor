phosphor cli grammar (frozen v1)
=================================

this document is the frozen reference grammar for the phosphor cli argument
parser. phase 1 implementation must code against this specification exactly.

changes to this grammar require a new version bump and formal review.


notation
--------

this grammar uses iso 14977 ebnf with the following conventions:

- ``=`` defines a production.
- ``|`` separates alternatives.
- ``{ ... }`` zero or more repetitions.
- ``[ ... ]`` optional (zero or one).
- ``"..."`` terminal literal.
- ``(* ... *)`` comment.
- ``-`` set difference (exclusion).
- ``;`` terminates a production.
- character ranges use ``"a"-"z"`` notation.


part 1: argument parser grammar
---------------------------------

.. code-block:: text

   (* ── top-level ─────────────────────────────────────────────── *)

   command_line   = "phosphor" command { arg } ;

   command        = "create"
                  | "build"
                  | "clean"
                  | "rm"
                  | "certs"
                  | "doctor"
                  | "version"
                  | "help" ;

   arg            = long_flag ;

   (* ── flag categories ───────────────────────────────────────── *)

   long_flag      = valued_flag
                  | bool_switch          (* try before bool_action -- see DR-01 *)
                  | bool_action ;

   valued_flag    = "--" flag_ident "=" typed_value ;

   bool_action    = "--" flag_ident ;

   bool_switch    = "--enable-" flag_ident
                  | "--disable-" flag_ident ;

   (* ── typed values ──────────────────────────────────────────── *)

   typed_value    = kvp                  (* lookahead: starts with "!" *)
                  | scalar ;

   scalar         = quoted_string
                  | bool_lit             (* try before bare_token -- see DR-02 *)
                  | number_lit           (* try before bare_token -- see DR-02 *)
                  | bare_token ;         (* fallback *)

   (* ── kvp (key-value payload) ───────────────────────────────── *)

   kvp            = "!" kvp_pairs ;

   kvp_pairs      = kvp_pair { "|" kvp_pair } ;

   kvp_pair       = kvp_key ":" kvp_value ;

   kvp_value      = "{" kvp_pairs "}"    (* nested object *)
                  | kvp_scalar ;         (* leaf value *)

   kvp_scalar     = quoted_string
                  | bool_lit
                  | number_lit
                  | kvp_bare_token ;

   kvp_key        = ident ;

   (* ── terminals ─────────────────────────────────────────────── *)

   flag_ident     = letter { letter | digit | "-" | "_" } ;
                  (* flag identifiers: lowercase-start, kebab/snake allowed *)

   ident          = letter { letter | digit | "-" | "_" } ;
                  (* general identifiers: same rules as flag_ident *)

   quoted_string  = '"' { qs_char } '"' ;
   qs_char        = any_char - '"'
                  | '\\"' ;              (* escaped double-quote *)

   bare_token     = bt_char { bt_char } ;
   bt_char        = visible_char - ( '"' | "=" | "|" | "{" | "}" | "!" ) ;
                  (* excludes: whitespace, double-quote, equals, pipe,
                     braces, bang. see DR-03 for rationale. *)

   kvp_bare_token = kbt_char { kbt_char } ;
   kbt_char       = visible_char - ( '"' | ":" | "|" | "{" | "}" ) ;
                  (* excludes: whitespace, double-quote, colon, pipe, braces.
                     note: "!" and "=" are allowed inside kvp leaf values. *)

   number_lit     = [ "-" ] digit { digit } ;

   bool_lit       = "true" | "false" ;

   (* ── character classes ─────────────────────────────────────── *)

   letter         = "a"-"z" | "A"-"Z" ;

   digit          = "0"-"9" ;

   visible_char   = (* any unicode codepoint with general category L, M, N, P,
                       or S -- i.e., any printable non-whitespace character.
                       implementation: any byte >= 0x21 in the ascii range;
                       for utf-8 multi-byte sequences, any sequence that does
                       not decode to a whitespace or control codepoint. *) ;

   any_char       = (* any unicode codepoint except unescaped NUL (0x00).
                       implementation: any byte or multi-byte utf-8 sequence. *) ;


part 2: condition expression grammar
--------------------------------------

used in ``[[ops]]`` and ``[[hooks]]`` ``if`` fields inside
``template.phosphor.toml``. evaluated at create-time against resolved variables.

.. code-block:: text

   cond_expr      = cond_or ;

   cond_or        = cond_and { "||" cond_and } ;

   cond_and       = cond_unary { "&&" cond_unary } ;

   cond_unary     = "!" cond_unary
                  | cond_primary ;

   cond_primary   = cond_cmp
                  | "(" cond_expr ")" ;

   cond_cmp       = cond_atom [ cmp_op cond_atom ] ;

   cmp_op         = "==" | "!=" ;

   cond_atom      = var_ref
                  | quoted_string        (* reuses parser terminal *)
                  | number_lit           (* reuses parser terminal *)
                  | bool_lit ;           (* reuses parser terminal *)

   var_ref        = "var." ident ;


disambiguation rules
---------------------

the following rules resolve structural ambiguities in the grammar. they are
normative and must be implemented as specified.

**DR-01: bool_switch before bool_action.**

when the parser encounters ``--``, it must attempt to match the ``--enable-``
and ``--disable-`` prefixes before falling back to ``bool_action``. without this
rule, ``--enable-color`` would parse as ``bool_action`` with ident
``enable-color`` instead of ``bool_switch`` with ident ``color``.

implementation: after consuming ``--``, check if the next bytes are ``enable-``
or ``disable-``. if so, consume the prefix and parse the remainder as
``flag_ident`` for a ``bool_switch``. otherwise, parse the full token as
``flag_ident`` for a ``bool_action``.

**DR-02: scalar alternative ordering.**

when parsing a ``scalar`` (or ``kvp_scalar``), alternatives are tried in
precedence order:

1. ``quoted_string`` -- lookahead ``"``
2. ``bool_lit`` -- lookahead ``t`` (true) or ``f`` (false), full match required
3. ``number_lit`` -- lookahead ``-`` or digit
4. ``bare_token`` / ``kvp_bare_token`` -- fallback

the string ``"true"`` (unquoted) resolves to ``bool_lit``. the string
``"trueblue"`` resolves to ``bare_token`` because ``bool_lit`` requires exact
match. type coercion from ``bare_token`` to typed values happens in the
semantic validation phase via the ``argspec`` registry.

**DR-03: bare_token exclusion set.**

``bare_token`` (top-level scalar context) excludes six characters:

- ``"`` -- would start a ``quoted_string``
- ``=`` -- flag assignment separator (prevents ``--flag=val=ue`` misparse)
- ``|`` -- kvp pair separator
- ``{`` / ``}`` -- kvp nesting delimiters
- ``!`` -- kvp prefix marker

``kvp_bare_token`` (inside kvp leaf context) has a different exclusion set:

- ``"`` -- would start a ``quoted_string``
- ``:`` -- kvp key-value separator
- ``|`` -- kvp pair separator
- ``{`` / ``}`` -- kvp nesting delimiters

note: ``=`` and ``!`` are allowed inside kvp leaf values because they have no
structural meaning in kvp context.

**DR-04: typed_value kvp lookahead.**

``typed_value = kvp | scalar``. the parser uses single-character lookahead on
``!`` to choose: if the first character after ``=`` is ``!``, parse as ``kvp``.
otherwise, parse as ``scalar``.

**DR-05: condition ``!`` vs kvp ``!``.**

the ``!`` character serves different roles in the two grammars:

- in the argument parser: kvp prefix (``--flag=!key:val``)
- in the condition expression: logical negation (``!var.debug``)

these grammars are never active simultaneously. the argument parser processes
cli tokens; the condition evaluator processes toml string values. no ambiguity
exists at runtime.


semantic validation rules (post-parse)
---------------------------------------

these rules apply after the grammar has produced a parse tree. they are enforced
by the ``argspec`` registry and ``validate.c``.

1. every flag must be registered in the command's ``argspec`` table. unknown
   flags produce ``ux001``.

2. ``valued_flag`` requires the flag to have a declared type. if the flag is
   registered as ``bool_action`` or ``bool_switch``, the ``=value`` form is
   rejected with ``ux002``.

3. ``bool_action`` flags must not receive ``=value``. ``--force=true`` is
   rejected with ``ux002``.

4. ``bool_switch`` polarity conflict: if both ``--enable-x`` and ``--disable-x``
   appear, reject with ``ux004``.

5. repeated bare boolean flags are hard errors (``ux003``).

6. repeated scalar flags are hard errors (``ux003``).

7. type coercion from ``scalar`` to declared type:

   - ``string``: any scalar accepted.
   - ``int``: ``number_lit`` required, reject non-integer tokens (``ux005``).
   - ``bool``: ``bool_lit`` required (``ux005``).
   - ``enum``: exact match against declared ``choices`` (``ux007``).
   - ``path``: ``bare_token`` or ``quoted_string``, optional relative-only
     constraint checked per-command (``ux005``).
   - ``url``: requires explicit scheme ``https://`` or ``http://`` in strict
     mode (``ux005``).
   - ``kvp``: ``kvp`` parse tree required (``ux005``).

8. kvp validation:

   - duplicate keys at the same object depth rejected (``ux006``).
   - missing key, missing value, malformed nesting, unbalanced braces rejected
     (``ux006``).


changes from masterplan draft
------------------------------

the following corrections and clarifications were made during the freeze review:

1. **letter range typo fixed.** draft had ``"a"-"z" | "a"-"z"`` (duplicate
   lowercase). frozen grammar: ``"a"-"z" | "A"-"Z"``.

2. **``any_char`` defined.** draft referenced but never defined this terminal.
   frozen grammar: any unicode codepoint except unescaped NUL.

3. **``visible_char`` defined.** draft referenced but never defined this
   terminal. frozen grammar: any printable non-whitespace unicode codepoint.

4. **``bare_token`` exclusion set formalized.** draft had only a comment
   (``(* no whitespace, no '|', no '{', no '}' *)``). frozen grammar adds
   explicit exclusions for ``"``, ``=``, and ``!`` with rationale (DR-03).
   introduces separate ``kvp_bare_token`` with context-appropriate exclusions.

5. **``kvp_pair`` → ``kvp_value`` renamed from ``kvp_atom``.** the name
   ``kvp_atom`` conflicted conceptually with ``cond_atom`` in the condition
   grammar. renamed to ``kvp_value`` for clarity. introduced ``kvp_scalar``
   to parallel the top-level ``scalar`` with its own ``kvp_bare_token``.

6. **disambiguation rules added.** the draft grammar was ambiguous on
   ``bool_action`` vs ``bool_switch`` ordering and ``scalar`` alternative
   precedence. five explicit disambiguation rules (DR-01 through DR-05) are
   now normative.

7. **condition grammar terminal names unified.** draft condition grammar used
   ``string_lit`` and ``int_lit`` which did not match the parser grammar's
   ``quoted_string`` and ``number_lit``. frozen grammar reuses the parser
   terminal names directly.

8. **``flag_ident`` introduced.** separates flag identifier namespace from
   general ``ident`` (used in kvp keys and condition var refs). both share the
   same character rules for v1 but can diverge independently if needed.

9. **``cmp_op`` extracted.** the condition grammar's inline ``( "==" | "!=" )``
   was extracted into a named production for clarity.

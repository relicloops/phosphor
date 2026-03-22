.. meta::
   :title: exit codes and logging sign-off
   :tags: #neonsignal, #phosphor
   :status: completed
   :updated: 2026-02-14

.. index::
   triple: phase 0; deliverable; exit codes
   single: logging levels
   single: ux diagnostics
   single: error codes

exit codes and logging conventions sign-off
=============================================

this document records the sign-off for phosphor exit codes, logging levels, and
parser diagnostic subcodes. the frozen reference is published at
``tools/phosphor/docs/source/reference/exit-codes-and-logging.rst``.


acceptance criteria verification
---------------------------------

- ✓ every exit code (0-8) has at least one concrete triggering scenario
  documented. most have multiple scenarios covering different commands and
  failure modes.

- ✓ child process exit code mapping rule documented. four conditions covered:
  child exits 0 (pass-through), child not spawned (exit 5), child killed by
  signal (exit 5), child exits non-zero without mapping (exit 1).

- ✓ all seven ux diagnostic subcodes have example flag/value/message triples.
  ux005 has four sub-examples (string pattern, integer, url scheme, relative
  path). ux006 has three sub-examples (duplicate key, missing value, unbalanced
  braces).

- ✓ logging levels have clear boundary rules. each level has a "when to use"
  guideline and concrete examples. boundary between warn and error:
  warn = operation continues in degraded state, error = operation cannot
  continue. boundary between info and debug: info = user-facing progress,
  debug = troubleshooting detail (--verbose).


decisions made during formalization
------------------------------------

1. **stdout vs stderr split.** info-level messages go to stdout; all other
   levels go to stderr. rationale: follows unix convention (cargo, git, cmake),
   allows piping stdout without diagnostic noise.

2. **no user-facing trace flag.** trace is developer-only, activated via
   ``PHOSPHOR_LOG=trace`` environment variable. this keeps the CLI surface
   clean and avoids confusing users with internal parser/allocator noise.

3. **error object structure.** formalized five fields: category, subcode,
   message, context, cause_id. the cause_id field enables error chains
   (e.g., filesystem error wrapping a permission error wrapping an OS errno).

4. **category enum maps 1:1 to exit codes.** eight categories (general, usage,
   config, filesystem, process, validation, internal, signal) map to exit
   codes 1-8. this avoids any ambiguity in implementation: the category
   determines the exit code.

5. **diagnostic message format.** all ux* diagnostics follow a four-line format:
   ``error [uxNNN]: <message>`` / ``flag:`` / ``expected:`` / ``received:``.
   optional ``hint:`` and ``note:`` lines for suggestions and cross-references.

6. **hint: did you mean?** ux001 (unknown flag) includes a "did you mean"
   suggestion line. implementation should use edit distance (Levenshtein) with a
   threshold of 3 to suggest the closest registered flag. if no flag is within
   threshold, omit the hint line.

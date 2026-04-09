# Plan -- args-parser audit fix (2026-04-09T11-28-03Z)

## Context

Codex audit `.claude/codex-audit-fix/2026-04-09T11-28-03Zargs_parser.bugs.audit.md`
identified three bugs in the args-parser/serve pipeline that survived the
`609f8c2` toggle-form fix. All three allow the CLI to silently accept input
that does nothing or produces undefined behavior.

## Finding inventory

1. **[P1] Toggle CLI flags cannot override manifest booleans in opposite direction**
2. **[P2] Toggle specs accept bare `--flag` / `--flag=value` on TOGGLE specs**
3. **[P2] Integer flags use `atoi` with no range validation**

## Fixes (implementation order)

### A. [P2] Reject bare `--flag` / `--flag=value` on TOGGLE specs

- `src/args-parser/validate.c` -- new UX008 check after line 98
- `include/phosphor/args.h` -- add `PH_UX008_TOGGLE_SYNTAX 8`

### B. [P2] Replace `atoi` with `strtoll` + INT range check

- `src/args-parser/validate.c` -- replace `PH_TYPE_INT` case with `strtoll`
  + `INT_MIN..INT_MAX` bounds, remove dead `is_integer_string()` helper
- `src/commands/serve_cmd.c` -- replace `flag_int` with `strtol` defense-in-depth

### C. [P1] Three-state toggle merge

- `src/args-parser/args_helpers.c` -- new `ph_args_toggle_resolve()` helper
- `include/phosphor/args.h` -- declare `ph_args_toggle_resolve`
- `src/commands/serve_cmd.c` -- replace OR-merge with three-state resolve;
  `proxies-check` uses double negation for inverted polarity

## Commit

Single commit:
```
fix(args): three-state toggle merge, reject bare toggle syntax, safe int parse
```

## Verification

- `ninja -C build` -- zero new warnings
- `./build/phosphor version` -- smoke test
- `phosphor serve --debug` -- UX008 error
- `phosphor serve --port=99999999999999` -- type mismatch error
- `phosphor serve --port=2147483648` -- out of range error

## Critical files

### Modified
- `src/args-parser/validate.c`
- `src/args-parser/args_helpers.c`
- `src/commands/serve_cmd.c`
- `include/phosphor/args.h`
- `docs/actual.plan.md` (this plan)
- `docs/current.audit.fix.md` (audit mirror)
- `.claude/codex-audit-fix/2026-04-09T11-28-03Zargs_parser.bugs.audit.md`

### Renamed at end (audit-fix skill)
- `.claude/codex-audit-fix/2026-04-09T11-28-03Zargs_parser.bugs.audit.md`
  -> `...[COMPLETED].md`

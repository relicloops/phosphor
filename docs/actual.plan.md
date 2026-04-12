# Plan -- args-parser audit fix (2026-04-12T07-06-35Z)

## Context

Codex audit `.claude/codex-audit-fix/2026-04-12T07-06-35Zargs_parser.bugs.audit.md`
found three findings. One was already fixed in a prior session. Two are live.

| # | Sev | Finding | Status |
|---|-----|---------|--------|
| 1 | P2 | Nonpositive int flags accepted, silently discarded by serve | Fixed |
| 2 | P2 | `create --template` http:// URLs misclassified as local paths | Fixed |
| 3 | P3 | PH_TYPE_BOOL valued flags never type-checked | Already fixed (prior session) |

## Fixes

### A. Classify http:// in ph_git_is_url so rejection path is reachable

- `src/io/git_fetch.c` -- added `http://` prefix check to `ph_git_is_url()`
- `ph_git_url_parse()` already rejects `http://` with clear diagnostic

### B. Reject nonpositive integer flags in serve

- `src/commands/serve_cmd.c` -- added explicit `t1 <= 0 && ph_args_has_flag`
  checks at all 5 integer merge sites (threads, port, redirect-instances,
  redirect-port, redirect-target-port)
- Returns `PH_ERR_USAGE` with cleanup on rejection

## Commits

1. `fix(git): classify http:// URLs so the rejection path is reachable`
2. `fix(serve): reject nonpositive integer flags instead of silent fallback`

## Critical files

### Modified
- `src/io/git_fetch.c` -- finding 2
- `src/commands/serve_cmd.c` -- finding 1
- `docs/actual.plan.md` -- this plan
- `docs/current.audit.fix.md` -- audit mirror

# Args Parser Audit

Run: 2026-04-12T07:06:35Z

Scope:
- `include/phosphor/args.h`
- `src/args-parser/lexer.c`
- `src/args-parser/parser.c`
- `src/args-parser/validate.c`
- `src/args-parser/args_helpers.c`
- `src/args-parser/spec.c`
- `src/args-parser/kvp.c`
- Cross-checks in `src/commands/phosphor_commands.c`, `src/commands/create_cmd.c`, `src/commands/serve_cmd.c`, and `src/io/git_fetch.c` where argspec behavior is consumed.

Recent args-related fixes reviewed first:
- `f3b2757` `fix(args): three-state toggle merge, reject bare toggle syntax, safe int parse`
- `609f8c2` `fix(args): convert serve toggle flags to PH_FORM_TOGGLE and mark create --template required`
- `dd68429` `feat(args): add var_name field for flag-to-manifest variable mapping`

The findings below exclude the issues already addressed by those commits.

## 1. [P2] Nonpositive integer flags are accepted, then silently discarded by `serve`

Evidence:
- `src/args-parser/validate.c:107-123` validates `PH_TYPE_INT` for integer syntax and `INT_MIN..INT_MAX`, but does not enforce the positive-only semantics required by current serve flags.
- `src/commands/serve_cmd.c:295-308` and `src/commands/serve_cmd.c:439-455` only honor parsed integer values when they are `> 0`; zero and negatives fall back to manifest/default values.

Why this is a bug:
- `phosphor serve --threads=0`, `--port=0`, `--redirect-port=-1`, and `--redirect-target-port=-1` all pass args validation but do not take effect.
- The CLI therefore accepts invalid values for every current integer serve flag and fails open instead of returning a usage/type error.

Impact:
- Scripts and automation can believe they selected a port or thread count while `serve` actually runs with a different manifest/default value.
- This is the same class of issue as the already-fixed bare-toggle acceptance bug: accepted input that is silently ignored at runtime.

Suggested fix:
- Add min/max or per-flag semantic constraints to `ph_argspec_t`, or fail explicitly in `serve_cmd.c` when a parsed integer is `<= 0`.

## 2. [P2] `create --template` is typed as `PH_TYPE_PATH`, so insecure or malformed URL rejection is bypassed

Evidence:
- `src/commands/phosphor_commands.c:25-26` documents `--template` as "local path, git URL, or archive" but registers it as `PH_TYPE_PATH`.
- `src/args-parser/validate.c:138-149` only applies path-traversal checks to that type.
- `src/io/git_fetch.c:27-30` only classifies `https://...` as a git URL, while `src/io/git_fetch.c:44-55` contains the explicit `http://` rejection path.
- `src/commands/create_cmd.c:157-185` only enters URL parsing if `ph_git_is_url()` succeeds; otherwise `src/commands/create_cmd.c:231-242` drops into local-path resolution.

Why this is a bug:
- `--template=http://example.com/repo` is not rejected as an insecure remote URL.
- Instead, it is misclassified as a local path and produces a misleading filesystem error path.
- The same class of problem applies to other URL-like values that should fail URL validation instead of path resolution.

Impact:
- The documented/implemented URL defense in `ph_git_url_parse()` is only partially reachable from the CLI.
- Users get the wrong failure mode, and insecure `http://` inputs are not rejected at the args/spec layer.

Suggested fix:
- Give `template` a dedicated source-kind validator, or add command-specific prevalidation that classifies and rejects unsupported URL schemes before path resolution.

## 3. [P3] `PH_TYPE_BOOL` valued flags are never type-checked

Evidence:
- `include/phosphor/args.h:82-85` exposes `PH_TYPE_BOOL` as a first-class arg type.
- `src/args-parser/validate.c:180-182` performs no validation at all for valued `PH_TYPE_BOOL` flags.

Why this is a bug:
- Any future argspec declared as `PH_FORM_VALUED` + `PH_TYPE_BOOL` would accept arbitrary non-empty strings such as `--flag=maybe` or `--flag=123`.
- That violates the generic type contract advertised by the args layer.

Impact:
- This is latent today because current command tables only use `PH_TYPE_BOOL` for action/toggle flags, but the generic validator is still incorrect.
- The gap is also untested in the archived unit coverage.

Suggested fix:
- Enforce `true|false` in `validate.c` for valued bool flags and add dedicated unit coverage.

## Notes

- I did not find new ownership or cleanup bugs in the lexer/parser destroy paths.
- KVP depth limiting and duplicate-key rejection look consistent with the current test corpus.
- No builds or tests were run for this audit.

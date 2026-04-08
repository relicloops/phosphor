# Codex Audit Remediation Plan

## Context

Three independent static audits were run by OpenAI Codex on 2026-04-06. All produced overlapping findings. All findings have been verified as **still present** in the current code (commit 681daa0). The core risk: manifest-controlled paths can escape intended filesystem boundaries, and ACME certificate flows have correctness/injection issues.

## Audit File Status

| File | Timestamp | Findings | Verdict |
|------|-----------|----------|---------|
| `.claude/2026-04-06_08-10-55_GMT.audit.md` | 08:10 GMT | 6 findings | Real, verified |
| `.claude/2026-04-06T09-06-58Z.audit.md` | 09:06 UTC | 6 findings | Real, verified |
| `.claude/2026-04-06T10-04-24Z.audit.md` | 10:04 UTC | 5 findings | Real, verified |

All are read-only static audits. Findings overlap ~85%. Combined, 8 distinct issues across 4 severity tiers.

---

## Phase 0: Save plan to docs

Save the final remediation plan as `docs/current.audit.fix.md` for project tracking, and to `docs/actual.plan.md` per CLAUDE.md convention.

---

## Phase 1: Path Hardening Primitives (foundation -- must land first)

All path-escape fixes depend on these new utilities in `src/io/path_norm.c` + `include/phosphor/path.h`.

### 1A. Add `ph_path_resolve()`
- Wraps POSIX `realpath(3)` with `ph_alloc`/`ph_free` contract
- Fully resolves `..`, `.`, and symlinks
- Fallback for nonexistent tails: resolve longest existing prefix, reject traversal in remainder

### 1B. Add `ph_path_is_under()`
- Predicate: "does `child` stay within `root` after full canonicalization?"
- Calls `ph_path_resolve()` on both args, then prefix-checks resolved paths
- Replaces naive `deploy_at_escapes_root()` string-prefix check

### 1C. Add `ph_path_safe_join()`
- Like `ph_path_join()` but rejects absolute `rel` and `..` traversal
- Returns NULL + sets `ph_error_t` on violation
- Used wherever untrusted input (manifest ops, deploy paths) feeds into joins

**Files:** `src/io/path_norm.c`, `include/phosphor/path.h`

---

## Phase 2: Path Traversal Fixes (HIGH -- Findings 1, 2)

Depends on Phase 1.

### 2A. Change build path flags from `PH_TYPE_STRING` to `PH_TYPE_PATH`
- In `src/commands/phosphor_commands.c:65-68`, `--project` and `--deploy-at` are declared as `PH_TYPE_STRING`
- The generic validator at `src/args-parser/validate.c:108-118` only runs path-traversal checks for `PH_TYPE_PATH`
- Change both to `PH_TYPE_PATH` so the args-parser rejects `..` traversal at the validation layer

### 2B. Reject unsafe manifest ops at parse time
- In `src/template/manifest_load.c:467-468`, after parsing `ops[i].from` / `ops[i].to`
- Reject if `ph_path_is_absolute()` or `ph_path_has_traversal()` returns true
- This is the parse-time gate -- catches malicious manifests early

### 2C. Validate `template.source_root` at parse time
- In `src/template/manifest_load.c:134-156`, `source_root` is loaded verbatim
- Reject if absolute or contains traversal
- This is the other parse-time gate for the template trust boundary

### 2D. Validate resolved planner paths against roots
- In `src/template/planner.c:156-170`, after joining `from_abs` / `to_abs`
- Call `ph_path_is_under(from_abs, template_root)` and `ph_path_is_under(to_abs, dest_dir)`
- Catches variable-substitution-injected traversal (e.g., `<<name>>` = `../../escape`)
- Also covers `glow_cmd.c` which shares the same planner/writer pipeline

### 2E. Fix build deploy path escape
- In `src/commands/build_cmd.c:122-136`, replace `deploy_at_escapes_root()` with `ph_path_is_under()`
- In `src/commands/build_cmd.c:470-482`, add same validation for manifest `[deploy].public_dir` (currently unchecked)

### 2F. Add containment check in writer.c remove operation
- In `src/template/writer.c:603-614`, `PH_OP_REMOVE` calls `ph_fs_rmtree()` on resolved paths without containment
- Add `ph_path_is_under()` check before any `ph_fs_rmtree()` call in the executor

**Files:** `src/commands/phosphor_commands.c`, `src/template/manifest_load.c`, `src/template/planner.c`, `src/commands/build_cmd.c`, `src/template/writer.c`

---

## Phase 3: ACME Fixes (HIGH/LOW -- Findings 3, 4, 7)

Independent of Phase 2. Can be developed in parallel.

### 3A. Eliminate shell injection in JWK extraction
- In `src/certs/acme_jws.c:258-284` and `:384-411`
- Replace `sh -c "openssl ... '<key_path>' ..."` with direct `execvp` via argv
- Reference: `ph_acme_jws_sign()` at lines 130-148 already does this correctly
- Add static helper `exec_to_file(argv, stdout_path)` using fork/dup2/execvp if `ph_proc_exec` lacks stdout redirection
- Also audit rest of `src/certs/acme_jws.c` for any remaining shell-based helpers

### 3B. Fix challenge timeout returning success
- In `src/certs/acme_challenge.c:232-289`
- Add `bool validated = false;` before loop, set true on `"valid"` break
- After loop: if `!validated`, return `PH_ERR` with timeout error (not `PH_OK`)
- Include last observed status in error message for diagnosability
- The unconditional `return PH_OK` at line 289 is the bug

### 3C. Replace busy-wait with `nanosleep`
- In `src/certs/acme_challenge.c:245-249` and `src/certs/acme_finalize.c:235-238`
- Replace `while (ph_clock_elapsed_ms(...) < 2000.0) ;` with `nanosleep(&(struct timespec){2,0}, NULL)`

**Files:** `src/certs/acme_jws.c`, `src/certs/acme_challenge.c`, `src/certs/acme_finalize.c`

---

## Phase 4: Temp Dirs + Serve CWD (MEDIUM -- Findings 5, 6)

Independent of other phases.

### 4A. Replace predictable temp paths with `mkdtemp`
Reference pattern already in codebase: `src/commands/glow_cmd.c:131-135`

| File | Current pattern | Fix |
|------|----------------|-----|
| `src/io/archive.c:164-174` | `/tmp/.phosphor-extract-<pid>-<time>` + `ph_fs_mkdir_p()` | `mkdtemp()` |
| `src/io/git_fetch.c:210-220` | `/tmp/.phosphor-clone-<pid>-<time>` + `ph_fs_mkdir_p()` | `mkdtemp()` |
| `src/template/staging.c:46-68` | `<parent>/.phosphor-staging-<pid>-<time>` + `ph_fs_mkdir_p()` | `mkdtemp()` (heap-allocated template since parent varies) |

### 4B. Default serve child cwd to project root
- In `src/commands/serve_cmd.c`, after building cfg struct
- If `cfg.ns.working_dir` is NULL, default to `project_root_abs`
- Same for `cfg.redir.working_dir`
- Only affects the default; explicit CLI/manifest `working_dir` still wins

**Files:** `src/io/archive.c`, `src/io/git_fetch.c`, `src/template/staging.c`, `src/commands/serve_cmd.c`

---

## Phase 5: Documentation Drift (LOW -- Finding 8)

Independent of all other phases.

### 5A. Fix README phantom Meson options
- `README.md` documents `-Dlibgit2`, `-Dlibarchive`, `-Dpcre2`, `-Dlibcurl` toggles
- `meson.options` only has `script_fallback` and `dashboard`
- `meson.build:140-273` configures all four deps unconditionally
- Update README to reflect reality: all deps are always built from vendored subprojects

### 5B. Update project-audit.rst
- Still references "531 tests", "5-job CI", version `0.0.1-021`
- Current state: 0 active tests, 3 CI jobs (`build`, `release-build`, `release`)
- Update to reflect current state

**Files:** `README.md`, `docs/source/reference/project-audit.rst`

---

## Implementation Order

```
Phase 0 (save plan to docs)
Phase 1 (path primitives) --> Phase 2 (path fixes)
                          \
Phase 3 (ACME)             > can run in parallel
Phase 4 (temp dirs + serve)/
Phase 5 (docs)            /
```

## Verification

1. **Build**: `meson setup build && ninja -C build` -- must compile clean
2. **Smoke**: `./build/phosphor version` -- binary runs
3. **Path hardening**: craft a malicious `template.phosphor.toml` with `ops[0].to = "../../escape"` and verify `phosphor create` rejects it
4. **Args layer**: verify `--deploy-at=../../outside` is rejected by args-parser before reaching build_cmd
5. **ACME timeout**: inspect code path -- after loop exhaustion, `PH_ERR` is returned (not `PH_OK`)
6. **Temp dirs**: verify `mkdtemp` pattern in archive/git_fetch/staging via code inspection
7. **Docs**: verify README no longer documents nonexistent Meson options

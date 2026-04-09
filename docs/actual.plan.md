# Plan ▸ consolidated audit fix for 4 active bugs audits

## Context

Codex generated four static-source bugs audits in the past 24 hours, all
currently marked `.[ACTIVE ▸].md` in `.claude/codex-audit-fix/`:

- `2026-04-08T20-06-32Z.bugs.audit` (audit A, 5 findings)
- `2026-04-08T23-07-16Z.bugs.audit` (audit B, 5 findings)
- `2026-04-09T02-08-22Z.bugs.audit` (audit C, 5 findings)
- `2026-04-09T05-09-51Z.bugs.audit` (audit D, 5 findings)

The four audits overlap heavily. 20 raw finding slots collapse to **14
distinct issues**. Five issues are persistent (reported by 2-3 audits each),
which indicates the underlying defects never got fixed and Codex keeps
re-discovering them.

Rather than fixing the same defect across four separate audit-fix passes,
this plan addresses every distinct finding in a single consolidated run.
The source-of-truth mirror in `docs/current.audit.fix.md` will be a
byte-for-byte copy of the **newest** audit (D, `2026-04-09T05-09-51Z`), and
all four audits will be renamed from `.[ACTIVE ▸].md` to `.[COMPLETED ✓].md`
once every finding is addressed. The older three audits each contain
findings not present in D; those are folded into this plan by explicit
cross-reference, not lost.

Intended outcome: every documented feature that the README promises either
works or is removed from the README, every legacy / deprecated code path
that bypasses hardening either runs the same checks as the native path or
is compile-gated shut, every secret variable stays masked in logs, and the
recursive template walkers inherit the containment hardening the single-file
branches already have.

## Finding inventory

Each finding is tagged with the audit IDs that reported it so the rename
step at the end can be justified across all four sources.

### Persistent findings (reported by 2 or 3 audits)

1. `[variables]` config table never loaded `[A1 / B2 / C2]` — High
2. `--legacy-scripts` runtime-reachable and bypasses deploy containment
   `[A2 / B3 / D2]` — High
3. esbuild `--define` values not JS-escaped `[A4 / C4]` — Medium
4. Recursive `copy` / `render` writes lack per-child containment re-check
   `[B4 / D5]` — Medium
5. `manifest.toml` filename not probed everywhere README promises `[B5 / C1]`
   — Medium

### Unique to a single audit

6. Single-file `copy` / `render` lose source mode `[A3]` — Medium
7. Cert generation ignores filesystem hardening failures `[A5]` — Low/Medium
8. `create` / `glow` leak secret template variables in verbose mode `[B1]`
   — High
9. Archive templates don't descend into a single top-level wrapper dir
   `[B5 second half]` — Medium
10. `ph_serve_wait()` doesn't honor documented signal exit contract `[C3]`
    — Medium
11. `doctor` reports misleading global esbuild warning `[C5]` — Low
12. Legacy `clean` mode ignores `--dry-run` and `--wipe` `[D1]` — High
13. `doctor` and `rm` treat `phosphor.toml` as manifest `[D3]` — Medium
14. `serve` HTTPS URL banner omits the active port `[D4]` — Medium

## Fixes

### Finding 1 (High) ▸ `[variables]` config table never loaded

**Problem**
`README.md:54-79` documents `.phosphor.toml` / `phosphor.toml` with a
`[variables]` sub-table that feeds template variable resolution.
`src/core/config.c:43-85` (`parse_config_toml`) only iterates root-level
TOML scalars and never descends into `variables`. `src/template/var_merge.c:174`
then looks up `def->name` directly via `ph_config_get`, so nested entries
stay invisible. The documented precedence chain is a lie for the documented
config shape.

**Fix**
Extend `parse_config_toml` to descend into the `[variables]` sub-table
after the root-scalar pass, using the same pattern `parse_defaults` already
uses in `src/template/manifest_load.c:172-196`: `toml_table_table(root,
"variables")` followed by key-by-key `toml_table_string/bool/int` iteration.
Flatten each entry into `ph_config_t` via `dup_str` (existing helper at
`src/core/config.c:12-17`). Entries from `[variables]` should follow
root-level scalars so they override earlier root entries on name collision
(matches README precedence wording).

`ph_config_t` is a flat `(keys[], values[], count)` map, so flattening is
type-preserving. No signature changes to `ph_config_get` or `var_merge.c`.
The only call site, `src/template/var_merge.c:174`, inherits the new
entries automatically.

---

### Finding 2 (High) ▸ `--legacy-scripts` bypass (persistent)

**Problem**
`meson.options:1-2` says `script_fallback` is disabled by default, and
`README.md:163-166` says the legacy fallback is available only when the
binary is compiled with `-Dscript_fallback=true`. But
`src/commands/build_cmd.c:271-282` and the identical copy in
`src/commands/clean_cmd.c:165-174` define `use_legacy_scripts()` as a
runtime-flag check that does NOT consult `PHOSPHOR_SCRIPT_FALLBACK`. The
legacy dispatch at `build_cmd.c:345-370` then calls `build_via_scripts`
without running `deploy_at_escapes_root`, reopening the write-outside-project
risk that the native path already fixed.

**Fix**
1. Hard-gate `use_legacy_scripts()` in both `build_cmd.c:271-282` and
   `clean_cmd.c:165-174`: when `PHOSPHOR_SCRIPT_FALLBACK` is not defined,
   `--legacy-scripts` is rejected with a `PH_ERR_USAGE`-style error at
   the top of `ph_cmd_build` / `ph_cmd_clean`, before any work runs. The
   error message should name the meson option.
2. In `ph_cmd_build`'s legacy branch (currently `src/commands/build_cmd.c:345-370`),
   after `legacy_deploy` is resolved (lines 351-362), insert the same
   `deploy_at_escapes_root(legacy_deploy, project_root_abs)` containment
   call that the native path uses at `src/commands/build_cmd.c:448-457`.
   On escape, free everything and return `PH_ERR_VALIDATE`.

The two copies of `use_legacy_scripts()` will be refactored into a single
inline check in both commands rather than a shared helper — the function
is 8 lines and the dependency graph doesn't justify adding a new header.
Finding 12 below depends on the same gate.

---

### Finding 3 (Medium) ▸ esbuild `--define` values not JS-escaped

**Problem**
`src/commands/build_cmd.c:743-788` has two loops that push `--define:NAME="VALUE"`
arguments via `ph_argv_pushf`. Neither escapes `"`, `\`, newlines, or
control characters in `VALUE`. Environment-provided site metadata or
manifest defaults with a `"` char produce malformed JS literals and break
otherwise-valid builds.

**Fix**
Add a small static helper in `build_cmd.c`, `escape_define_value(const
char *in)`, that returns a heap-allocated JSON-style escaped string:

- `"` → `\"`
- `\` → `\\`
- `\n` → `\n` (literal `\` `n` — two chars)
- `\r` → `\r`
- `\t` → `\t`
- control chars `0x00-0x1F` that aren't the above → `\u00XX`
- everything else copied verbatim

No cJSON exposure here — `src/core/json.c` only has parse/read/write, not
a public string-escape entry point, and adding one just for this call site
is disproportionate. The helper can stay file-local in `build_cmd.c`.

Apply at both sites in the two `--define` loops (manifest-driven and the
legacy compatibility set). Free the returned string after `ph_argv_pushf`.

---

### Finding 4 (Medium) ▸ recursive copy/render per-child containment

**Problem**
Single-file `PH_OP_COPY` and `PH_OP_RENDER` got execute-time containment
re-checks in the prior audit fix (`src/template/writer.c:644-651, 735-742`).
The recursive branches still delegate to `ph_fs_copytree` and the
file-local `rendertree_recurse`, both of which walk descendants and write
via `ph_fs_write_file` (which `open`s with `O_WRONLY | O_CREAT | O_TRUNC`,
no `O_NOFOLLOW`, at `src/platform/posix/fs_posix.c:81`). A same-user
concurrent symlink swap inside a deploy / staging tree can redirect
descendant writes outside the approved root after the one-time top-level
containment check passed.

**Fix**
Per-child canonical re-check inside both walkers, anchored to the same
`plan->dest_dir` the single-file branches use. No `O_NOFOLLOW` rewrite of
`ph_fs_write_file` — that helper has other callers with legitimate
symlink following, and broadening its semantics is out of scope.

1. Add `const char *dest_dir` to `rendertree_ctx_t` in
   `src/template/writer.c` near its definition. Populate it in the single
   caller at `src/template/writer.c:717` (`PH_OP_RENDER` directory branch)
   with `plan->dest_dir`.
2. In `rendertree_recurse` right before the `ph_fs_write_file(dst_child,
   …)` call at `src/template/writer.c:419`, insert
   `if (rctx->dest_dir && !ph_path_is_under(dst_child, rctx->dest_dir))
   goto cleanup`, mirroring the chmod/remove pattern. Set `err` with a
   descriptive message naming the op and target.
3. For `ph_fs_copytree`, add an optional `const char *root` parameter
   (or overload via a wrapper). Pass `plan->dest_dir` from the directory
   branch of `PH_OP_COPY` at `src/template/writer.c:621`. Inside
   `copytree_recurse` (`src/io/fs_copytree.c:13`), re-check every
   `dst_child` against `root` before `ph_fs_write_file` at line 122.
4. Update the two other callers of `ph_fs_copytree` to pass `NULL` for
   the new `root` parameter unless they have a meaningful anchor:
   - `src/template/staging.c` — EXDEV fallback for staging commits. The
     staging tree is already a private `0700` mkdtemp root; pass the
     staging root as the containment anchor for defense-in-depth.
   - Any call site in `embedded_registry.c` if it exists — pass NULL
     (embedded asset expansion is compiled-in data, not user-supplied).

**Risk**: `ph_fs_copytree` signature change ripples into three callers.
Both callers can pass `NULL` initially and be migrated afterwards without
changing behavior, so the change is additive.

---

### Finding 5 (Medium) ▸ `manifest.toml` probing gap

**Problem**
`README.md:193-213` lists both `template.phosphor.toml` and `manifest.toml`
as valid manifest filenames, and
`docs/source/reference/template-manifest-schema.rst` echoes that. But the
five commands that actually load a manifest only probe
`template.phosphor.toml`:

- `src/commands/create_cmd.c:275`
- `src/commands/glow_cmd.c:145`
- `src/commands/build_cmd.c:377-378`
- `src/commands/serve_cmd.c:198-199`
- `src/commands/certs_cmd.c:601-602`

`include/phosphor/manifest.h:251` header doc also hardcodes the
`template.phosphor.toml` name in its description.

**Fix**
Add a shared helper `ph_manifest_find` that encapsulates the probe and
returns the first filename that exists:

- Declared in `include/phosphor/manifest.h` near `ph_manifest_load`.
- Implemented in `src/template/manifest_load.c`.
- Signature: `char *ph_manifest_find(const char *project_root);` —
  returns a heap-allocated absolute path or `NULL` if neither file is
  present. No `ph_error_t **` needed; callers already log their own
  "manifest not found" errors.
- Precedence: `template.phosphor.toml` first (for compatibility with
  every existing shipped template), then `manifest.toml`.

Update all five call sites to use `ph_manifest_find` instead of
hand-rolling the `ph_path_join` + `ph_fs_stat` dance. Update the
`ph_manifest_load` docstring in `include/phosphor/manifest.h:251` to note
both filenames are supported.

---

### Finding 6 (Medium) ▸ single-file copy/render lose source mode

**Problem**
`src/template/writer.c` `PH_OP_COPY` single-file branch reads `ph_fs_stat`
into `st` at line 608 and then writes bytes at line 672 (non-atomic) or
line 667 (atomic via `ph_fs_atomic_write`), never reapplying `st.mode`.
The `PH_OP_RENDER` single-file branch has the same gap at lines 697, 793,
799. The recursive paths preserve mode at `src/io/fs_copytree.c:135`
(`ph_fs_chmod(dst_child, st.mode & 07777)`) and
`src/template/writer.c:432` for rendertree — the single-file branches
diverged.

Atomic writes via `ph_fs_atomic_write` are especially visible: `mkstemp`
yields 0600 so the file ends up more restrictive than the source.

**Fix**
1. In the `PH_OP_COPY` single-file branch, stash `mode_t saved_mode =
   st.mode & 07777;` right after the `ph_fs_stat` call. After the
   successful write (both the atomic and non-atomic `else` branches),
   call `ph_fs_chmod(op->to_abs, saved_mode)` and route failure through
   the same `cleanup_err` path the existing write-error case uses.
2. Same fix pattern in the `PH_OP_RENDER` single-file branch.
3. `ph_fs_atomic_write` itself doesn't need a signature change. Applying
   chmod after the atomic write in writer.c works because rename-over is
   already done by the time we chmod, so the chmod targets the final
   inode.

No changes to `fs_atomic.c`. The EXDEV fallback inside `ph_fs_atomic_write`
calls `ph_fs_write_file` and does not preserve mode, but that concern is
narrower than the template-ops audit, which is satisfied once writer.c
chmod's after atomic writes complete.

---

### Finding 7 (Low/Medium) ▸ cert generation fs hardening failures ignored

**Problem**
Five cert sites drop the return value of `ph_fs_chmod` or `ph_fs_mkdir_p`:

- `src/certs/certs_ca.c:149-150` — `ph_fs_chmod(key_path, 0600)` unchecked
- `src/certs/certs_leaf.c:148-149` — `ph_fs_chmod(privkey_path, 0600)` unchecked
- `src/certs/acme_finalize.c:65` — `ph_fs_chmod(privkey_path, 0600)` unchecked
- `src/certs/acme_finalize.c:291-292` — `ph_fs_chmod(cert_path, 0644)` unchecked
- `src/commands/certs_cmd.c` (post earlier edits, around the old line
  396-399 area) — `ph_fs_mkdir_p(domain_dir, 0755)` unchecked

Each can degrade into a confusing downstream OpenSSL or process error
instead of a direct `PH_ERR_FS`.

**Fix**
Treat each as fatal: check the return value, set `*err` via
`ph_error_createf(PH_ERR_FS, …)` with the path in the message, and
propagate via the existing `goto fail` / `return PH_ERR` idiom each file
already uses. `certs_cmd.c`'s `ph_fs_mkdir_p` failure should set
`exit_code` and break out of the LE loop instead of silently falling
through to the next ACME step.

---

### Finding 8 (High) ▸ `create` / `glow` leak secret variables in verbose mode

**Problem**
`src/template/var_merge.c:202-206` masks secret variables as `[secret]`
during merge logging. But `src/commands/create_cmd.c:359-360` and
`src/commands/glow_cmd.c:200-202` both re-log the merged variable list
via `ph_log_debug("…  %s = %s", vars[i].name, vars[i].value)` without
masking. Any variable marked `secret = true` in the manifest leaks when
`--verbose` is on.

There's a second undefined-behavior bug nested inside: `vars[i].value`
can be `NULL` for optional variables that never got a value
(`src/template/var_merge.c:191` only assigns under `if (value)`), and
passing `NULL` to `%s` is undefined.

**Fix**
1. Add `bool secret;` to `ph_resolved_var_t` in
   `include/phosphor/template.h:17-21` so consumers can mask without
   re-looking-up the def.
2. In `ph_var_merge` (`src/template/var_merge.c:145`), copy the secret
   flag from the matching `ph_var_def_t` into the resolved entry at the
   same point the name/value get populated.
3. In `src/commands/create_cmd.c:359-360` and
   `src/commands/glow_cmd.c:200-202`, change the debug log to mirror the
   masking pattern in `var_merge.c:202-206`:
   ```c
   const char *shown = vars[i].secret
       ? "[secret]"
       : (vars[i].value ? vars[i].value : "(null)");
   ph_log_debug("create:   %s = %s", vars[i].name, shown);
   ```
   (Same for glow.)
4. The `(null)` guard closes the separate UB bug for optional unset vars.

**Risk**: Adding a field to `ph_resolved_var_t` affects every consumer
that iterates the struct. `ph_var_merge` populates it; `create_cmd`,
`glow_cmd`, `plan_build`, and `plan_execute` read it. None of those
consumers initialize `ph_resolved_var_t` values themselves — they all
receive a merged slice — so the field is forward-compatible.

---

### Finding 9 (Medium) ▸ archive single top-dir descent

**Problem**
Common archives unpack to `project-name/template.phosphor.toml` — one
wrapping directory containing everything. `src/io/archive.c:312-321`
(`ph_archive_extract`) returns the `mkdtemp` root directly.
`src/commands/create_cmd.c:211-275` stores that root in `template_abs`
and immediately probes `template_abs/template.phosphor.toml`. If the
manifest sits one level deeper, the probe fails.

**Fix**
Fix in `create_cmd.c`, not `archive.c`. Keeping `ph_archive_extract`'s
contract stable (returns the exact extraction root) is friendlier to
future callers that want raw access.

After `template_abs` is set and before the manifest probe (around line
274), add a small helper `resolve_template_root` that:

1. Calls `ph_manifest_find(template_abs)` (the new helper from finding 5).
2. If that returns non-NULL, use `template_abs` as-is.
3. Otherwise, open `template_abs` via `opendir` / `readdir`, skip `.` and
   `..` and anything matching `.phosphor-*` prefix. Count visible
   entries.
4. If exactly one visible entry exists and `ph_fs_stat` says it's a
   directory, join it to `template_abs`, then re-run
   `ph_manifest_find` on the descended path. If the manifest exists
   there, replace `template_abs` with the descended path (free the old
   string). Otherwise leave `template_abs` alone and let the existing
   "missing manifest" error path fire with the accurate original path.

No new public API. The descend logic stays private to `create_cmd.c`.
`readdir` iteration uses the same pattern the existing `fs_copytree.c`
walker uses at `src/io/fs_copytree.c:25-54`.

---

### Finding 10 (Medium) ▸ `ph_serve_wait` signal exit contract

**Problem**
`include/phosphor/serve.h:85-89` documents that `ph_serve_wait` returns
`PH_ERR_SIGNAL` (8) when interrupted. `src/serve/serve.c:455-521` tracks
`worst_exit` across children and returns it. The `EINTR` branch at
`src/serve/serve.c:462-471` forwards `SIGTERM` to the child group and
loops with `continue` — it never records that a signal arrived.

Result: Ctrl+C during `phosphor serve` returns `128 + SIGTERM = 143` (or
whichever child signal exit code happens first), not 8. Shell wrappers
that key off phosphor's 0-8 error taxonomy misclassify interrupted
serves.

**Fix**
Introduce a local `bool signaled = false;` near the top of
`ph_serve_wait`. In the `EINTR` branch (and alongside any
`ph_signal_interrupted()` check), set `signaled = true;` without
touching `worst_exit`. At the return site (currently
`src/serve/serve.c:521`), return `signaled ? PH_ERR_SIGNAL : worst_exit`.

This keeps child exit codes visible for non-signal termination while
honoring the documented contract for signals. The `SIGTERM` forward to
children remains unchanged.

---

### Finding 11 (Low) ▸ `doctor` misleading global esbuild warning

**Problem**
`src/commands/doctor_cmd.c:55-80` runs `which esbuild` against the global
PATH and emits `[warn]` when it's missing.
`src/commands/build_cmd.c:626-705` actually uses
`node_modules/.bin/esbuild` inside the project and auto-runs
`npm install` when the local binary is missing. So the doctor warning
fires on correctly-configured projects and is silent on projects that
are actually broken.

**Fix**
Add a project-local check first. In `doctor_cmd.c`, before the
PATH-based check, try `ph_fs_stat(project_root_abs +
"/node_modules/.bin/esbuild")`. If it exists and is a file, report `[ok]`.
Otherwise fall back to the PATH check but label the warning as optional
(the binary is no longer a hard prerequisite because `build` will
`npm install` it). The call site at `doctor_cmd.c:320` switches to the
new check.

Minor scope — single-file, single-function change.

---

### Finding 12 (High) ▸ legacy `clean` ignores `--dry-run` and `--wipe`

**Problem**
`src/commands/clean_cmd.c:94-161` `clean_via_scripts(const char
*project_root_abs)` accepts only the project root and always runs
`scripts/_default/clean.sh` with no flags. The legacy dispatch at
`clean_cmd.c:235-243` picks it up whenever `--legacy-scripts` is on and
`--stale` is off. The flags never reach the script.

**Fix**
1. Extend `clean_via_scripts` signature to
   `clean_via_scripts(const char *project_root_abs, bool dry_run, bool wipe)`.
2. Inside the argv build block (lines 108-115), push `"--dry-run"` and
   `"--wipe"` conditionally after the script name.
3. Update the single call site at `clean_cmd.c:240` to pass `dry_run` and
   `wipe`.
4. The compile-gate on `--legacy-scripts` from finding 2 applies here
   too — a binary compiled without `PHOSPHOR_SCRIPT_FALLBACK` will reject
   the flag outright before `clean_via_scripts` is even reached.

The shipped `scripts/_default/clean.sh` doesn't live in this repo
(downstream projects provide their own); a script that doesn't recognize
`--dry-run` / `--wipe` will error, which is strictly better than
silently ignoring them.

---

### Finding 13 (Medium) ▸ `doctor` / `rm` treat `phosphor.toml` as manifest

**Problem**
`README.md:54-79` documents `phosphor.toml` as walk-up project config
(feeding template variable resolution). `src/commands/doctor_cmd.c:29-50`
(`check_manifest`) probes both `template.phosphor.toml` and
`phosphor.toml` and reports the latter as a valid phosphor manifest.
`src/commands/rm_cmd.c:66-90` drops its manifest safety check if
`phosphor.toml` is present. Meanwhile `build`, `serve`, `certs` only
consume `template.phosphor.toml`, so a directory with only
`phosphor.toml` passes `doctor` / `rm` gates but fails every actual
command.

**Fix**
Remove `phosphor.toml` from the probe list in both files:

1. `doctor_cmd.c:29-50` — delete the second `path2` stat branch (lines
   31, 40-44 per the exploration report) so only
   `template.phosphor.toml` is accepted.
2. `rm_cmd.c:66-90` — same surgery (lines 70-71, 78-80).

After finding 5 lands, these call sites should use `ph_manifest_find`
for consistency. That's the end state: both `doctor` and `rm` call
`ph_manifest_find`, which probes `template.phosphor.toml` first and
`manifest.toml` second, and never `phosphor.toml`.

---

### Finding 14 (Medium) ▸ `serve` HTTPS URL banner omits port

**Problem**
`src/commands/serve_cmd.c:625-628` constructs the startup URL as
`https://<host>` without a port. The effective port lives in a separate
variable (`ns_port_val`, defaulting to 9443 at line 618) and is only
used for a separate `port_buf` formatting. Lines 642, 684, 694 render
the portless URL in the banner and the ncurses dashboard info panel.

**Fix**
Change the `snprintf` at `serve_cmd.c:626` to include the port:

```c
snprintf(url_buf, sizeof(url_buf), "https://%s:%d", url_host,
         ns_port_val);
```

All downstream uses inherit the corrected string. The existing 256-byte
`url_buf` at line 625 is ample for `https://` + max hostname + `:` +
5-digit port. No dashboard-side changes — it already consumes `url_buf`.

---

## Execution order

Ordered so that earlier fixes unblock / de-risk later ones:

1. Finding 5 (manifest.toml probe helper) — new public API everyone else
   can use.
2. Finding 13 (`doctor` / `rm` stop conflating `phosphor.toml` with
   manifest) — now rides on the helper.
3. Finding 1 (`[variables]` config parse) — independent, unblocks the
   persistent high.
4. Finding 8 (secret var masking + null guard) — adds a field to
   `ph_resolved_var_t`; do early so later refactors see the new field.
5. Finding 2 + 12 combined (`--legacy-scripts` gate + clean flag
   forwarding) — one commit covering both legacy paths.
6. Finding 3 (esbuild define escaping) — independent `build_cmd.c`
   change.
7. Finding 4 (recursive per-child containment) — `ph_fs_copytree`
   signature change ripples; do after smaller fixes.
8. Finding 6 (single-file mode preservation) — narrow, `writer.c` only.
9. Finding 7 (cert fs hardening propagation) — five small sites, one
   commit.
10. Finding 9 (archive single-top-dir descent) — depends on finding 5's
    helper.
11. Finding 10 + 14 combined (`ph_serve_wait` signal exit + URL port) —
    both in `serve` subsystem.
12. Finding 11 (`doctor` project-local esbuild) — last, trivial scope.

## Critical files

### Modified
- `src/core/config.c` ▸ finding 1
- `src/template/manifest_load.c` ▸ finding 5
- `include/phosphor/manifest.h` ▸ finding 5 (declaration + docstring)
- `src/commands/doctor_cmd.c` ▸ findings 11, 13
- `src/commands/rm_cmd.c` ▸ finding 13
- `src/commands/create_cmd.c` ▸ findings 5, 8, 9
- `src/commands/glow_cmd.c` ▸ findings 5, 8
- `src/commands/build_cmd.c` ▸ findings 2, 3, 5
- `src/commands/clean_cmd.c` ▸ findings 2, 12
- `src/commands/serve_cmd.c` ▸ findings 5, 14
- `src/commands/certs_cmd.c` ▸ findings 5, 7
- `include/phosphor/template.h` ▸ finding 8 (new `secret` field)
- `src/template/var_merge.c` ▸ finding 8
- `src/template/writer.c` ▸ findings 4, 6
- `src/io/fs_copytree.c` ▸ finding 4
- `src/template/staging.c` ▸ finding 4 (migrated caller)
- `src/serve/serve.c` ▸ finding 10
- `src/certs/certs_ca.c` ▸ finding 7
- `src/certs/certs_leaf.c` ▸ finding 7
- `src/certs/acme_finalize.c` ▸ finding 7
- `docs/current.audit.fix.md` ▸ step 5 mirror (audit D byte-for-byte)
- `docs/actual.plan.md` ▸ step 7 executing plan

### Renamed at step 8 (audit-fix skill)
- `.claude/codex-audit-fix/2026-04-08T20-06-32Z.bugs.audit.[ACTIVE ▸].md`
  → `…[COMPLETED ✓].md`
- `.claude/codex-audit-fix/2026-04-08T23-07-16Z.bugs.audit.[ACTIVE ▸].md`
  → `…[COMPLETED ✓].md`
- `.claude/codex-audit-fix/2026-04-09T02-08-22Z.bugs.audit.[ACTIVE ▸].md`
  → `…[COMPLETED ✓].md`
- `.claude/codex-audit-fix/2026-04-09T05-09-51Z.bugs.audit.[ACTIVE ▸].md`
  → `…[COMPLETED ✓].md`

All four are in `.claude/`, which is gitignored per `.gitignore:33`, so
the rename is a plain `mv`, not `git mv`.

### Read-only references
- `src/template/manifest_load.c:172-196` ▸ `parse_defaults` pattern
  reused in finding 1.
- `src/io/fs_copytree.c:122-135` ▸ recursive chmod-after-write pattern
  reused in finding 6.
- `src/template/writer.c:419-432` ▸ rendertree chmod-after-write pattern
  reused in finding 6.
- `src/commands/build_cmd.c:312-342` ▸ `project_root_abs` resolver, not
  touched here but provides the pattern for any future extraction.
- `meson.options:1-2` ▸ authoritative state for the `script_fallback`
  option name referenced in finding 2's error message.

## Commit breakdown (commit-guidelines skill)

All commits GPG-signed, no `Co-Authored-By`, no staging of `.claude/`,
`CLAUDE.md`, `GEMINI.md`, or `AGENTS.md`. Unicode symbols only from the
unicode-symbols skill — no emoji.

1. `fix(manifest): probe manifest.toml alongside template.phosphor.toml`
   - `include/phosphor/manifest.h`, `src/template/manifest_load.c`,
     `src/commands/{create,glow,build,serve,certs}_cmd.c`
   - Finding 5.
2. `fix(cmd): stop treating phosphor.toml as a template manifest`
   - `src/commands/doctor_cmd.c`, `src/commands/rm_cmd.c`
   - Finding 13. Lands after 1 so both files ride on `ph_manifest_find`.
3. `fix(config): parse [variables] table in .phosphor.toml discovery`
   - `src/core/config.c`
   - Finding 1.
4. `fix(template): mask secret variables in create and glow debug logs`
   - `include/phosphor/template.h`, `src/template/var_merge.c`,
     `src/commands/create_cmd.c`, `src/commands/glow_cmd.c`
   - Finding 8. Also guards against `NULL` value UB.
5. `fix(cmd): gate --legacy-scripts on PHOSPHOR_SCRIPT_FALLBACK`
   - `src/commands/build_cmd.c`, `src/commands/clean_cmd.c`
   - Findings 2 and 12 combined. Adds the native containment gate on
     the legacy build deploy path and forwards `--dry-run` / `--wipe`
     to the legacy clean script.
6. `fix(build): JSON-escape esbuild --define values`
   - `src/commands/build_cmd.c`
   - Finding 3.
7. `fix(template): enforce dest_dir containment on recursive walks`
   - `src/template/writer.c`, `src/io/fs_copytree.c`,
     `src/template/staging.c`
   - Finding 4.
8. `fix(template): preserve source mode on single-file copy and render`
   - `src/template/writer.c`
   - Finding 6.
9. `fix(certs): treat chmod and mkdir_p failures as fatal`
   - `src/certs/certs_ca.c`, `src/certs/certs_leaf.c`,
     `src/certs/acme_finalize.c`, `src/commands/certs_cmd.c`
   - Finding 7.
10. `fix(create): descend into single top-level dir when extracting archives`
    - `src/commands/create_cmd.c`
    - Finding 9. Depends on `ph_manifest_find` from commit 1.
11. `fix(serve): normalize signal exit and include port in startup URL`
    - `src/serve/serve.c`, `src/commands/serve_cmd.c`
    - Findings 10 and 14 combined.
12. `fix(doctor): check project-local esbuild before global PATH warning`
    - `src/commands/doctor_cmd.c`
    - Finding 11.
13. `docs(plan): record consolidated audit fix for four active bug audits`
    - `docs/current.audit.fix.md`, `docs/actual.plan.md`

13 commits total. Each commit body cites the audit IDs and finding
numbers it addresses.

## Verification (run only on request per build-workflow skill)

1. **Clean configure and compile**
   ```
   meson setup build --reconfigure
   ninja -C build
   ```
   Expect zero new warnings from touched files. The 86 pre-existing
   `-Wmissing-field-initializers` warnings in
   `src/commands/phosphor_commands.c` are unrelated and persist.

2. **Smoke test**
   ```
   ./build/phosphor version
   ./build/phosphor help
   ```

3. **Targeted probes**
   - **Finding 1**: project with
     `.phosphor.toml` containing `[variables]\nproject_name = "probe"`
     and a template that references `<<project_name>>` — running
     `create` should resolve the variable from the config, not from
     manifest defaults or env.
   - **Finding 2**: `phosphor build --legacy-scripts --project=<tmp>`
     on a binary compiled without `-Dscript_fallback=true` must exit
     `PH_ERR_USAGE` with a clear "requires -Dscript_fallback=true"
     message. With fallback compiled in, `--legacy-scripts
     --deploy-at=/tmp/escape` must exit `PH_ERR_VALIDATE`.
   - **Finding 3**: manifest with `[build].defines` where a `default`
     value contains `"` and `\`; `phosphor build` should pass a
     well-formed `--define:NAME="\"quoted\\\\"` argument to esbuild
     (or whatever the JSON escape yields).
   - **Finding 4**: scratch `copy` op with a deep subdir; swap a
     descendant path to a symlink pointing outside the dest tree right
     before execute; `ph_plan_execute` must error with "copy op N:
     child escapes dest_dir".
   - **Finding 5**: rename `template.phosphor.toml` to `manifest.toml`
     in `templates/cathode-landing/` (temporarily) and run
     `phosphor glow` — must succeed.
   - **Finding 6**: manifest with single-file `copy` or `render` op
     targeting a 0755 script; after execute, `stat -f %p`
     (macOS) / `stat -c %a` (linux) of the destination must show 0755.
   - **Finding 7**: make the cert output dir read-only and run
     `phosphor certs --generate --local` — should exit `PH_ERR_FS`
     with a clear path, not a downstream OpenSSL error.
   - **Finding 8**: manifest variable with `secret = true` and
     `phosphor create --verbose` — debug log must show `[secret]`,
     not the value. Optional unset variable must log `(null)`, not
     crash.
   - **Finding 9**: `tar -czf probe.tgz project-name/` where
     `project-name/template.phosphor.toml` exists, then
     `phosphor create --template=probe.tgz --name=foo` — must
     succeed, descending into `project-name/` automatically.
   - **Finding 10**: `phosphor serve` then `kill -INT` the PID —
     exit code must be 8.
   - **Finding 11**: `phosphor doctor --project=<project with
     node_modules/.bin/esbuild>` — no warning about missing esbuild.
   - **Finding 12**: `phosphor clean --legacy-scripts --dry-run`
     (with fallback compiled in) — invoked script must receive
     `--dry-run` in its argv.
   - **Finding 13**: directory with only `phosphor.toml` (no
     `template.phosphor.toml`) — `phosphor doctor` must report
     missing manifest; `phosphor rm <path>` must refuse without the
     manifest safety check.
   - **Finding 14**: `phosphor serve` startup banner must print
     `https://localhost:9443` (or whatever port is configured),
     not `https://localhost`.

4. **Final grep check**
   - `include/` should still have zero `-Dlibgit2=true` /
     `-Dlibarchive=true` / `-Dpcre2=true` occurrences (regression
     check from the prior audit-fix pass).

# Plan -- consolidated audit fix for E + F (5 distinct findings)

## Context

Codex audits E (`2026-04-09T08-08-09Z`) and F (`2026-04-09T09-09-58Z`)
were generated after the 14-finding consolidated fix landed. E has 4
findings; F duplicates those 4 and adds a fifth. The four shared
findings acknowledge our prior fixes but identify secondary gaps the
earlier pass left open. The fifth is entirely new.

All five are actionable in a single pass. Once addressed, both E and F
can be renamed from `[ACTIVE]` to `[COMPLETED]`.

## Finding inventory

1. **Top-level symlink escape in recursive copy/render** [E1 / F1] -- High
2. **TOCTOU deploy escape in build** [E2 / F2] -- High
3. **doctor cert check hardcodes template.phosphor.toml** [E3 / F3] -- Medium
4. **Archive descent breaks on metadata sidecars** [E4 / F4] -- Medium
5. **certs --force ignored for Let's Encrypt flows** [F5] -- Medium

## Pre-execution steps

1. **Mirror audits**: concatenate both E and F (byte-for-byte, with a
   separator) into `docs/current.audit.fix.md` so the working mirror
   covers all 5 findings.
2. **Save plan**: overwrite `docs/actual.plan.md` with this plan.

Both happen at the start of execution, before any code changes.

## Fixes

### Finding 1 (High) -- top-level symlink escape on recursive ops

**Problem**

`ph_fs_copytree` creates `dst` via `ph_fs_mkdir_p(dst, 0755)` at
`src/io/fs_copytree.c:36` before any containment check runs. The
per-child `ph_path_is_under` (line 101) only fires on descendants
inside the `readdir` loop. If `dst` itself is a symlink pointing
outside `contain_root`, the top-level mkdir follows the symlink and
the entire tree writes outside the approved root.

Same gap in the directory branch of PH_OP_RENDER: writer.c enters
`rendertree_recurse` at line 736 without checking `op->to_abs`
first (the single-file branches do check at lines 663-664 and
766-767).

**Fix**

Two insertion points, both cheap:

1. `src/io/fs_copytree.c` -- at the top of `copytree_recurse`, after
   the NULL check and before `ph_fs_mkdir_p(dst, ...)` (between
   current lines 25 and 35):
   ```c
   /* top-level containment: verify dst itself is under contain_root
    * before mkdir_p follows any symlinked components. */
   if (depth == 0 && contain_root &&
       !ph_path_is_under(dst, contain_root)) {
       if (err)
           *err = ph_error_createf(PH_ERR_VALIDATE, 0,
               "copytree: destination escapes containment root: %s",
               dst);
       closedir(d);
       return PH_ERR;
   }
   ```
   Move the `opendir` below this check, or insert the check between
   `opendir` and `mkdir_p`. Either way the guard runs before any
   filesystem mutation.

2. `src/template/writer.c` -- in the directory branch of PH_OP_COPY
   (around line 635, before `ph_fs_copytree`) and PH_OP_RENDER
   (around line 724, before `rendertree_recurse`), add the same
   pattern the single-file branches already use:
   ```c
   if (plan->dest_dir &&
       !ph_path_is_under(op->to_abs, plan->dest_dir)) {
       if (err)
           *err = ph_error_createf(PH_ERR_VALIDATE, 0,
               "copy op %zu: target escapes dest_dir: %s",
               i, op->to_abs);
       goto cleanup_err;
   }
   ```

---

### Finding 2 (High) -- TOCTOU deploy escape in build

**Problem**

`build_cmd.c:610` calls `deploy_at_escapes_root` once and then
trusts `deploy_dir` for the rest of the pipeline:

- `ph_fs_rmtree(deploy_dir, ...)` at lines 627 and 979
- `ph_fs_mkdir_p(deploy_dir, 0755)` at line 646
- `ph_fs_copytree(build_dir, deploy_dir, ..., NULL, ...)` at line 983

The copytree call passes `NULL` for `contain_root`. A same-user
symlink swap at `deploy_dir` between the escape check and the
copytree can redirect the entire deploy write outside the project
root.

**Fix**

Pass `project_root_abs` as `contain_root` to the deploy copytree
at `build_cmd.c:983-985`, replacing `NULL`:

```c
if (ph_fs_copytree(build_dir, deploy_dir,
                   metadata_skip_filter, NULL,
                   project_root_abs, &err) != PH_OK) {
```

This is the same pattern `writer.c` already uses (pass the approved
root to copytree so every child is re-checked). The static-asset
copytree at line 942 can stay `NULL` -- `build_dir` is a private
mkdtemp-like directory we just created, not user-controlled.

For the `ph_fs_rmtree` + `ph_fs_mkdir_p` window: add a re-check of
`deploy_dir` via `ph_path_is_under(deploy_dir, project_root_abs)`
right before the final deploy wipe at line 979 (the step 10 deploy
block). This catches swaps that happened between the original check
and the deploy phase.

---

### Finding 3 (Medium) -- doctor cert check hardcodes filename

**Problem**

`doctor_cmd.c:191` calls `ph_path_join(project_root,
"template.phosphor.toml")` directly instead of using
`ph_manifest_find`. Projects using the documented alternate filename
`manifest.toml` pass the manifest check (which now uses
`ph_manifest_find`) but silently skip all cert diagnostics.

**Fix**

Replace the `ph_path_join` + manual stat in `check_certs`
(lines 191-198) with `ph_manifest_find(project_root)`:

```c
static void check_certs(const char *project_root) {
    char *toml_path = ph_manifest_find(project_root);
    if (!toml_path) return;
    ...
```

No signature change. The rest of the function already works with a
generic path from `toml_path` -- the hardcoded name was only in the
probe, not in any downstream logic.

---

### Finding 4 (Medium) -- archive descent breaks on metadata sidecars

**Problem**

`create_cmd.c:290` only skips dot-prefixed entries (`te->d_name[0]
== '.'`). Common archive sidecars like `__MACOSX`, `Thumbs.db`, and
`desktop.ini` are not dot-prefixed and count as visible children.
An archive with `project-name/` + `__MACOSX/` has child_count == 2,
so the descent logic gives up and reports "no manifest found".

The existing `ph_metadata_is_denied` helper in
`src/io/metadata_filter.c` already knows these names (lines 6-13:
`.DS_Store`, `Thumbs.db`, `.Spotlight-V100`, `.Trashes`,
`desktop.ini`, `.fseventsd`, plus `._*` and `.phosphor-staging-*`
glob patterns). It also needs `__MACOSX` added (not dot-prefixed,
common in macOS zip archives).

**Fix**

1. Add `"__MACOSX"` to the `deny_exact` table in
   `src/io/metadata_filter.c:6-13`.

2. In the archive descent loop at `create_cmd.c:289-290`, replace
   the dot-prefix check with a call to `ph_metadata_is_denied`:
   ```c
   while ((te = readdir(td)) != NULL) {
     if (te->d_name[0] == '.' ||
         ph_metadata_is_denied(te->d_name)) continue;
   ```
   This reuses the existing helper rather than duplicating the deny
   list.

---

### Finding 5 (Medium) -- certs --force ignored for LE flows

**Problem**

`run_letsencrypt` at `certs_cmd.c:80` accepts `bool force` but
immediately casts it away with `(void)force` at line 87. The ACME
path unconditionally regenerates the private key and overwrites
certs. The local CA path (`certs_ca.c`) and local leaf path
(`certs_leaf.c`) both honor `--force` (they check whether the file
exists and skip generation without `--force`).

**Fix**

In `run_letsencrypt` (`certs_cmd.c`), remove `(void)force` and add
an early-out guard before the ACME finalize call (around line 415,
before `ph_acme_finalize`):

```c
if (!force) {
    ph_fs_stat_t pst;
    if (ph_fs_stat(privkey_path, &pst) == PH_OK && pst.exists) {
        ph_log_info("certs: %s already exists (use --force to "
                     "overwrite)", privkey_path);
        /* skip this domain, clean up and continue loop */
        ...
        continue;
    }
}
```

Mirror the pattern from `certs_ca.c:92-97` and `certs_leaf.c:92-96`
where existing key files are skipped without `--force`.

---

## Execution order

1. Finding 4 (metadata sidecar filter) -- adds `__MACOSX` to
   deny_exact and reuses the helper in create_cmd.c. Smallest scope,
   unblocks archive workflows.
2. Finding 3 (doctor cert check) -- single-function change in
   doctor_cmd.c, rides on `ph_manifest_find` from the prior pass.
3. Finding 1 (top-level symlink escape) -- copytree.c + writer.c
   guards. Do before finding 2 since build_cmd.c uses copytree.
4. Finding 2 (TOCTOU deploy) -- build_cmd.c: pass `project_root_abs`
   to deploy copytree + re-check before deploy wipe.
5. Finding 5 (certs --force) -- certs_cmd.c: honor force flag in LE
   pipeline.

## Commit breakdown

1. `fix(create): skip metadata sidecars during archive wrapper descent`
   - `src/io/metadata_filter.c`, `src/commands/create_cmd.c`
   - Finding 4 [E4 / F4]
2. `fix(doctor): use ph_manifest_find for cert diagnostics`
   - `src/commands/doctor_cmd.c`
   - Finding 3 [E3 / F3]
3. `fix(template): validate top-level destination before recursive walks`
   - `src/io/fs_copytree.c`, `src/template/writer.c`
   - Finding 1 [E1 / F1]
4. `fix(build): harden deploy copytree against post-check symlink swap`
   - `src/commands/build_cmd.c`
   - Finding 2 [E2 / F2]
5. `fix(certs): honor --force flag in Let's Encrypt pipeline`
   - `src/commands/certs_cmd.c`
   - Finding 5 [F5]
6. `docs(plan): record consolidated audit fix for E and F`
   - `docs/actual.plan.md` (this plan)
   - `docs/current.audit.fix.md` (both E and F concatenated)

## Critical files

### Modified
- `src/io/metadata_filter.c` -- finding 4
- `src/commands/create_cmd.c` -- finding 4
- `src/commands/doctor_cmd.c` -- finding 3
- `src/io/fs_copytree.c` -- finding 1
- `src/template/writer.c` -- finding 1
- `src/commands/build_cmd.c` -- finding 2
- `src/commands/certs_cmd.c` -- finding 5
- `docs/actual.plan.md` -- plan mirror
- `docs/current.audit.fix.md` -- audit mirror

### Renamed at end (audit-fix skill)
- `.claude/codex-audit-fix/2026-04-09T08-08-09Z.bugs.audit.[ACTIVE].md`
  -> `...[COMPLETED].md`
- `.claude/codex-audit-fix/2026-04-09T09-09-58Z.bugs.audit.[ACTIVE].md`
  -> `...[COMPLETED].md`

### Read-only references
- `src/io/path_norm.c:320` -- `ph_path_is_under` implementation
- `src/certs/certs_ca.c:92-97` -- `--force` skip pattern to mirror
- `src/certs/certs_leaf.c:92-96` -- `--force` skip pattern to mirror
- `src/platform/posix/fs_posix.c:154-165` -- `ph_fs_mkdir_p` symlink behavior

## Verification (run only on request)

1. `meson setup build --reconfigure && ninja -C build` -- zero new warnings
2. `./build/phosphor version` + `./build/phosphor help` -- smoke test
3. Archive descent: create a tar.gz with `project/template.phosphor.toml`
   + `__MACOSX/` sidecar; `phosphor create --template=probe.tgz` should
   descend into `project/` and ignore the sidecar.
4. Doctor certs: rename manifest to `manifest.toml`; `phosphor doctor`
   should show cert diagnostics (not silently skip them).
5. Copytree top-level: craft a plan op where `op->to_abs` is a symlink
   outside `plan->dest_dir`; execute must fail with "escapes dest_dir".
6. Deploy copytree: verify build_cmd.c passes `project_root_abs` to
   the deploy copytree call (code review -- hard to trigger TOCTOU
   in a deterministic test).
7. Certs force: `phosphor certs --letsencrypt` on a domain that already
   has certs should skip without `--force` and regenerate with `--force`.

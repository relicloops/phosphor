# Phosphor Bugs Audit

Run time: `2026-04-09T08:08:09Z`

## Scope

- Read-only audit of `README.md`, `meson.build`, `src/`, and `include/`
- Compared README-documented behavior against the current dirty workspace
- No build, test, or long-running commands were run

## Repo state

- Branch: `main` ahead of `origin/main` by 26 commits
- Working tree was dirty in `src/`, `include/`, `docs/`, and `diagram/`
- This report reflects the current workspace, not just `HEAD`

## Summary

- Several findings from earlier audits are fixed in the current tree, including `manifest.toml` probing in the main command paths, secret-variable masking, `serve` signal normalization, and the `build` define escaping work.
- Four actionable defects remain in the current workspace: two path-containment gaps, one incomplete `manifest.toml` rollout in `doctor`, and one archive-template compatibility break.

## Findings

### 1. High: recursive template directory ops still allow a top-level symlink escape

Evidence:

- Directory `copy` ops call `ph_fs_copytree(op->from_abs, op->to_abs, ..., plan->dest_dir, ...)` in `src/template/writer.c:640-643`.
- Directory `render` ops call `rendertree_recurse(op->from_abs, op->to_abs, ...)` with `dest_dir = plan->dest_dir` in `src/template/writer.c:739-749`.
- Both recursive walkers create the destination directory before they perform any containment re-check:
  - `src/io/fs_copytree.c:35-42`
  - `src/template/writer.c:290-296`
- The new containment check only runs on `dst_child`, after that `mkdir -p` step:
  - `src/io/fs_copytree.c:97-112`
  - `src/template/writer.c:422-436`
- `ph_fs_mkdir_p()` treats any existing path component as acceptable (`errno == EEXIST`) and then keeps descending, so a swapped symlinked parent is followed:
  - `src/platform/posix/fs_posix.c:154-165`

Impact:

- The recent containment hardening catches descendant escapes, but it still misses the first directory target itself.
- If a same-user attacker swaps the planned top-level output directory, or one of its parent components, to a symlink after planning but before execution, `create` or `glow` can still create directories and write files outside `plan->dest_dir`.

### 2. High: `build` still has a TOCTOU deploy escape despite the preflight containment check

Evidence:

- README promises that deploy targets are checked for containment under the project root before writes happen: `README.md:163-164`.
- `ph_cmd_build()` performs that one-time canonical check at `src/commands/build_cmd.c:601-619`.
- After the check, the command still re-resolves the path for destructive and write operations:
  - `ph_fs_rmtree(deploy_dir, ...)` at `src/commands/build_cmd.c:621-629` and again at `src/commands/build_cmd.c:976-985`
  - `ph_fs_mkdir_p(deploy_dir, 0755)` at `src/commands/build_cmd.c:646-653`
  - `ph_fs_copytree(build_dir, deploy_dir, ..., NULL, ...)` at `src/commands/build_cmd.c:983-985`
- `ph_fs_stat()` uses `lstat(path, ...)` on the full path, which still traverses intermediate symlinked parents before it decides whether to recurse or unlink:
  - `src/platform/posix/fs_posix.c:13-32`
- The new `contain_root` support in `ph_fs_copytree()` is explicitly skipped here by passing `NULL`, even though the API exists for per-child containment:
  - `include/phosphor/fs.h:51-60`
  - `src/commands/build_cmd.c:942-944`
  - `src/commands/build_cmd.c:983-985`

Impact:

- A post-check symlink swap can still redirect `--clean-first`, the final deploy wipe, or the deploy copy outside the project root.
- In the worst case, `phosphor build` can recursively delete or repopulate an arbitrary same-user directory even though the documented containment gate already passed.

### 3. Medium: `doctor` still skips certificate diagnostics for `manifest.toml` projects

Evidence:

- README documents both `template.phosphor.toml` and `manifest.toml` as supported manifest filenames: `README.md:193-210`.
- `doctor`'s manifest presence check now uses `ph_manifest_find()` and correctly accepts either filename: `src/commands/doctor_cmd.c:33-44`.
- But the cert subcheck still hardcodes `template.phosphor.toml`:
  - `src/commands/doctor_cmd.c:189-207`

Impact:

- A project that uses the documented alternate filename can get `[ok] manifest: manifest.toml found`, but `doctor` will silently skip all cert expiry and presence checks.
- That makes `doctor` internally inconsistent and weakens its value for exactly the projects the README says are supported.

### 4. Medium: archive wrapper descent still breaks on common metadata sidecars

Evidence:

- README advertises archive templates as a supported source type: `README.md:176-182`.
- `create` only descends into an extracted wrapper directory when there is exactly one visible non-hidden child at the archive root:
  - `src/commands/create_cmd.c:278-319`
- That loop ignores only dot-prefixed names. It does not ignore other metadata sidecars such as `Thumbs.db`, `desktop.ini`, or `__MACOSX`.
- The project already has a metadata deny list for these kinds of artifacts, but the wrapper detection path does not use it:
  - `src/io/metadata_filter.c:6-18`
- Archive extraction preserves regular files and directories as-is, so those sidecars survive into the extracted root:
  - `src/io/archive.c:230-282`

Impact:

- A common wrapped archive that contains `project-name/` plus `__MACOSX/`, `Thumbs.db`, or similar metadata fails the "exactly one child" test, so `create` reports "no manifest found" even though the template itself is valid.
- This keeps archive-template support narrower than the README implies, especially for user-generated zip files.

## Recommended order

1. Re-check the top-level recursive destination before any `mkdir -p` in the template walkers, not just the descendants.
2. Harden `build`'s deploy path against post-validation symlink swaps, including the delete path, and stop opting out of `contain_root` where the destination is security-sensitive.
3. Switch `doctor`'s cert inspection over to `ph_manifest_find()`.
4. Make archive wrapper descent ignore metadata sidecars the same way the rest of the filesystem pipeline does.

## Verification limits

- Static analysis only.
- No compile, smoke, or docs-build commands were run because the repository instructions require asking before long-running work.


---

# Phosphor Bugs Audit

Run time: `2026-04-09T09:09:58Z`

## Scope

- Read-only audit of `README.md`, `meson.build`, `meson.options`, `src/`, and `include/`
- Compared README-documented behavior against the current command implementations and Meson compile graph
- No build, test, or long-running commands were run

## Repo state

- Branch: `main`
- Working tree was dirty in `docs/`, `include/`, and `src/`, with untracked content under `diagram/`
- This report reflects the current workspace, not just `HEAD`

## Summary

- The current tree still carries four previously reported defects that materially affect safety or documented behavior: two path-containment gaps, one `manifest.toml` diagnostic gap, and one archive-template compatibility break.
- One additional command-surface mismatch remains in the current workspace: `certs --force` is advertised by the CLI but ignored for Let's Encrypt flows, so existing ACME key/cert material can be overwritten without the documented opt-in.
- README also lags the actual build graph in one important place: Meson now requires `python3` for embedded template code generation during normal builds.

## Findings

### 1. High: recursive template directory ops still allow a top-level symlink escape

Evidence:

- Directory `copy` ops call `ph_fs_copytree(op->from_abs, op->to_abs, ..., plan->dest_dir, ...)` in `src/template/writer.c:640-643`.
- Directory `render` ops call `rendertree_recurse(op->from_abs, op->to_abs, ...)` with `dest_dir = plan->dest_dir` in `src/template/writer.c:739-749`.
- Both recursive walkers create the destination directory before they perform any containment re-check:
  - `src/io/fs_copytree.c:35-42`
  - `src/template/writer.c:290-296`
- The newer containment checks only run on descendants like `dst_child`, not on the top-level `dst` itself:
  - `src/io/fs_copytree.c:97-112`
  - `src/template/writer.c:422-436`
- `ph_fs_mkdir_p()` still accepts `EEXIST` and continues descending through path components, so a swapped symlinked parent is followed:
  - `src/platform/posix/fs_posix.c:154-165`

Impact:

- The recent hardening catches descendant escapes, but it still misses the first directory target itself.
- If a same-user attacker swaps the planned top-level output directory, or one of its parent components, to a symlink after planning but before execution, `create` or `glow` can still create directories and write files outside `plan->dest_dir`.

### 2. High: `build` still has a TOCTOU deploy escape despite the documented containment guard

Evidence:

- README promises that deploy targets are checked for containment under the project root before writes happen: `README.md:163-164`.
- `ph_cmd_build()` performs that one-time canonical check at `src/commands/build_cmd.c:601-619`.
- After the check, the command still reuses the raw path for destructive and write operations:
  - `ph_fs_rmtree(deploy_dir, ...)` at `src/commands/build_cmd.c:621-629` and `src/commands/build_cmd.c:976-980`
  - `ph_fs_mkdir_p(deploy_dir, 0755)` at `src/commands/build_cmd.c:646-653`
  - `ph_fs_copytree(build_dir, deploy_dir, ..., NULL, ...)` at `src/commands/build_cmd.c:983-985`
- The deploy copy still opts out of `ph_fs_copytree()`'s per-child containment support by passing `NULL` for `contain_root`: `src/commands/build_cmd.c:983-985`.

Impact:

- A post-check symlink swap can still redirect `--clean-first`, the final deploy wipe, or the deploy copy outside the project root.
- In the worst case, `phosphor build` can recursively delete or repopulate an arbitrary same-user directory even though the documented containment gate already passed.

### 3. Medium: `doctor` still skips certificate diagnostics for `manifest.toml` projects

Evidence:

- README documents both `template.phosphor.toml` and `manifest.toml` as supported manifest filenames: `README.md:193-208`.
- `doctor`'s manifest presence check correctly uses `ph_manifest_find()` and therefore accepts either file name: `src/commands/doctor_cmd.c:33-44`.
- But the cert subcheck still hardcodes `template.phosphor.toml`:
  - `src/commands/doctor_cmd.c:189-207`

Impact:

- A project that uses the documented alternate filename can get `[ok] manifest: manifest.toml found`, but `doctor` will silently skip all cert expiry and presence checks.
- That makes `doctor` internally inconsistent and weakens it for exactly the projects the README says are supported.

### 4. Medium: archive wrapper descent still breaks on common metadata sidecars

Evidence:

- README advertises archive templates as a supported source type: `README.md:176-182`.
- `create` only descends into an extracted wrapper directory when there is exactly one visible non-hidden child at the archive root: `src/commands/create_cmd.c:278-321`.
- That loop ignores only dot-prefixed names. It does not ignore common sidecars like `Thumbs.db`, `desktop.ini`, or wrapper directories such as `__MACOSX`.
- The project already filters some metadata files elsewhere, but that filter is not reused here:
  - `src/io/metadata_filter.c:6-33`

Impact:

- A wrapped archive that contains `project-name/` plus a metadata sidecar fails the "exactly one child" test, so `create` reports "no manifest found" even though the template itself is valid.
- This keeps archive-template support narrower than the README implies, especially for user-generated zip files.

### 5. Medium: `certs --force` is ignored for Let's Encrypt flows

Evidence:

- The certs command spec advertises `--force` as "overwrite existing cert files": `src/commands/phosphor_commands.c:152-153`.
- The Let's Encrypt pipeline receives the flag but immediately discards it with `(void)force`: `src/commands/certs_cmd.c:75-87`.
- The ACME finalize path then unconditionally regenerates the private key and writes the certificate output:
  - `openssl genrsa -out <privkey_path>` in `src/certs/acme_finalize.c:41-63`
  - `ph_io_write_file(cert_path, ...)` in `src/certs/acme_finalize.c:288-293`

Impact:

- `phosphor certs --letsencrypt ...` and `phosphor certs --generate` for LE domains can overwrite existing ACME key/cert material even when the user did not pass `--force`.
- That is inconsistent with the advertised CLI behavior and with the local CA / local leaf paths, which do honor overwrite gating.

## Notable docs drift

- README's build prerequisites omit `python3`, but the top-level Meson build always calls `find_program('python3')` and uses it for embedded template codegen: `meson.build:311-333`, `README.md:85-90`.
- README says "phosphor supports three template source types" but then lists four (`local`, `remote git`, `archive file`, `embedded`): `README.md:176-180`.
- The current tree contains public/source files not reflected in the repository structure snapshot, including `src/args-parser/args_helpers.c`, `src/template/embedded_registry.c`, `src/core/color.c`, `src/core/term.c`, `include/phosphor/certs.h`, and `include/phosphor/embedded.h`.

## Recommended order

1. Re-check the top-level recursive destination before any `mkdir -p` in the template walkers, not just descendants.
2. Harden `build`'s deploy path against post-validation symlink swaps, including delete and deploy-copy paths.
3. Switch `doctor`'s cert inspection over to `ph_manifest_find()`.
4. Make archive wrapper descent reuse metadata filtering or explicitly ignore common archive sidecars.
5. Either honor `--force` in the Let's Encrypt path or reject it there until overwrite behavior is implemented.
6. Update README's build prerequisites and template-source wording to match the current Meson graph and command surface.

## Verification limits

- Static analysis only.
- No compile, smoke, or docs-build commands were run because the repository instructions require asking before long-running work.

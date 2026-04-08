# Plan ▸ audit fix 2026-04-08T17-06-51Z.bugs

## Context

Codex produced a static-source audit
(`.claude/codex-audit-fix/2026-04-08T17-06-51Z.bugs.audit.[ACTIVE ▸].md`) that
lists 5 confirmed hardening gaps and 1 documentation-drift item in the current
dirty working tree. Every finding concerns a path that can escape the declared
project root, or public-header comments that no longer reflect the build and
runtime.

Intended outcome: every path-bearing manifest / CLI field that feeds `serve`,
`build`, `certs`, and the template executor is anchored to and
containment-checked against the resolved project root; the public headers
match how Meson actually wires libgit2 / libarchive / PCRE2 and how `archive`
and `git_fetch` allocate their temp directories.

## Findings and fixes

### Finding 1 (High) ▸ `serve --project` mishandles relative `certs-root`

**Problem**
`src/commands/serve_cmd.c:338-349` sets `cfg.ns.certs_root` directly from the
`--certs-root` flag, `[serve.neonsignal].certs_root`, or
`[certs].output_dir` without calling `anchor_serve_path`. The containment gate
at `src/commands/serve_cmd.c:469-502` then calls
`serve_validate_under_root("certs-root", …, project_root_abs)`, which delegates
to `ph_path_is_under` → `ph_path_resolve` → `realpath(".")` — i.e. the caller
cwd, not `project_root_abs`.

**Fix**
1. Add `char *certs_root;` to `struct serve_derived_paths` and free it in
   `serve_derived_paths_free`.
2. Replace the raw assignments at `src/commands/serve_cmd.c:338-349` with
   the `anchor_serve_path(raw, project_root_abs, false)` pattern used by
   every other serve path field.
3. No change needed to the containment call; it will now see a
   project-anchored absolute path.

---

### Finding 2 (High) ▸ LE path pipeline not rooted to project, `--output` escapes

**Problem**
- `src/commands/certs_cmd.c:473-479` leaves `project_root` as the raw
  `--project` value (no normalization to absolute).
- `src/commands/certs_cmd.c:502-509` overwrites `certs_cfg.output_dir` from
  `--output` with no revalidation.
- `run_letsencrypt` uses `config->account_key`, `d->webroot`, and
  `domain_dir` without anchoring to or containment-checking against
  `project_root`.

**Fix**
1. At the top of `ph_cmd_certs`, replace the raw `project_root` assignment
   with the same `project_root_abs` resolver used by `ph_cmd_build`
   (`src/commands/build_cmd.c:312-342`). Thread `project_root_abs` through
   `run_local`, `run_letsencrypt`, and every TOML-path join.
2. After the `--output` override, reject absolute / traversal values inline
   with `ph_path_is_absolute` and `ph_path_has_traversal`.
3. In `run_letsencrypt`, before using `config->account_key`: if non-NULL and
   not absolute, `ph_path_join(project_root_abs, key)` and containment-check.
   Reject absolute values.
4. Containment-check `domain_dir` via `ph_path_is_under(domain_dir,
   project_root_abs)` immediately after construction.
5. For each LE domain iteration, anchor `d->webroot` under `project_root_abs`
   into a local `webroot_abs`, containment-check, and pass that to
   `ph_acme_challenge_respond`. Free on every loop exit.
6. Update the dry-run `webroot:` log line to print `webroot_abs`.

---

### Finding 3 (Medium) ▸ watcher receives raw `[deploy].public_dir`

**Problem**
`parse_deploy_config` copies `[deploy].public_dir` without validation. The
serve watcher argv appends it directly.

**Fix**
In `parse_deploy_config`, after copying `out->public_dir`, reject absolute
or traversal values with a `PH_ERR_CONFIG` error. All call sites inherit
the tightened invariant.

---

### Finding 4 (Medium) ▸ copy/render writes lack execute-time containment

**Problem**
`ph_plan_execute` re-checks `plan->dest_dir` containment for `PH_OP_CHMOD`
and `PH_OP_REMOVE` but not for the single-file branches of `PH_OP_COPY` and
`PH_OP_RENDER`.

**Fix**
Add the same `ph_path_is_under(op->to_abs, plan->dest_dir)` gate
immediately before each single-file write, mirroring the chmod/remove
pattern and using the same `goto cleanup_err` path.

---

### Finding 5 (Medium) ▸ `[build].entry` unconstrained

**Problem**
`parse_build_config` copies `[build].entry` without validation.

**Fix**
Right after `out->entry = toml_get_string(b, "entry");`, reject absolute
and traversal values with `PH_ERR_CONFIG`.

---

### Finding 6 (Low) ▸ public header doc drift

**Problem**
`include/phosphor/git_fetch.h`, `include/phosphor/archive.h`, and
`include/phosphor/regex.h` still describe `-Dlibgit2=true`,
`-Dlibarchive=true`, `-Dpcre2=true`, and predictable
`/tmp/...-<pid>-<timestamp>` temp directories. Meson unconditionally wires
all three, and both archive / git_fetch use `mkdtemp`.

**Fix**
Documentation-only. Drop the `-D...` flag text, note the deps are
unconditionally linked, and replace the predictable temp-dir examples with
`mkdtemp`-based ones. No runtime change.

---

## Commit breakdown

1. `fix(serve): anchor certs_root under project root before containment`
2. `fix(certs): root LE account_key and webroot under project`
3. `fix(manifest): reject non-relative [deploy].public_dir at parse`
4. `fix(template): re-check copy/render containment before writing`
5. `fix(manifest): reject non-relative [build].entry at parse`
6. `docs(include): sync header comments with meson and mkdtemp`

GPG-signed, no `Co-Authored-By`, no staging of `.claude/`, `CLAUDE.md`,
`GEMINI.md`, `AGENTS.md`.

---

## Critical files

### Modified
- `src/commands/serve_cmd.c` ▸ finding 1
- `src/commands/certs_cmd.c` ▸ finding 2
- `src/template/manifest_load.c` ▸ findings 3 and 5
- `src/template/writer.c` ▸ finding 4
- `include/phosphor/git_fetch.h` ▸ finding 6
- `include/phosphor/archive.h` ▸ finding 6
- `include/phosphor/regex.h` ▸ finding 6

### Renamed (at completion)
- `.claude/codex-audit-fix/2026-04-08T17-06-51Z.bugs.audit.[ACTIVE ▸].md`
  → `…[COMPLETED ✓].md` via `git mv`.

---

## Verification (run only on request per build-workflow skill)

1. `meson setup build --reconfigure && ninja -C build`
2. `./build/phosphor version` and `./build/phosphor --help`
3. Manual path-escape probes on a scratch project (per plan verification
   section).
4. Grep `include/` for `-Dlibgit2=true`, `-Dlibarchive=true`, `-Dpcre2=true`,
   `/tmp/.phosphor-clone-`, `/tmp/.phosphor-extract-` — must no longer show
   opt-in flag text or pid/timestamp temp-dir examples.

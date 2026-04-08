# Phosphor Deep Bugs Audit

Run time: 2026-04-08T17-06-51Z
Mode: static source audit only
Scope: `README.md`, `meson.build`, `meson.options`, `src/`, `include/`
Tree state: audit reflects the current dirty working tree; I did not run builds or tests.

## Confirmed findings

### 1. High: `serve --project` still mishandles relative `certs-root`

- `ph_cmd_serve()` anchors `www_root`, working-dir, upload-dir, augments-dir, grafts-dir, log-directory, and redirect paths under `project_root_abs`, but it leaves `cfg.ns.certs_root` as the raw flag or manifest string:
  - `src/commands/serve_cmd.c:338-349`
  - `src/commands/serve_cmd.c:351-403`
- The later containment gate calls `serve_validate_under_root("certs-root", cfg.ns.certs_root, project_root_abs)`:
  - `src/commands/serve_cmd.c:469-500`
- `serve_validate_under_root()` relies on `ph_path_is_under()`, and `ph_path_is_under()` resolves relative children against the caller's current working directory via `realpath(".")`, not against `project_root_abs`:
  - `src/commands/serve_cmd.c:82-95`
  - `src/io/path_norm.c:213-340`

Impact:

- `phosphor serve --project /path/to/site` can reject a valid relative certs directory from `[certs].output_dir` or `[serve.neonsignal].certs_root` when invoked outside the project root.
- In the worst case, if the caller cwd happens to contain a matching relative path, the check is performed against the wrong tree entirely.
- This contradicts the documented intent that serve reads cert defaults from the project manifest while using `--project` as the anchor:
  - `README.md:111-120`

### 2. High: the Let's Encrypt path pipeline is still not rooted to the project, and `--output` can escape it

- The manifest parser now validates `output_dir`, `account_key`, `dir_name`, and `webroot` as relative, non-traversing paths:
  - `src/certs/certs_config.c:55-79`
  - `src/certs/certs_config.c:137-154`
  - `src/certs/certs_config.c:236-263`
- But `run_letsencrypt()` still uses `config->account_key` and `d->webroot` as raw strings, without anchoring them under `project_root`:
  - `src/commands/certs_cmd.c:90-107`
  - `src/commands/certs_cmd.c:285-288`
- `ph_cmd_certs()` also overwrites `certs_cfg.output_dir` from `--output` without revalidating it:
  - `src/commands/certs_cmd.c:502-509`
- `run_letsencrypt()` then builds `domain_dir = ph_path_join(project_root, config->output_dir)` and writes to it without any `ph_path_is_under()` containment check:
  - `src/commands/certs_cmd.c:121-130`
  - `src/commands/certs_cmd.c:307-319`

Impact:

- `phosphor certs --project /path/to/site --letsencrypt ...` will still read or create `account_key` and HTTP-01 challenge files relative to the caller cwd instead of the target project.
- `--output=../../somewhere-else` or any other escaping relative path can push the LE private key and certificate output outside the project root, despite the README promising that all cert paths are project-root validated:
  - `README.md:237-245`
- The local CA and local leaf code already do a second `ph_path_is_under()` check; the LE path is the inconsistent gap.

### 3. Medium: `serve` still passes raw `[deploy].public_dir` to the default watcher

- `parse_deploy_config()` copies `[deploy].public_dir` without validation:
  - `src/template/manifest_load.c:337-347`
- `ph_cmd_serve()` stores that raw string in `cfg.ns.deploy_dir` for the watcher:
  - `src/commands/serve_cmd.c:407-413`
- `ph_serve_start()` appends it directly to the default watcher argv as `--deploy <deploy_dir>`:
  - `src/serve/serve.c:394-410`

Impact:

- If `[serve.neonsignal].watch = true` and no explicit `watch_cmd` is provided, a manifest with `[deploy].public_dir = "../../outside"` still sends an escaping deploy path to the build-on-change helper.
- The main `build` command has containment checks for `[deploy].public_dir`; the watcher path bypasses that hardening and can reintroduce out-of-tree writes through the default watch workflow.

### 4. Medium: template `copy` and `render` writes still lack the final containment re-check added for `chmod` and `remove`

- Plan build does an early `ph_path_is_under()` check when `to_abs` is first resolved:
  - `src/template/planner.c:256-291`
- During execution, `chmod` and `remove` re-check `plan->dest_dir` immediately before mutating the filesystem:
  - `src/template/writer.c:789-850`
- `copy` and `render` do not. They write directly to `op->to_abs` with `ph_fs_atomic_write()` or `ph_fs_write_file()`:
  - `src/template/writer.c:636-668`
  - `src/template/writer.c:713-780`
- `ph_fs_write_file()` opens the target with plain `open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)`, which follows symlinks:
  - `src/platform/posix/fs_posix.c:81-104`

Impact:

- A symlink swapped into the staging or destination tree after plan construction can redirect single-file `copy` or `render` writes outside `dest_dir`.
- The codebase already recognized this TOCTOU class for `chmod` and `remove`; the missing symmetry on `copy` and `render` leaves the same class of escape available for file creation and overwrite.

### 5. Medium: `[build].entry` is still unconstrained and can point outside the project

- `parse_build_config()` copies `[build].entry` without absolute-path or traversal validation:
  - `src/template/manifest_load.c:285-296`
- `ph_cmd_build()` passes that manifest value directly to esbuild:
  - `src/commands/build_cmd.c:728-731`

Impact:

- A project manifest can set `entry = "../other-project/src/app.tsx"` or an absolute path and cause `phosphor build` to bundle code from outside the declared project root.
- The deploy target is tightly contained, but the input root is not, which is inconsistent with the rest of the build hardening and makes untrusted manifests a broader read surface than the README implies.

## Documentation drift

### 6. Low: public headers still describe feature flags and temp-dir behavior that no longer match Meson or the code

- The public headers still claim optional dependency support is enabled with flags like `-Dlibgit2=true`, `-Dlibarchive=true`, and `-Dpcre2=true`:
  - `include/phosphor/git_fetch.h:8-12`
  - `include/phosphor/git_fetch.h:67-69`
  - `include/phosphor/archive.h:8-12`
  - `include/phosphor/archive.h:44-45`
  - `include/phosphor/regex.h:8-12`
  - `include/phosphor/regex.h:30-32`
- The top-level Meson build now unconditionally wires in libgit2, libarchive, and PCRE2 and only exposes `script_fallback` and `dashboard` as user options:
  - `meson.build:126-210`
  - `meson.options:1-4`
- The archive and git headers also still document predictable `/tmp/...<pid>-<timestamp>` temp directories, but the implementations now use `mkdtemp()`:
  - `include/phosphor/archive.h:49-53`
  - `include/phosphor/git_fetch.h:72-77`
  - `src/io/archive.c:156-166`
  - `src/io/git_fetch.c:203-213`

Impact:

- This is documentation drift rather than a runtime bug, but it makes the public API and build surface look older and looser than the actual implementation.

## Recommended fix order

1. Anchor and revalidate `serve` certs paths against `project_root_abs`, especially `certs_root`.
2. Root LE `account_key` and `webroot` under the project, and revalidate the `--output` override before use.
3. Apply the same containment gate to the watcher deploy path that `build` already uses.
4. Add an execution-time `ph_path_is_under()` re-check for file `copy` and `render` operations.
5. Validate `[build].entry` the same way path-like manifest fields are validated elsewhere.
6. Refresh the public header comments so they match the current Meson feature model and temp-dir implementation.

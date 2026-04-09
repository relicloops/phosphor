# Phosphor Bugs Audit

Run time: `2026-04-09T05-09-51Z`

Scope:
- Read-only audit of `README.md`, `meson.build`, `meson.options`, `src/`, and `include/`
- Focused on new findings not already captured in automation memory from `2026-04-09T02-08-22Z`
- `audit-fix` skill was not available in this session, so this was a manual audit

Repo state:
- `main` is ahead of `origin/main` by 26 commits
- Working tree was otherwise clean

## Findings

### 1. High: legacy `clean` mode ignores `--dry-run` and `--wipe`

Refs:
- `src/commands/clean_cmd.c:94`
- `src/commands/clean_cmd.c:165`
- `src/commands/clean_cmd.c:178`
- `src/commands/clean_cmd.c:234`

Why this matters:
- `clean_via_scripts()` only receives `project_root_abs` and always runs `scripts/_default/clean.sh`.
- The branch into legacy mode happens before the native implementation consumes `wipe` or `dry-run`.
- `use_legacy_scripts()` is true either when the binary was compiled with `PHOSPHOR_SCRIPT_FALLBACK` or when the caller passes `--legacy-scripts`.

Impact:
- `phosphor clean --legacy-scripts --dry-run` can perform real deletions instead of a preview.
- `phosphor clean --legacy-scripts --wipe` does not guarantee the documented wider delete set, because the flag never reaches the script.
- In builds compiled with `-Dscript_fallback=true`, this broken behavior becomes the default path for every non-`--stale` clean run.

### 2. High: legacy `build` mode bypasses the project-root deploy containment guard

Refs:
- `src/commands/build_cmd.c:148`
- `src/commands/build_cmd.c:271`
- `src/commands/build_cmd.c:344`
- `src/commands/build_cmd.c:448`
- `README.md:163`
- `README.md:165`

Why this matters:
- The native build path rejects deploy targets that escape the project root.
- The legacy branch returns before those checks run and passes `deploy_at_abs` straight to `scripts/_default/all.sh` as `--public`.
- The runtime `--legacy-scripts` flag enables this path even though the README says the legacy fallback is only available when compiled with `-Dscript_fallback=true`.

Impact:
- `phosphor build --legacy-scripts --deploy-at=/tmp/outside` can write or clean outside the project tree.
- This directly violates the documented guarantee that deploy targets are containment-checked before writes happen.

### 3. Medium: `doctor` and `rm` treat `phosphor.toml` as a manifest even though the docs define it as project config

Refs:
- `README.md:56`
- `README.md:72`
- `src/commands/doctor_cmd.c:29`
- `src/commands/rm_cmd.c:66`
- `src/commands/build_cmd.c:372`
- `src/commands/serve_cmd.c:188`
- `src/commands/certs_cmd.c:600`

Why this matters:
- The README documents `.phosphor.toml` and `phosphor.toml` as walk-up config files used for variable resolution.
- `doctor` reports `phosphor.toml` as a valid phosphor manifest, and `rm` drops its manifest safety check if `phosphor.toml` exists.
- The actual runtime commands that need project manifest data still only load `template.phosphor.toml`.

Impact:
- A directory with only project config can pass `doctor`'s manifest check and `rm`'s safety gate.
- The same directory will still be treated as missing manifest data by `build`, `serve`, and `certs`.
- This creates a misleading safety model around destructive commands.

### 4. Medium: `serve` prints the wrong HTTPS URL because it omits the active port

Refs:
- `src/commands/serve_cmd.c:625`
- `src/commands/serve_cmd.c:642`
- `src/commands/serve_cmd.c:684`
- `src/commands/serve_cmd.c:694`

Why this matters:
- The startup URL is formatted as `https://<host>` while the actual server port is tracked separately and defaults to `9443`.
- The same truncated URL is reused in the ncurses dashboard info panel.

Impact:
- The banner and dashboard point users at port 443 instead of the real dev server.
- On the default configuration, the clickable/openable URL is wrong on every run.

### 5. Medium: recursive directory writes still follow symlinks during copy/render walks

Refs:
- `src/io/fs_copytree.c:107`
- `src/template/writer.c:419`
- `src/platform/posix/fs_posix.c:81`

Why this matters:
- `ph_fs_copytree()` and `rendertree_recurse()` both write descendants through `ph_fs_write_file()`.
- `ph_fs_write_file()` opens with `O_WRONLY | O_CREAT | O_TRUNC`, so it follows symlinks.
- Single-file template ops gained execute-time containment re-checks, but recursive descendant writes did not.

Impact:
- A concurrent symlink swap inside a deploy or staging tree can redirect writes outside the intended root.
- This is harder to exploit than the legacy build/clean issues because it needs local filesystem races, but it is still a real write-escape surface.

## Notes

- I did not repeat earlier memory findings about the documented `[variables]` config format, `manifest.toml` support, `ph_serve_wait()` signal mapping, unescaped esbuild define injection, or `doctor`'s global `esbuild` check.
- No repository source files were modified during this run. Only this audit report and automation memory were updated.

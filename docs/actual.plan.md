# Plan: Update docs + commit all pending changes

## Pre-commit: Update docs, help, website

Before committing, these files need updating:
- `src/dashboard/db_popup.c` -- help popup already updated
- `docs/source/reference/dashboard-event-loop.rst` -- already updated
- `website/src/pages/DashboardManual.tsx` -- already updated
- `website/src/pages/DashboardImplementation.tsx` -- already updated
- Regenerate AI-History: `just ai-history` from `website/`
- Rebuild website: `just build` from `website/`

## Commit Plan (9 commits, SoC-ordered)

### Commit 1: cJSON vendor + ph_json wrapper

```
feat(json): vendor cJSON v1.7.18 and add ph_json wrapper API

Add cJSON as a Meson cmake subproject with PHOSPHOR_HAS_CJSON compile
flag. Create include/phosphor/json.h and src/core/json.c providing
ph_json_parse, ph_json_parse_file, ph_json_get_string,
ph_json_get_object, ph_json_add_object, ph_json_add_string,
ph_json_print, ph_json_destroy -- thin wrappers over cJSON following
the ph_ naming convention.
```

Files:
- `subprojects/cjson.wrap`
- `subprojects/cjson/` (vendored)
- `include/phosphor/json.h`
- `src/core/json.c`
- `meson.build` (cjson dep + json.c source)

### Commit 2: ACME migration to ph_json

```
refactor(certs): migrate ACME module from hand-rolled JSON to ph_json

Replace all json_extract_string/json_extract_string_array calls with
ph_json_parse/ph_json_get_string across 4 ACME files. Remove the old
acme_json.h and acme_json.c. Zero internal headers remain in src/.
```

Files:
- `src/certs/acme_account.c`
- `src/certs/acme_order.c`
- `src/certs/acme_challenge.c`
- `src/certs/acme_finalize.c`
- `src/certs/acme_json.c` (deleted)
- `src/certs/acme_json.h` (deleted)

### Commit 3: Dashboard refactor + TUI features

```
feat(dashboard): refactor into granular files with cursor, selection,
search, save, fuzzy finder, JSON viewer, and popup system

Split monolithic dashboard.c into 15 db_*.c files with single
responsibility each. Add line cursor with auto-follow, vim-style
visual select (v), JSON export (V), :save/:saveall/:clear commands,
fuzzy log finder (g) with file picker, JSON viewer popup with tree
folding, inline JSON fold (z), viewport freeze, and 6-type popup
system.
```

Files:
- `src/dashboard/db_types.h`
- `src/dashboard/db_ring.c`
- `src/dashboard/db_layout.c`
- `src/dashboard/db_draw.c`
- `src/dashboard/db_popup.c`
- `src/dashboard/db_evt_pipe.c`
- `src/dashboard/db_evt_signal.c`
- `src/dashboard/db_evt_child.c`
- `src/dashboard/db_evt_key.c`
- `src/dashboard/db_evt_tick.c`
- `src/dashboard/db_event.c`
- `src/dashboard/db_lifecycle.c`
- `src/dashboard/db_fuzzy.c`
- `src/dashboard/db_json_fold.c`
- `src/dashboard/dashboard.c`
- `include/phosphor/dashboard.h`
- `meson.build` (dashboard source entries)

### Commit 4: Panel tabs (neonsignal live-stream / debug-stream)

```
feat(dashboard): add per-panel tabs with stdout/stderr stream routing

Introduce db_tab_t struct with per-tab ring buffer, scroll, cursor,
selection, and fold state. Accessor inlines (panel_ring, panel_scroll,
etc.) transparently resolve to active tab or legacy inline fields.
feed_accum_multi routes completed lines to matching tabs by source.
Neonsignal panel gets live-stream (stdout) and debug-stream (stderr)
tabs switchable with 1/2 keys. Non-tabbed panels unchanged.
```

Files:
- `src/dashboard/db_types.h`
- `include/phosphor/dashboard.h`
- `src/dashboard/dashboard.c`
- `src/dashboard/db_ring.c`
- `src/dashboard/db_evt_pipe.c`
- `src/dashboard/db_draw.c`
- `src/dashboard/db_evt_key.c`
- `src/dashboard/db_lifecycle.c`
- `src/dashboard/db_json_fold.c`
- `src/dashboard/db_fuzzy.c`
- `src/commands/serve_cmd.c`
- `src/dashboard/db_popup.c`

### Commit 5: Neonsignal logging flags

```
feat(serve): add neonsignal logging flags to serve pipeline

Add 6 new flags (--enable-debug, --enable-log, --enable-log-color,
--enable-file-log, --log-directory, --disable-proxies-check) flowing
from template.phosphor.toml [serve.neonsignal] through serve config
to neonsignal spawn argv. Three-tier resolution: CLI > manifest >
default.
```

Files:
- `include/phosphor/serve.h`
- `include/phosphor/manifest.h`
- `src/template/manifest_load.c`
- `src/commands/phosphor_commands.c`
- `src/commands/serve_cmd.c`
- `src/serve/serve.c`

### Commit 6: Embedded shell panel

```
feat(dashboard): add embedded shell panel with PTY-backed command
execution, views, and screen overlays

Introduce DB_MODE_SHELL with db_shell.c handling PTY spawning via
posix_openpt, view tabs (up to 4), screen popups for command output,
and integrated event loop polling of PTY master fds. Shell opens with
Ctrl+P, closes with Ctrl+Q, views cycle with Ctrl+B/R, screens
navigate with Ctrl+N, minimize with Ctrl+X, save with Ctrl+S.
```

Files:
- `src/dashboard/db_types.h`
- `src/dashboard/db_shell.c`
- `src/dashboard/dashboard.c`
- `src/dashboard/db_layout.c`
- `src/dashboard/db_event.c`
- `src/dashboard/db_evt_key.c`
- `src/dashboard/db_evt_child.c`
- `src/dashboard/db_draw.c`
- `src/dashboard/db_lifecycle.c`
- `src/dashboard/db_popup.c`
- `meson.build`

### Commit 7: Website dashboard pages + nav dropdown

```
feat(website): add dashboard landing, manual, and implementation pages
with NavDropdown component

Create Dashboard.tsx landing with card links, DashboardManual.tsx with
16-section granular user guide using initScrollTracker,
DashboardImplementation.tsx replacing DashboardArchitecture with
updated content. Add reusable NavDropdown.tsx hover dropdown for
header navigation. Update app.tsx routes and .cathode route registry.
```

Files:
- `website/src/pages/Dashboard.tsx`
- `website/src/pages/DashboardManual.tsx`
- `website/src/pages/DashboardImplementation.tsx`
- `website/src/pages/DashboardArchitecture.tsx`
- `website/src/components/NavDropdown.tsx`
- `website/src/components/Header.tsx`
- `website/src/app.tsx`
- `website/src/static/css/pages/dashboard.css`
- `website/src/static/css/pages/dashboard-manual.css`
- `website/src/static/css/pages/dashboard-architecture.css`
- `website/src/static/css/components/nav-dropdown.css`
- `website/src/static/dashboard.html`
- `website/src/static/dashboard-manual.html`
- `website/src/static/dashboard-implementation.html`
- `website/src/static/.cathode`

### Commit 8: Website manifest logging config

```
chore(website): enable neonsignal logging features in manifest

Add enable_debug, enable_log, enable_log_color, enable_file_log,
and log_directory settings to [serve.neonsignal] section of the
website template.phosphor.toml.
```

Files:
- `website/template.phosphor.toml`

### Commit 9: Docs, reference, plans, AI-History

```
docs: update plans, reference docs, and AI-History for dashboard
tabs, logging flags, and embedded shell

Update dashboard-event-loop.rst with panel tabs, embedded shell,
and DB_MODE_SHELL sections. Update summary plan with new completed
entries. Rename soc-audit plan to COMPLETED. Regenerate AI-History
and rebuild website.
```

Files:
- `docs/source/reference/dashboard-event-loop.rst`
- `docs/source/plans/summary.[ACTIVE].rst`
- `docs/source/plans/soc-audit-json-consolidation.[COMPLETED].rst`
- `docs/actual.plan.md`
- `website/src/static/ai-history.json`

## Excluded from commits

Per commit-guidelines, never stage:
- `.claude/` directory
- `CLAUDE.md`, `GEMINI.md`, `AGENTS.md`

Also exclude generated/test data:
- `website/04.04.2026.*.json`
- `website/05.04.2026.*.json`
- `website/phosphor.*.json`
- `website/log/`
- `website/log.x`
- `website/package-lock.json` (unless intentional)

## Verification

After all commits:
1. `ninja -C build` -- zero warnings
2. `./build/phosphor version` -- smoke test
3. `git log --oneline -9` -- verify commit messages
4. `git diff HEAD~9..HEAD --stat` -- verify scope

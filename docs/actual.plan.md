# Phosphor 2026-04-08T11-07-17Z Bugs Audit Remediation Plan

## Context

A new codex audit landed at
`.claude/codex-audit-fix/2026-04-08T11-07-17Z.bugs.audit.md`. This is
a static source audit that reports 5 concrete defects (2 HIGH, 3
MEDIUM) plus README drift. All findings were re-verified against the
tree during Phase 1 exploration. No bug is a false positive.

The goal is to fix the correctness issues in `src/` and resolve the
README drift in a single remediation pass without changing any
public API contracts or introducing new Meson options.

---

## Verified Findings

### 1. HIGH -- ACME and local leaf pipelines drop `domain.name`

**Sites:**
- `src/commands/certs_cmd.c:222-227` (`ph_acme_order_create` call
  passes only `d->san` / `d->san_count`).
- `src/commands/certs_cmd.c:283-287` (`ph_acme_finalize` call, same
  bug).
- `src/certs/certs_leaf.c:141-147` (SAN cnf only written when
  `san_count > 0`).
- `src/certs/certs_leaf.c:168` (`-config` is `/dev/null` when
  `san_count == 0`).
- `src/certs/certs_leaf.c:206-211` (`-extfile` / `-extensions v3_req`
  gated on `san_count > 0`).

**Downstream reject points:**
- `src/certs/acme_order.c:24-29` -- returns `PH_ERR_INTERNAL` on
  `domain_count == 0`.
- `src/certs/acme_finalize.c:32-39` -- returns `PH_ERR_INTERNAL` on
  `domain_count == 0`.
- `src/certs/acme_finalize.c:81-86` -- calls `ph_cert_san_write_cnf`
  with the same domains array.
- `src/certs/certs_san.c:34-43` -- `ph_cert_san_write_cnf` rejects
  `san_count == 0` with `PH_ERR_INTERNAL`.
- `src/certs/acme_finalize.c:90` and `src/certs/certs_leaf.c:154` --
  both set CSR subject to `/CN=<domains[0]>` or `/CN=<d->name>`, so
  `domains[0]` must be the primary.

**Consequence:**
- Manifests like `[[certs.domains]] name = "example.com"` (no `san`)
  fail in Let's Encrypt because `domain_count == 0`.
- Manifests with `san` but without `name` in the san list generate
  leaf certs whose SANs do not include the primary name.
- Without SAN entries, modern TLS clients reject the leaf cert.

### 2. HIGH -- `phosphor certs --generate --domain=X` ignores the filter

**Site:** `src/commands/certs_cmd.c:484-492` -- hard-codes `NULL`
for the domain filter in both `run_local` and `run_letsencrypt`
dispatch calls despite reading `domain_filter` at line 420.

**Consequence:**
- Every configured domain is processed.
- LE rate-limit risk on accidental bulk renewals.
- Local leaf certs overwritten even when the user explicitly scoped
  the action to one domain.

**Edge case:** `run_local` already errors out with `"no local domain
matching"` if `domain_filter && generated == 0`. A filter that
matches an LE-only domain would otherwise trip that error during the
`--generate` dispatch. The fix must detect the target domain's mode
upfront and skip the non-matching pipeline.

### 3. MEDIUM -- serve preflight rejects bare PATH binary names

**Sites:**
- `src/serve/serve.c:71-87` (`ns.bin_path` branch) and
  `src/serve/serve.c:89-107` (`redir.bin_path` branch) use `access()`
  whenever `bin_path != NULL`.
- `src/commands/serve_cmd.c:62-69` (`anchor_serve_path`) and
  `src/commands/serve_cmd.c:250-254` / `385-390`
  (`cfg.ns.bin_path = derived.ns_bin ? derived.ns_bin : raw`) both
  intentionally leave bare names unanchored so PATH lookup can run
  later.

**Consequence:** `--neonsignal-bin=neonsignal` and manifest values
like `bin = "neonsignal"` fail preflight because `access("neonsignal",
X_OK)` looks in the current working directory, not PATH. The inline
comment at `serve_cmd.c:246-249` and README `lines 89-90` both claim
PATH-based preflight, which is currently wrong for any configured
bare name.

### 4. MEDIUM -- serve log-dir bootstrap non-recursive, silent on failure

**Site:** `src/commands/serve_cmd.c:481-489` -- three raw `mkdir()`
calls whose return values are discarded. The top-level
`cfg.ns.log_directory` (already anchored at
`serve_cmd.c:361-370`) may be nested (e.g. `var/log/phosphor`), so
the parent component may not exist when step 7b runs.

**Consequence:** Nested `log_directory` values silently fail to
create. Downstream, neonsignal launches with `--enable-file-log`
configured but without the log tree that was supposed to exist.
Permission / `ENAMETOOLONG` / `ENOSPC` failures are equally
invisible.

### 5. MEDIUM -- `filament` returns success while printing "not yet implemented"

**Site:** `src/commands/filament_cmd.c:8-23`. Handler prints
`"filament: not yet implemented"` and then returns `0`.

**Consequence:** Scripts, CI, and users cannot distinguish the
placeholder from a successful command. A non-zero exit is the only
honest signal.

### 6. README drift

- `README.md:173-178` -- LE examples missing `--letsencrypt`.
- `README.md:183` -- documents `--output-dir=<path>`; real flag is `--output`.
- `README.md:17-29` -- command list omits `filament`.
- `README.md:68-71` -- implies ncurses is unconditional; it is
  system-first with wrap fallback.
- `README.md:89-90` -- PATH preflight claim depends on Fix 3.

---

## Recommended Fixes

### Fix 1 -- Effective-SAN helper + application (Finding 1)

Add a public helper in `include/phosphor/certs.h` and implement
it in `src/certs/certs_san.c`:

```c
ph_result_t ph_cert_domain_effective_sans(
    const ph_cert_domain_t *domain,
    char ***out_list,
    size_t *out_count,
    ph_error_t **err);

void ph_cert_domain_sans_free(char **list, size_t count);
```

The helper allocates a heap list with `domain->name` at index 0
followed by each unique entry from `domain->san` that does not
string-equal `domain->name`. List is guaranteed non-empty.

Apply in `src/commands/certs_cmd.c run_letsencrypt`: build the
effective list once per iteration, pass it to both
`ph_acme_order_create` and `ph_acme_finalize`, free at every
loop-exit path.

Apply in `src/certs/certs_leaf.c`: drop the `san_count > 0` gates
at lines 141, 168, and 206-211. Build the effective list
unconditionally via the helper. Always pass `cnf_path` to openssl
req and always append `-extfile/-extensions v3_req` to openssl
x509.

No change to `ph_acme_order_create`, `ph_acme_finalize`, or
`ph_cert_san_write_cnf` themselves -- they remain strict about
`count > 0`; the fix is upstream.

### Fix 2 -- Honor `--generate --domain=X` (Finding 2)

Rewrite `src/commands/certs_cmd.c:484-492` to pre-resolve the
target domain's mode and dispatch only the matching pipeline:

```c
if (f_generate) {
    bool do_local = true;
    bool do_letsencrypt = true;

    if (domain_filter) {
        const ph_cert_domain_t *found = NULL;
        for (size_t i = 0; i < certs_cfg.domain_count; i++) {
            if (strcmp(certs_cfg.domains[i].name, domain_filter) == 0) {
                found = &certs_cfg.domains[i];
                break;
            }
        }
        if (!found) {
            ph_log_error("certs: no domain matching '%s' in manifest",
                         domain_filter);
            ph_certs_config_destroy(&certs_cfg);
            return PH_ERR_CONFIG;
        }
        do_local       = (found->mode == PH_CERT_LOCAL);
        do_letsencrypt = (found->mode == PH_CERT_LETSENCRYPT);
    }

    if (do_local) {
        exit_code = run_local(&certs_cfg, project_root, false,
                               domain_filter, f_dry_run, f_force);
    }
    if (exit_code == 0 && !ph_signal_interrupted() && do_letsencrypt) {
        exit_code = run_letsencrypt(&certs_cfg, project_root,
                                     domain_filter, "request",
                                     directory_url, f_dry_run, f_force);
    }
}
```

### Fix 3 -- Bare-name PATH preflight (Finding 3)

Add `static bool is_bare_name(const char *s)` to
`src/serve/serve.c`. Rewrite `ph_serve_check_binaries` to check
bareness before falling to `access()`, routing bare names to
`find_in_path`. Apply the pattern to both `ns.bin_path` and
`redir.bin_path` branches.

### Fix 4 -- Checked recursive log-dir bootstrap (Finding 4)

Replace the three raw `mkdir()` calls at
`src/commands/serve_cmd.c:481-489` with `ph_fs_mkdir_p` from
`include/phosphor/platform.h:44`. Treat each failure as
`PH_ERR_FS` with a clear error message and full cleanup.

### Fix 5 -- `filament` exits non-zero (Finding 5)

Replace `return 0;` with `return PH_ERR_GENERAL;` at
`src/commands/filament_cmd.c:22`. Keep the `printf` lines.

### Fix 6 -- README drift

Surgical edits to `README.md`:

1. Lines 173-178: rewrite LE examples to include `--letsencrypt`.
2. Line 183: rename `--output-dir` to `--output`.
3. Lines 17-29: add `filament` entry as `[experimental]`.
4. Lines 68-71: carve out ncurses from the "built unconditionally"
   claim.

---

## Critical files to modify

| File | Findings | Notes |
|------|----------|-------|
| `include/phosphor/certs.h` | 1 | add helper + free prototype |
| `src/certs/certs_san.c` | 1 | implement helper + free |
| `src/commands/certs_cmd.c` | 1, 2 | apply helper, rewrite `--generate` dispatch |
| `src/certs/certs_leaf.c` | 1 | remove `san_count > 0` gates, use helper |
| `src/serve/serve.c` | 3 | `is_bare_name` helper + preflight rewrite |
| `src/commands/serve_cmd.c` | 4 | swap `mkdir` for `ph_fs_mkdir_p`, fail-closed |
| `src/commands/filament_cmd.c` | 5 | return `PH_ERR_GENERAL` |
| `README.md` | 6 | certs examples, `--output`, `filament`, ncurses note |

## Implementation order

```
Fix 1 infra           -> add helper + free in certs_san.c / certs.h
Fix 1 application #1  -> certs_cmd.c run_letsencrypt (order + finalize)
Fix 1 application #2  -> certs_leaf.c (cnf + req + x509)
Fix 2                 -> certs_cmd.c --generate dispatch
Fix 3                 -> serve.c ph_serve_check_binaries
Fix 4                 -> serve_cmd.c step 7b
Fix 5                 -> filament_cmd.c return
Fix 6                 -> README.md
```

## Commit granularity

1. `fix(certs): always include primary domain name in CSR and ACME order`
2. `fix(certs): honor --domain filter in --generate dispatch`
3. `fix(serve): resolve bare binary names via PATH in preflight`
4. `fix(serve): use recursive mkdir_p for log directory bootstrap`
5. `fix(filament): return non-zero when command is not implemented`
6. `docs(readme): update certs examples, output flag, filament, ncurses`

## Verification

Static (safe, no build): grep-based checks on each fix site.
Dynamic (requires build; ask user first): meson build, version
smoke, `filament --path .` exit code, `certs --generate
--domain=nonexistent` error, cert dry-run SAN listing, serve
bare-name preflight, serve log-dir bootstrap.

## Out of scope

- Changing the `ph_acme_*` or `ph_cert_san_write_cnf` signatures.
- Meson graph changes for optional deps.
- Hiding `filament` from help.
- README rewording beyond the five drift items.

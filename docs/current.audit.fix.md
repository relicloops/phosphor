# Args Parser Audit

Run time: `2026-04-09T11-28-03Z`

Scope:
- `include/phosphor/args.h`
- `src/args-parser/lexer.c`
- `src/args-parser/parser.c`
- `src/args-parser/validate.c`
- `src/args-parser/spec.c`
- `src/args-parser/args_helpers.c`
- `src/args-parser/kvp.c`
- recent git history touching this area

Git comparison:
- `609f8c2` fixed the earlier `serve` toggle-name mismatch by converting the five logging/proxy flags to `PH_FORM_TOGGLE` and wiring `ph_args_is_enabled` / `ph_args_is_disabled`.
- `dd68429` added `var_name` mapping support and does not overlap with the findings below.
- The findings below are still present after `609f8c2`; they are follow-on gaps in validation and merge behavior, not duplicates of the already-fixed bug.

## Findings

### [P1] Toggle CLI flags cannot override manifest booleans in the opposite direction

Files:
- `include/phosphor/manifest.h:157-180`
- `src/commands/serve_cmd.c:391-411`

Why this is a bug:
- `ph_serve_manifest_config_t` explicitly documents that CLI flags override manifest defaults.
- The merge logic only checks one polarity for each toggle:
  - `debug`, `log`, `log-color`, and `file-log` use `ph_args_is_enabled(...) || manifest_value`
  - `proxies-check` uses `ph_args_is_disabled(...) || manifest_value`
- As a result, the opposite toggle is accepted by the parser but ignored during merge.

Concrete breakage:
- If the manifest sets `ns_enable_debug = true`, `phosphor serve --disable-debug` still leaves debug logging enabled.
- If the manifest sets `ns_enable_file_log = true`, `phosphor serve --disable-file-log` still enables file logging.
- If the manifest sets `ns_disable_proxies_check = true`, `phosphor serve --enable-proxies-check` cannot re-enable the health-check endpoint logging.

Impact:
- The advertised toggle contract is false for manifest-backed defaults.
- In the `file-log` case this can keep JSON logging enabled even when the operator explicitly asked to disable it.

### [P2] Toggle specs accept `--flag` and `--flag=value` even though help only advertises `--enable-flag` / `--disable-flag`

Files:
- `src/cli/cli_help.c:76-78`
- `src/args-parser/validate.c:72-120`
- `src/args-parser/args_helpers.c:25-44`
- `src/commands/serve_cmd.c:392-410`

Why this is a bug:
- Help prints toggle flags exclusively as `--enable-<name>` / `--disable-<name>`.
- Validation rejects toggle syntax on non-toggle specs, but it never performs the inverse check for toggle specs.
- For a toggle spec, both `--debug` and `--debug=1` survive validation.

Concrete breakage:
- `serve` only consumes toggle state through `ph_args_is_enabled` / `ph_args_is_disabled`.
- A parsed `PH_FLAG_BOOL` or `PH_FLAG_VALUED` entry for a toggle name is therefore silently ignored at runtime.
- Example: `phosphor serve --debug` is accepted, but `cfg.ns.enable_debug` remains false unless `--enable-debug` was used or the manifest already enabled it.

Impact:
- Users get a successful parse for a flag spelling that does nothing.
- This is especially risky immediately after `609f8c2`, because the new toggle form looks close enough to a normal boolean flag that operators are likely to try `--debug`.

### [P2] Integer flags are only syntax-checked, then parsed with `atoi`, so invalid ranges are silently coerced or overflow

Files:
- `src/args-parser/validate.c:112-119`
- `src/commands/serve_cmd.c:25-27`
- `src/commands/serve_cmd.c:288-301`
- `src/commands/serve_cmd.c:432-448`

Why this is a bug:
- `PH_TYPE_INT` validation accepts any signed digit string.
- `serve` then converts the value with `atoi`.
- Non-positive values are silently treated as "unset" by the later `t1 > 0 ? t1 : fallback` merge logic.
- Oversized values are unchecked and rely on `atoi`, which has implementation-defined / undefined overflow behavior.

Concrete breakage:
- `phosphor serve --port=0` parses successfully and silently falls back to the manifest/default port instead of rejecting the invalid value.
- `phosphor serve --threads=-1` parses successfully and is ignored instead of failing fast.
- Very large values such as `--redirect-port=999999999999` pass validation and then depend on libc overflow behavior before they are forwarded to child process config.

Impact:
- Operators do not get feedback for invalid numeric input.
- The runtime behavior depends on fallback defaults or overflow quirks rather than explicit validation.

## Notes

- I did not find evidence that `609f8c2` already fixed any of the three issues above.
- No source files were modified during this audit; only this report file was added.

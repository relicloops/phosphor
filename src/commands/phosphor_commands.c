#include "phosphor/commands.h"

/* ---- shared enum choice arrays ---- */

static const char *const eol_choices[] = {"lf", "crlf"};
static const char *const le_action_choices[] = {"request", "renew", "verify"};

/* ---- per-command flag spec tables (appendices A-E) ---- */

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Create command (appendix A)
 *
 * required/primary:  --name, --template
 * common optional:   --output, --tld, --owner, --owner-slug,
 *                    --github, --instagram, --x
 * action modifiers:  --force, --dry-run, --toml, --allow-hooks, --yes,
 *                    --allow-hidden, --verbose
 * valued optional:   --normalize-eol
 */
static const ph_argspec_t create_specs[] = {
    {"name", PH_TYPE_STRING, PH_FORM_VALUED, true, NULL, NULL, 0,
     "project name; used as destination folder"},
    {"template", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "template source (local path, git URL, or archive)"},
    {"output", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "custom output directory; overrides --name for destination"},
    {"tld", PH_TYPE_STRING, PH_FORM_VALUED, false, ".host", NULL, 0,
     "top-level domain passed to template variables"},
    {"owner", PH_TYPE_STRING, PH_FORM_VALUED, false, NULL, NULL, 0,
     "project owner name; passed to template variables"},
    {"owner-slug", PH_TYPE_STRING, PH_FORM_VALUED, false, NULL, NULL, 0,
     "slugified owner name; passed to template variables"},
    {"github", PH_TYPE_URL, PH_FORM_VALUED, false, NULL, NULL, 0,
     "GitHub URL; passed to template variables"},
    {"instagram", PH_TYPE_URL, PH_FORM_VALUED, false, NULL, NULL, 0,
     "Instagram URL; passed to template variables"},
    {"x", PH_TYPE_URL, PH_FORM_VALUED, false, NULL, NULL, 0,
     "X (Twitter) URL; passed to template variables"},
    {"force", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "overwrite existing destination files"},
    {"dry-run", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "print planned operations without creating files"},
    {"toml", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "output report in TOML format (reserved)"},
    {"allow-hooks", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "run post-create hooks defined in the template (reserved)"},
    {"yes", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "auto-confirm all prompts (reserved)"},
    {"normalize-eol", PH_TYPE_ENUM, PH_FORM_VALUED, false, NULL, eol_choices,
     2, "normalize line endings in rendered files (reserved)"},
    {"allow-hidden", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "include hidden (dot) files from the template (reserved)"},
    {"verbose", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "enable verbose output"},
    {"checksum", PH_TYPE_STRING, PH_FORM_VALUED, false, NULL, NULL, 0,
     "SHA256 checksum for archive template verification"},
};

/*
 * Build command (appendix B)
 */
static const ph_argspec_t build_specs[] = {
    {"project", PH_TYPE_STRING, PH_FORM_VALUED, false, NULL, NULL, 0,
     "project root directory; defaults to current directory"},
    {"deploy-at", PH_TYPE_STRING, PH_FORM_VALUED, false, NULL, NULL, 0,
     "custom deployment target directory"},
    {"clean-first", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "remove build/ and deploy directories before building"},
    {"tld", PH_TYPE_STRING, PH_FORM_VALUED, false, NULL, NULL, 0,
     "top-level domain; used in deploy path and esbuild defines"},
    {"strict", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "treat post-build warnings as errors"},
    {"toml", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "output build report in TOML format"},
    {"verbose", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "enable debug-level logging"},
    {"normalize-eol", PH_TYPE_ENUM, PH_FORM_VALUED, false, NULL, eol_choices,
     2, "normalize line endings (reserved)"},
    {"legacy-scripts", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "use legacy shell scripts instead of native esbuild (deprecated)"},
};

/*
 * Clean command (appendix C)
 */
static const ph_argspec_t clean_specs[] = {
    {"stale", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "remove stale .phosphor-staging-* directories only"},
    {"wipe", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "remove build/, public/, and node_modules/"},
    {"project", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "project root directory; defaults to current directory"},
    {"dry-run", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "preview what would be deleted without removing"},
    {"verbose", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "enable debug-level logging"},
    {"legacy-scripts", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "use legacy clean script instead of native cleanup (deprecated)"},
};

/*
 * Doctor command (appendix D)
 */
static const ph_argspec_t doctor_specs[] = {
    {"project", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "project root directory; defaults to current directory"},
    {"verbose", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "enable debug-level logging"},
    {"toml", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "output diagnostic report in TOML format (reserved)"},
};

/*
 * Rm command (appendix F)
 */
static const ph_argspec_t rm_specs[] = {
    {"specific", PH_TYPE_PATH, PH_FORM_VALUED, true, NULL, NULL, 0,
     "path to remove (relative to --project or cwd)"},
    {"project", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "project root directory; defaults to current directory"},
    {"force", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "remove even if no phosphor manifest is found in project root"},
    {"verbose", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "enable debug-level logging"},
};

/*
 * Certs command (appendix G)
 */
static const ph_argspec_t certs_specs[] = {
    {"generate", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "generate all certs from manifest (CA + local + LE)"},
    {"local", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "local CA + leaf certs only"},
    {"letsencrypt", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "Let's Encrypt mode only"},
    {"ca-only", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "generate root CA only (combine with --local)"},
    {"domain", PH_TYPE_STRING, PH_FORM_VALUED, false, NULL, NULL, 0,
     "target a specific domain by name"},
    {"action", PH_TYPE_ENUM, PH_FORM_VALUED, false, NULL,
     le_action_choices, 3,
     "LE action: request (default), renew, verify"},
    {"project", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "project root directory; defaults to current directory"},
    {"output", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "override certs output directory"},
    {"dry-run", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "print operations without executing"},
    {"force", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "overwrite existing cert files"},
    {"verbose", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "enable debug-level logging"},
    {"staging", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "use Let's Encrypt staging endpoint (no rate limits)"},
};

#ifdef PHOSPHOR_HAS_EMBEDDED
/*
 * Glow command -- scaffold from embedded cathode-landing template
 */
static const ph_argspec_t glow_specs[] = {
    {"name", PH_TYPE_STRING, PH_FORM_VALUED, true, NULL, NULL, 0,
     "project name; used as destination folder and template variable"},
    {"output", PH_TYPE_PATH, PH_FORM_VALUED, false, NULL, NULL, 0,
     "custom output directory; overrides --name for destination"},
    {"description", PH_TYPE_STRING, PH_FORM_VALUED, false,
     "A Cathode JSX website", NULL, 0,
     "project description; passed to template variables"},
    {"github-url", PH_TYPE_URL, PH_FORM_VALUED, false,
     "https://github.com", NULL, 0,
     "GitHub URL; passed to template variables"},
    {"force", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "overwrite existing destination files"},
    {"dry-run", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "print planned operations without creating files"},
    {"verbose", PH_TYPE_BOOL, PH_FORM_ACTION, false, NULL, NULL, 0,
     "enable verbose output"},
};
#endif

/* version and help have no flags (appendix E) */

/* ---- command table ---- */

static const ph_cmd_def_t phosphor_commands[] = {
    {"create", PHOSPHOR_CMD_CREATE, create_specs, ARRAY_LEN(create_specs),
     false, "scaffold a new project from a template"},
    {"build", PHOSPHOR_CMD_BUILD, build_specs, ARRAY_LEN(build_specs),
     false, "bundle and deploy a Cathode JSX project via esbuild"},
    {"clean", PHOSPHOR_CMD_CLEAN, clean_specs, ARRAY_LEN(clean_specs),
     false, "remove build artifacts and stale staging directories"},
    {"rm", PHOSPHOR_CMD_RM, rm_specs, ARRAY_LEN(rm_specs),
     false, "remove a specific path within the project"},
    {"certs", PHOSPHOR_CMD_CERTS, certs_specs, ARRAY_LEN(certs_specs),
     false, "generate TLS certificates (local CA or Let's Encrypt)"},
    {"doctor", PHOSPHOR_CMD_DOCTOR, doctor_specs, ARRAY_LEN(doctor_specs),
     false, "run project diagnostics"},
#ifdef PHOSPHOR_HAS_EMBEDDED
    {"glow", PHOSPHOR_CMD_GLOW, glow_specs, ARRAY_LEN(glow_specs),
     false, "scaffold a Cathode landing page from embedded template"},
#endif
    {"version", PHOSPHOR_CMD_VERSION, NULL, 0, false,
     "print phosphor version"},
    {"help", PHOSPHOR_CMD_HELP, NULL, 0, true,
     "show help for a command"},
};

/* ---- exported configuration ---- */

const ph_cli_config_t phosphor_cli_config = {
    .tool_name = "phosphor",
    .commands = phosphor_commands,
    .command_count = ARRAY_LEN(phosphor_commands),
};

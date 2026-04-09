/* phosphor args-parser diagram -- interactive behavior */

const details = {
  header: {
    title: "include/phosphor/args.h",
    body: "Central header defining all args-parser types and the public API.\n\nTypes: ph_token_stream_t, ph_parsed_args_t, ph_cli_config_t,\nph_argspec_t, ph_cmd_def_t, ph_kvp_node_t\n\nError subcodes UX001..UX007 are defined here but only\nused within src/args-parser/ -- no external file references them."
  },
  lexer: {
    title: "src/args-parser/lexer.c  [stage 1]",
    body: "Tokenizes raw argv[] into a ph_token_stream_t.\n\nRecognizes five token kinds:\n  --flag=value   PH_TOK_VALUED_FLAG\n  --flag         PH_TOK_BOOL_FLAG\n  --enable-x     PH_TOK_ENABLE_FLAG\n  --disable-x    PH_TOK_DISABLE_FLAG\n  bare word      PH_TOK_POSITIONAL\n\nRejects short flags (-x) and bare --.\nValidates identifiers: letter { letter | digit | - | _ }.\nAppends PH_TOK_END sentinel."
  },
  parser: {
    title: "src/args-parser/parser.c  [stage 2]",
    body: "Consumes the token stream and produces ph_parsed_args_t.\n\nExpects first positional = command name, matched against\nph_cli_config_t command table.\n\nEnforces:\n  - no duplicate valued/bool flags (UX003)\n  - no conflicting enable+disable for same name (UX004)\n  - positional only if cmd_def->accepts_positional\n\nAll strings are heap-duplicated; ph_parsed_args_destroy() frees."
  },
  validator: {
    title: "src/args-parser/validate.c  [stage 3]",
    body: "Type-checks each parsed flag against its ph_argspec_t.\n\nChecks per type:\n  PH_TYPE_INT   -- is_integer_string\n  PH_TYPE_URL   -- http:// or https:// prefix\n  PH_TYPE_PATH  -- rejects .. traversal\n  PH_TYPE_KVP   -- full ph_kvp_parse round-trip\n  PH_TYPE_ENUM  -- value in choices array\n\nAlso enforces required flags (spec.required == true).\nAction flags reject =value assignment."
  },
  spec: {
    title: "src/args-parser/spec.c",
    body: "Config-driven lookup functions over the static\nph_cli_config_t structure.\n\nph_arg_type_name()       -- enum to string\nph_cmd_def_name()        -- command_id to name\nph_cmd_def_specs()       -- command_id to spec array\nph_cmd_def_spec_lookup() -- command_id + flag_name to spec\n\nUsed by cli_help.c (render help text) and\nvar_merge.c (walk specs for variable precedence)."
  },
  kvp: {
    title: "src/args-parser/kvp.c",
    body: "Recursive descent parser for nested key-value syntax.\n\nFormat: !key:value|key:{nested|key2:val2}\n  !  -- mandatory prefix\n  :  -- key-value separator\n  |  -- sibling separator\n  {} -- nesting (max depth 8)\n\nScalar types (precedence order):\n  1. Quoted string: \"hello \\\"world\\\"\"\n  2. Bool literal:  true / false\n  3. Integer:       -42, 0, 100\n  4. Bare token:    anything else\n\nDuplicate keys at same depth are rejected."
  },
  helpers: {
    title: "src/args-parser/args_helpers.c",
    body: "Two convenience functions for command handlers:\n\nph_args_get_flag(args, name)\n  Returns the flag's value string, or NULL.\n\nph_args_has_flag(args, name)\n  Returns true if any flag with that name exists.\n\nEvery command handler uses these -- they are the\nprimary consumer-facing API of the args-parser."
  },
  main: {
    title: "src/main.c  [pipeline owner]",
    body: "Owns the complete args-parser pipeline:\n\n  1. ph_lexer_tokenize(argc, argv)\n  2. ph_parser_parse(config, tokens)\n  3. ph_validate(config, parsed_args)\n  4. dispatch to command handler\n  5. ph_parsed_args_destroy + ph_token_stream_destroy\n\nThis is the only file that calls the pipeline functions.\nAll other consumers receive already-parsed results."
  },
  "cli-h": {
    title: "include/phosphor/cli.h",
    body: "Declares the dispatch interface:\n  ph_cli_dispatch(config, args)\n\nIncludes args.h to use ph_cli_config_t and\nph_parsed_args_t in function signatures."
  },
  dispatch: {
    title: "src/cli/cli_dispatch.c",
    body: "Routes parsed command_id to the correct handler function.\n\nReceives (ph_cli_config_t *, ph_parsed_args_t *) from main.c\nand calls the matching command handler (create, build, etc.)."
  },
  help: {
    title: "src/cli/cli_help.c",
    body: "Generates human-readable help output.\n\nIterates ph_cmd_def_t entries for command listing.\nIterates ph_argspec_t arrays for per-command flag docs.\nUses ph_arg_type_name() to display flag types.\n\nThis is one of two external consumers of spec.c\n(the other being var_merge.c)."
  },
  "cmds-h": {
    title: "include/phosphor/commands.h",
    body: "Declares:\n  - extern ph_cli_config_t (the global config)\n  - command handler function signatures\n    e.g. ph_cmd_create(config, args)\n\nAll handlers share the same signature:\n  ph_result_t handler(const ph_cli_config_t *,\n                      const ph_parsed_args_t *,\n                      ph_error_t **)"
  },
  registry: {
    title: "src/commands/phosphor_commands.c",
    body: "The command registry -- defines all static data:\n\n  - ph_argspec_t tables for each command\n    (name, type, form, required, default, choices)\n  - ph_cmd_def_t array (name, id, specs, positional)\n  - The exported ph_cli_config_t struct\n\nThis is the single source of truth for what flags\neach command accepts."
  },
  create:   { title: "create_cmd.c",   body: "Scaffold project from external template.\nReads: name, template, output, checksum\nChecks: dry-run, force, verbose, toml" },
  build:    { title: "build_cmd.c",    body: "esbuild bundle + deploy.\nReads: project, deploy-at, tld\nChecks: clean-first, verbose, strict, toml, legacy-scripts" },
  clean:    { title: "clean_cmd.c",    body: "Remove build artifacts + staging.\nChecks: stale, wipe, dry-run, verbose" },
  rm:       { title: "rm_cmd.c",       body: "Sandboxed path removal.\nReads: specific, project\nChecks: force, verbose" },
  certs:    { title: "certs_cmd.c",    body: "TLS certificate generation (local CA + ACME).\nReads: domain, action, project, output\nChecks: generate, local, letsencrypt, ca-only,\n        dry-run, force, verbose, staging" },
  doctor:   { title: "doctor_cmd.c",   body: "Project diagnostics.\nReads: project\nChecks: verbose, toml" },
  glow:     { title: "glow_cmd.c",     body: "Embedded cathode-landing scaffold.\nReads: name, output, description, github-url\nChecks: force, dry-run, verbose" },
  serve:    { title: "serve_cmd.c",    body: "Dev server -- highest flag count.\nReads: project, host, port, www-root, certs-root,\n       neonsignal-bin, redirect-bin, threads,\n       working-dir, upload-dir, augments-dir,\n       grafts-dir, log-directory, watch-cmd, +more\nChecks: verbose, no-redirect, watch, no-dashboard,\n        enable-debug, enable-log, +more" },
  filament: { title: "filament_cmd.c", body: "Reserved / experimental command.\nCurrently a no-op stub that receives args\nbut performs no action." },
  varmerge: { title: "src/template/var_merge.c", body: "Template variable precedence engine.\nThe only non-command consumer of args-parser.\n\nWalks ph_argspec_t via ph_cmd_def_specs() to discover\nwhich CLI flags map to template variables.\n\n4-level merge order:\n  1. CLI flags (highest priority)\n  2. Environment variables\n  3. Project config (phosphor.toml)\n  4. Manifest defaults (lowest priority)\n\nUses ph_args_get_flag() to extract values." }
};

/* connection graph: node -> nodes it connects to */
const connections = {
  header:   ["lexer", "parser", "validator", "spec", "kvp", "helpers", "main", "cli-h", "cmds-h"],
  lexer:    ["main"],
  parser:   ["main"],
  validator: ["main", "kvp"],
  spec:     ["help", "varmerge"],
  kvp:      ["validator"],
  helpers:  ["create", "build", "clean", "rm", "certs", "doctor", "glow", "serve", "varmerge"],
  main:     ["lexer", "parser", "validator", "dispatch"],
  "cli-h":  ["main", "dispatch", "help"],
  dispatch: ["create", "build", "clean", "rm", "certs", "doctor", "glow", "serve", "filament"],
  help:     ["spec"],
  "cmds-h": ["dispatch", "registry"],
  registry: ["dispatch"],
  create:   ["helpers"],
  build:    ["helpers"],
  clean:    ["helpers"],
  rm:       ["helpers"],
  certs:    ["helpers"],
  doctor:   ["helpers"],
  glow:     ["helpers"],
  serve:    ["helpers"],
  filament: [],
  varmerge: ["helpers", "spec"]
};

/* ---- interaction ---- */

const panel = document.getElementById("detail-panel");
const panelTitle = document.getElementById("detail-title");
const panelBody = document.getElementById("detail-body");
const closeBtn = document.getElementById("detail-close");

function getAllNodes() {
  return document.querySelectorAll("[data-node]");
}

function showDetail(key) {
  const d = details[key];
  if (!d) return;
  panelTitle.textContent = d.title;
  panelBody.textContent = d.body;
  panel.classList.add("open");
}

function hideDetail() {
  panel.classList.remove("open");
}

function highlightConnections(key) {
  const related = new Set();
  related.add(key);

  /* outgoing */
  if (connections[key]) {
    connections[key].forEach(function(n) { related.add(n); });
  }
  /* incoming */
  Object.keys(connections).forEach(function(src) {
    if (connections[src].indexOf(key) !== -1) {
      related.add(src);
    }
  });

  getAllNodes().forEach(function(el) {
    var nk = el.getAttribute("data-node");
    if (related.has(nk)) {
      el.classList.add("highlight");
      el.classList.remove("dimmed");
    } else {
      el.classList.remove("highlight");
      el.classList.add("dimmed");
    }
  });
}

function clearHighlights() {
  getAllNodes().forEach(function(el) {
    el.classList.remove("highlight", "dimmed");
  });
}

/* bind events */
getAllNodes().forEach(function(el) {
  var key = el.getAttribute("data-node");

  el.addEventListener("click", function() {
    showDetail(key);
  });

  el.addEventListener("mouseenter", function() {
    highlightConnections(key);
  });

  el.addEventListener("mouseleave", function() {
    clearHighlights();
  });
});

closeBtn.addEventListener("click", hideDetail);

document.addEventListener("keydown", function(e) {
  if (e.key === "Escape") hideDetail();
});

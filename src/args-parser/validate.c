#include "phosphor/alloc.h"
#include "phosphor/args.h"
#include "phosphor/path.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool is_valid_url(const char *s) {
  if (!s)
    return false;
  return strncmp(s, "https://", 8) == 0 || strncmp(s, "http://", 7) == 0;
}

static bool enum_matches(const ph_argspec_t *spec, const char *value) {
  if (!spec->choices || spec->choice_count == 0)
    return true;
  for (size_t i = 0; i < spec->choice_count; i++) {
    if (strcmp(spec->choices[i], value) == 0)
      return true;
  }
  return false;
}

/* ---- public API ---- */

ph_result_t ph_validate(const ph_cli_config_t *config,
                        const ph_parsed_args_t *args, ph_error_t **err) {
  if (!config || !args || !err)
    return PH_ERR;
  *err = NULL;

  /* step 1: validate each parsed flag against its spec */
  for (size_t i = 0; i < args->flag_count; i++) {
    const ph_parsed_flag_t *flag = &args->flags[i];

    /* for toggle flags, the spec name is the base name */
    const ph_argspec_t *spec =
        ph_cmd_def_spec_lookup(config, args->command_id, flag->name);

    if (!spec) {
      /* UX001: unknown flag */
      if (flag->kind == PH_FLAG_ENABLE) {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                                "unknown flag: --enable-%s", flag->name);
      } else if (flag->kind == PH_FLAG_DISABLE) {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                                "unknown flag: --disable-%s", flag->name);
      } else {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
                                "unknown flag: --%s", flag->name);
      }
      return PH_ERR;
    }

    /* UX002: action flag given =value */
    if (spec->form == PH_FORM_ACTION && flag->kind == PH_FLAG_VALUED) {
      *err = ph_error_createf(
          PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
          "flag --%s is an action flag and does not accept a value",
          flag->name);
      return PH_ERR;
    }

    /* UX002: valued spec given bare --flag (no =value) */
    if (spec->form == PH_FORM_VALUED && flag->kind == PH_FLAG_BOOL) {
      *err = ph_error_createf(
          PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
          "flag --%s requires a value (use --%s=<value>)",
          flag->name, flag->name);
      return PH_ERR;
    }

    /* UX001: toggle syntax on non-toggle spec */
    if (spec->form != PH_FORM_TOGGLE &&
        (flag->kind == PH_FLAG_ENABLE || flag->kind == PH_FLAG_DISABLE)) {
      *err = ph_error_createf(
          PH_ERR_USAGE, PH_UX001_UNKNOWN_FLAG,
          "flag --%s does not support --enable-/--disable- syntax",
          flag->name);
      return PH_ERR;
    }

    /* UX008: toggle spec given bare --flag or --flag=value */
    if (spec->form == PH_FORM_TOGGLE &&
        (flag->kind == PH_FLAG_BOOL || flag->kind == PH_FLAG_VALUED)) {
      *err = ph_error_createf(
          PH_ERR_USAGE, PH_UX008_TOGGLE_SYNTAX,
          "flag --%s is a toggle: use --enable-%s or --disable-%s",
          flag->name, flag->name, flag->name);
      return PH_ERR;
    }

    /* type checks only apply to valued flags */
    if (flag->kind == PH_FLAG_VALUED && flag->value) {

      /* UX002: reject empty assignment (--flag=) for all types */
      if (!*flag->value) {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
                                "flag --%s has an empty value", flag->name);
        return PH_ERR;
      }

      switch (spec->type) {

      case PH_TYPE_INT: {
        char *end;
        errno = 0;
        long long val = strtoll(flag->value, &end, 10);
        if (*end != '\0' || errno == ERANGE) {
          *err = ph_error_createf(PH_ERR_USAGE, PH_UX005_TYPE_MISMATCH,
                                  "flag --%s expects an integer, got: %s",
                                  flag->name, flag->value);
          return PH_ERR;
        }
        if (val < INT_MIN || val > INT_MAX) {
          *err = ph_error_createf(PH_ERR_USAGE, PH_UX005_TYPE_MISMATCH,
                                  "flag --%s: value %lld out of range (%d..%d)",
                                  flag->name, val, INT_MIN, INT_MAX);
          return PH_ERR;
        }
        break;
      }

      case PH_TYPE_URL:
        /* UX005: value must have http:// or https:// prefix */
        if (!is_valid_url(flag->value)) {
          *err =
              ph_error_createf(PH_ERR_USAGE, PH_UX005_TYPE_MISMATCH,
                               "flag --%s expects a URL (http:// or https://), "
                               "got: %s",
                               flag->name, flag->value);
          return PH_ERR;
        }
        break;

      case PH_TYPE_PATH:
        /* UX005: reject path traversal (..) -- absolute paths are allowed
         * because --template and --output legitimately use them.
         * command-specific path restrictions (e.g. deploy-at escape guard)
         * are enforced in the command handler, not the generic validator. */
        if (ph_path_has_traversal(flag->value)) {
          *err = ph_error_createf(PH_ERR_USAGE, PH_UX005_TYPE_MISMATCH,
                                  "flag --%s contains path traversal: %s",
                                  flag->name, flag->value);
          return PH_ERR;
        }
        break;

      case PH_TYPE_KVP: {
        /* UX006: value must parse as valid KVP */
        ph_kvp_node_t *nodes = NULL;
        size_t count = 0;
        ph_error_t *kvp_err = NULL;

        if (ph_kvp_parse(flag->value, &nodes, &count, &kvp_err) != PH_OK) {
          *err =
              ph_error_createf(PH_ERR_USAGE, PH_UX006_MALFORMED_KVP,
                               "flag --%s has malformed KVP value", flag->name);
          if (kvp_err) {
            ph_error_chain(*err, kvp_err);
          }
          return PH_ERR;
        }
        ph_kvp_destroy(nodes, count);
        break;
      }

      case PH_TYPE_ENUM:
        /* UX007: value must be in choices array */
        if (!enum_matches(spec, flag->value)) {
          *err = ph_error_createf(PH_ERR_USAGE, PH_UX007_ENUM_VIOLATION,
                                  "flag --%s: invalid value '%s'", flag->name,
                                  flag->value);
          return PH_ERR;
        }
        break;

      case PH_TYPE_STRING:
      case PH_TYPE_BOOL:
        break;
      }
    }
  }

  /* step 2: check all required flags are present */
  {
    size_t spec_count = 0;
    const ph_argspec_t *specs =
        ph_cmd_def_specs(config, args->command_id, &spec_count);

    for (size_t s = 0; s < spec_count; s++) {
      if (!specs[s].required)
        continue;

      bool found = false;
      for (size_t f = 0; f < args->flag_count; f++) {
        if (strcmp(args->flags[f].name, specs[s].name) != 0)
          continue;
        /* valued specs require an actual valued flag, not bare --flag */
        if (specs[s].form == PH_FORM_VALUED &&
            args->flags[f].kind != PH_FLAG_VALUED) {
          continue;
        }
        found = true;
        break;
      }
      if (!found) {
        *err = ph_error_createf(PH_ERR_USAGE, PH_UX002_MISSING_VALUE,
                                "missing required flag: --%s", specs[s].name);
        return PH_ERR;
      }
    }
  }

  return PH_OK;
}

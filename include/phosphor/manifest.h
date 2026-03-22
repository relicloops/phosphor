#ifndef PHOSPHOR_MANIFEST_H
#define PHOSPHOR_MANIFEST_H

#include "phosphor/types.h"
#include "phosphor/error.h"

/* resource limits */
#define PH_MAX_OPS       1024
#define PH_MAX_VARIABLES  128
#define PH_MAX_HOOKS       16

/* ---- variable type ---- */

typedef enum {
    PH_VAR_STRING,
    PH_VAR_BOOL,
    PH_VAR_INT,
    PH_VAR_ENUM,
    PH_VAR_PATH,
    PH_VAR_URL,
} ph_var_type_t;

const char *ph_var_type_name(ph_var_type_t type);
ph_var_type_t ph_var_type_from_str(const char *s);

/* ---- variable definition ---- */

/*
 * ph_var_def_t -- template variable schema.
 *
 * ownership: all string fields heap-allocated, freed by ph_manifest_destroy.
 */
typedef struct {
    char          *name;
    ph_var_type_t  type;
    bool           required;
    char          *default_val;
    char          *env;
    char          *pattern;
    int64_t        min;
    int64_t        max;
    bool           has_min;
    bool           has_max;
    char         **choices;
    size_t         choice_count;
    bool           secret;
} ph_var_def_t;

/* ---- operation kind ---- */

typedef enum {
    PH_OP_MKDIR,
    PH_OP_COPY,
    PH_OP_RENDER,
    PH_OP_CHMOD,
    PH_OP_REMOVE,
} ph_op_kind_t;

const char *ph_op_kind_name(ph_op_kind_t kind);
ph_result_t ph_op_kind_from_str(const char *s, ph_op_kind_t *out);

/* ---- operation definition ---- */

/*
 * ph_op_def_t -- single template operation.
 *
 * ownership: all string fields heap-allocated, freed by ph_manifest_destroy.
 */
typedef struct {
    char         *id;
    ph_op_kind_t  kind;
    char         *from;
    char         *to;
    char         *mode;
    bool          overwrite;
    char         *condition;
    bool          atomic;
    char         *newline;      /* "lf", "crlf", "keep", or NULL */
} ph_op_def_t;

/* ---- hook definition ---- */

/*
 * ph_hook_def_t -- lifecycle hook.
 *
 * ownership: all string fields and argv heap-allocated, freed by
 *            ph_manifest_destroy.
 */
typedef struct {
    char  *when;        /* "pre-create" or "post-create" */
    char **run;         /* argv array */
    size_t run_count;
    char  *cwd;
    char  *condition;
    bool   allow_failure;
} ph_hook_def_t;

/* ---- filters ---- */

/*
 * ph_filters_t -- file filtering configuration.
 *
 * ownership: all arrays heap-allocated, freed by ph_manifest_destroy.
 */
typedef struct {
    char  **exclude;
    size_t  exclude_count;
    char  **deny;
    size_t  deny_count;
    char  **exclude_regex;       /* PCRE2 patterns -- requires -Dpcre2=true */
    size_t  exclude_regex_count;
    char  **deny_regex;          /* PCRE2 patterns -- requires -Dpcre2=true */
    size_t  deny_regex_count;
    char  **binary_ext;
    size_t  binary_ext_count;
    char  **text_ext;
    size_t  text_ext_count;
} ph_filters_t;

/* ---- build configuration ---- */

#define PH_MAX_BUILD_DEFINES  64

typedef struct {
    char *name;         /* e.g. "__PHOSPHOR_DEV__" */
    char *env;          /* env var to read value from */
    char *default_val;  /* fallback if env unset */
} ph_build_define_t;

typedef struct {
    char               *entry;        /* entry point, NULL = default */
    ph_build_define_t  *defines;
    size_t              define_count;
    bool                present;      /* true if [build] found in TOML */
} ph_build_config_t;

/* ---- manifest metadata ---- */

typedef struct {
    int    schema;
    char  *id;
    char  *version;
} ph_manifest_meta_t;

/* ---- template metadata ---- */

typedef struct {
    char *name;
    char *source_root;
    char *description;
    char *min_phosphor;
    char *license;
} ph_template_meta_t;

/* ---- defaults ---- */

typedef struct {
    char  **keys;
    char  **values;
    size_t  count;
} ph_defaults_t;

/* ---- manifest (composite) ---- */

/*
 * ph_manifest_t -- complete parsed manifest.
 *
 * ownership: self owns all nested heap data. freed by ph_manifest_destroy.
 */
typedef struct {
    ph_manifest_meta_t  manifest;
    ph_template_meta_t  tmpl;
    ph_defaults_t       defaults;
    ph_var_def_t       *variables;
    size_t              variable_count;
    ph_filters_t        filters;
    ph_build_config_t   build;
    ph_op_def_t        *ops;
    size_t              op_count;
    ph_hook_def_t      *hooks;
    size_t              hook_count;
} ph_manifest_t;

/* ---- API ---- */

/*
 * ph_manifest_load -- read and parse a template.phosphor.toml file.
 * on success, caller must call ph_manifest_destroy(out).
 * on error: exit code 3 (parse/schema error) or 6 (version mismatch).
 */
ph_result_t ph_manifest_load(const char *path, ph_manifest_t *out,
                              ph_error_t **err);

void ph_manifest_destroy(ph_manifest_t *m);

#endif /* PHOSPHOR_MANIFEST_H */

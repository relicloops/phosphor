#ifndef PHOSPHOR_ARGS_H
#define PHOSPHOR_ARGS_H

#include "phosphor/types.h"
#include "phosphor/error.h"

/* ---- token types (lexer output) ---- */

typedef enum {
    PH_TOK_VALUED_FLAG,     /* --flag=value */
    PH_TOK_BOOL_FLAG,       /* --flag       */
    PH_TOK_ENABLE_FLAG,     /* --enable-x   */
    PH_TOK_DISABLE_FLAG,    /* --disable-x  */
    PH_TOK_POSITIONAL,      /* bare word    */
    PH_TOK_END,             /* sentinel     */
} ph_token_kind_t;

/*
 * ph_arg_token_t -- single lexer token.
 *
 * ownership:
 *   name  -- owner: parent token stream (heap-allocated)
 *   value -- owner: parent token stream (heap-allocated, nullable)
 */
typedef struct {
    ph_token_kind_t kind;
    char           *name;
    char           *value;
    int             argv_index;
} ph_arg_token_t;

/*
 * ph_token_stream_t -- dynamic array of tokens produced by the lexer.
 *
 * ownership:
 *   tokens -- owner: self (heap-allocated array)
 *   each token's name/value strings -- owner: self
 */
typedef struct {
    ph_arg_token_t *tokens;
    size_t          count;
    size_t          cap;
} ph_token_stream_t;

/* ---- lexer ---- */

ph_result_t ph_lexer_tokenize(int argc, const char *const *argv,
                               ph_token_stream_t *out,
                               ph_error_t **err);

void ph_token_stream_destroy(ph_token_stream_t *stream);

/* ---- parsed flag (parser output) ---- */

typedef enum {
    PH_FLAG_VALUED,     /* --flag=value             */
    PH_FLAG_BOOL,       /* --flag (action modifier) */
    PH_FLAG_ENABLE,     /* --enable-x               */
    PH_FLAG_DISABLE,    /* --disable-x              */
} ph_flag_kind_t;

/*
 * ph_parsed_flag_t -- single parsed flag.
 *
 * ownership:
 *   name  -- owner: parent parsed_args (heap-allocated)
 *   value -- owner: parent parsed_args (heap-allocated, nullable)
 */
typedef struct {
    ph_flag_kind_t  kind;
    char           *name;
    char           *value;
    int             argv_index;
} ph_parsed_flag_t;

/* ---- arg value types ---- */

typedef enum {
    PH_TYPE_STRING,
    PH_TYPE_INT,
    PH_TYPE_BOOL,
    PH_TYPE_ENUM,
    PH_TYPE_PATH,
    PH_TYPE_URL,
    PH_TYPE_KVP,
} ph_arg_type_t;

const char *ph_arg_type_name(ph_arg_type_t type);

/* ---- flag form (how the flag appears on the command line) ---- */

typedef enum {
    PH_FORM_VALUED,     /* --flag=value  (requires assignment) */
    PH_FORM_ACTION,     /* --flag        (bare presence = true) */
    PH_FORM_TOGGLE,     /* --enable-x / --disable-x            */
} ph_flag_form_t;

/*
 * ph_argspec_t -- typed specification for a single flag.
 *
 * ownership:
 *   all pointers reference static const data (no heap).
 */
typedef struct {
    const char        *name;
    ph_arg_type_t      type;
    ph_flag_form_t     form;
    bool               required;
    const char        *default_value;
    const char *const *choices;
    size_t             choice_count;
    const char        *description;
    const char        *var_name;    /* manifest variable name override (NULL = use flag name) */
} ph_argspec_t;

/* ---- command definition (provided by application) ---- */

/*
 * ph_cmd_def_t -- static command definition.
 *
 * ownership:
 *   all pointers reference static const data (no heap).
 */
typedef struct {
    const char          *name;              /* "create", "build", etc. */
    int                  id;                /* app-defined ID (>= 1), 0 = none */
    const ph_argspec_t  *specs;             /* pointer to static argspec array */
    size_t               spec_count;
    bool                 accepts_positional; /* true for "help <topic>" style */
    const char          *description;       /* one-line command summary */
} ph_cmd_def_t;

/*
 * ph_cli_config_t -- parser configuration (provided by application).
 *
 * ownership:
 *   all pointers reference static const data (no heap).
 */
typedef struct {
    const char          *tool_name;     /* for error messages: "phosphor" */
    const ph_cmd_def_t  *commands;      /* static array of command defs */
    size_t               command_count;
} ph_cli_config_t;

/*
 * ph_parsed_args_t -- complete parser output (data-only, no side effects).
 *
 * ownership:
 *   flags      -- owner: self (heap-allocated array)
 *   positional -- owner: self (heap-allocated, nullable)
 *   each flag's name/value -- owner: self
 */
typedef struct {
    int                command_id;
    char              *positional;
    ph_parsed_flag_t  *flags;
    size_t             flag_count;
    size_t             flag_cap;
} ph_parsed_args_t;

/* ---- parser ---- */

ph_result_t ph_parser_parse(const ph_cli_config_t *config,
                             const ph_token_stream_t *tokens,
                             ph_parsed_args_t *out,
                             ph_error_t **err);

void ph_parsed_args_destroy(ph_parsed_args_t *args);

/* ---- config-driven lookups ---- */

const char *ph_cmd_def_name(const ph_cli_config_t *config, int command_id);

const ph_argspec_t *ph_cmd_def_specs(const ph_cli_config_t *config,
                                      int command_id, size_t *count);

const ph_argspec_t *ph_cmd_def_spec_lookup(const ph_cli_config_t *config,
                                            int command_id,
                                            const char *flag_name);

/* ---- KVP nested object parser ---- */

typedef enum {
    PH_KVP_STRING,      /* bare token or quoted string */
    PH_KVP_INT,         /* integer literal */
    PH_KVP_BOOL,        /* true/false */
} ph_kvp_scalar_kind_t;

typedef struct ph_kvp_node ph_kvp_node_t;

struct ph_kvp_node {
    char                 *key;          /* heap-allocated key name */
    bool                  is_object;    /* true = children, false = scalar */
    /* scalar leaf (when is_object == false) */
    ph_kvp_scalar_kind_t  scalar_kind;
    char                 *value;        /* heap-allocated scalar string */
    /* nested object (when is_object == true) */
    ph_kvp_node_t        *children;     /* heap-allocated array */
    size_t                child_count;
};

#define PH_KVP_MAX_DEPTH 8

ph_result_t ph_kvp_parse(const char *input, ph_kvp_node_t **out,
                          size_t *count, ph_error_t **err);

void ph_kvp_destroy(ph_kvp_node_t *nodes, size_t count);

/* ---- validator ---- */

ph_result_t ph_validate(const ph_cli_config_t *config,
                         const ph_parsed_args_t *args,
                         ph_error_t **err);

/* ---- flag lookup helpers ---- */

const char *ph_args_get_flag(const ph_parsed_args_t *args, const char *name);
bool ph_args_has_flag(const ph_parsed_args_t *args, const char *name);
bool ph_args_is_enabled(const ph_parsed_args_t *args, const char *name);
bool ph_args_is_disabled(const ph_parsed_args_t *args, const char *name);

/* ---- diagnostic subcodes (usage error category) ---- */

#define PH_UX001_UNKNOWN_FLAG      1
#define PH_UX002_MISSING_VALUE     2
#define PH_UX003_DUPLICATE_FLAG    3
#define PH_UX004_POLARITY_CONFLICT 4
#define PH_UX005_TYPE_MISMATCH     5
#define PH_UX006_MALFORMED_KVP     6
#define PH_UX007_ENUM_VIOLATION    7

#endif /* PHOSPHOR_ARGS_H */

#ifndef PHOSPHOR_ERROR_H
#define PHOSPHOR_ERROR_H

#include "phosphor/types.h"

/* error categories -- map directly to exit codes 0-8 */
typedef enum {
    PH_ERR_NONE     = 0,   /* success */
    PH_ERR_GENERAL  = 1,   /* general/unmapped error */
    PH_ERR_USAGE    = 2,   /* invalid args/usage */
    PH_ERR_CONFIG   = 3,   /* config/template parse error */
    PH_ERR_FS       = 4,   /* filesystem error */
    PH_ERR_PROCESS  = 5,   /* process execution failure */
    PH_ERR_VALIDATE = 6,   /* validation/guardrail failure */
    PH_ERR_INTERNAL = 7,   /* internal invariant violation */
    PH_ERR_SIGNAL   = 8    /* interrupted by signal */
} ph_err_category_t;

/*
 * ph_error_t -- structured error with cause chain.
 *
 * ownership:
 *   message  -- owner: self (heap-allocated, freed by ph_error_destroy)
 *   context  -- owner: self (heap-allocated, optional)
 *   cause    -- owner: self (entire linked chain freed recursively)
 */
typedef struct ph_error ph_error_t;

struct ph_error {
    ph_err_category_t category;
    int               subcode;
    char             *message;
    char             *context;
    ph_error_t       *cause;
};

ph_error_t *ph_error_create(ph_err_category_t cat, int subcode,
                            const char *msg);

PH_PRINTF(3, 4)
ph_error_t *ph_error_createf(ph_err_category_t cat, int subcode,
                             const char *fmt, ...);

void ph_error_set_context(ph_error_t *err, const char *ctx);
void ph_error_chain(ph_error_t *err, ph_error_t *cause);
void ph_error_destroy(ph_error_t *err);

#endif /* PHOSPHOR_ERROR_H */

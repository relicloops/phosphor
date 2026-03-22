#ifndef PHOSPHOR_REGEX_H
#define PHOSPHOR_REGEX_H

#include "phosphor/types.h"
#include "phosphor/error.h"

/*
 * optional PCRE2-based regex matching for template filters.
 *
 * availability check is always available. actual regex compilation
 * and execution require PCRE2 (compile with -Dpcre2=true).
 */

/* ---- compiled regex handle ---- */

/*
 * ph_regex_t -- opaque compiled PCRE2 regex.
 *
 * ownership: heap-allocated, freed by ph_regex_destroy.
 */
typedef struct ph_regex ph_regex_t;

/* ---- always available (no PCRE2 dependency) ---- */

/*
 * ph_regex_available -- returns true if PCRE2 support was compiled in.
 */
bool ph_regex_available(void);

/* ---- PCRE2-dependent (compile with -Dpcre2=true) ---- */

#ifdef PHOSPHOR_HAS_PCRE2

/*
 * ph_regex_compile -- compile a PCRE2 pattern.
 *
 * on success: caller must call ph_regex_destroy(*out).
 * on error: PH_ERR with err->category = PH_ERR_CONFIG (invalid pattern).
 */
ph_result_t ph_regex_compile(const char *pattern, ph_regex_t **out,
                               ph_error_t **err);

/*
 * ph_regex_match -- test whether subject matches the compiled pattern.
 *
 * returns true on match, false on no match or NULL arguments.
 */
bool ph_regex_match(const ph_regex_t *re, const char *subject);

/*
 * ph_regex_destroy -- free a compiled regex.
 * safe to call with NULL (no-op).
 */
void ph_regex_destroy(ph_regex_t *re);

#endif /* PHOSPHOR_HAS_PCRE2 */

#endif /* PHOSPHOR_REGEX_H */

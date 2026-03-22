#include "phosphor/regex.h"
#include "phosphor/alloc.h"
#include "phosphor/log.h"

#ifdef PHOSPHOR_HAS_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

#include <string.h>

/* ---- always available (no PCRE2 dependency) ---- */

bool ph_regex_available(void) {
#ifdef PHOSPHOR_HAS_PCRE2
    return true;
#else
    return false;
#endif
}

/* ---- PCRE2-dependent implementation ---- */

#ifdef PHOSPHOR_HAS_PCRE2

struct ph_regex {
    pcre2_code *code;
};

ph_result_t ph_regex_compile(const char *pattern, ph_regex_t **out,
                               ph_error_t **err) {
    if (!pattern || !out) {
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "ph_regex_compile: NULL argument");
        return PH_ERR;
    }

    *out = NULL;

    int errcode;
    PCRE2_SIZE erroffset;
    pcre2_code *code = pcre2_compile(
        (PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
        PCRE2_UTF | PCRE2_UCP, &errcode, &erroffset, NULL);

    if (!code) {
        PCRE2_UCHAR errmsg[256];
        pcre2_get_error_message(errcode, errmsg, sizeof(errmsg));
        if (err)
            *err = ph_error_createf(PH_ERR_CONFIG, 0,
                "invalid regex pattern '%s' at offset %zu: %s",
                pattern, (size_t)erroffset, (const char *)errmsg);
        return PH_ERR;
    }

    ph_regex_t *re = ph_alloc(sizeof(ph_regex_t));
    if (!re) {
        pcre2_code_free(code);
        if (err)
            *err = ph_error_createf(PH_ERR_INTERNAL, 0,
                                     "allocation failed");
        return PH_ERR;
    }

    re->code = code;
    *out = re;
    return PH_OK;
}

bool ph_regex_match(const ph_regex_t *re, const char *subject) {
    if (!re || !re->code || !subject) return false;

    pcre2_match_data *md =
        pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!md) return false;

    int rc = pcre2_match(re->code, (PCRE2_SPTR)subject,
                          PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    pcre2_match_data_free(md);

    return rc >= 0;
}

void ph_regex_destroy(ph_regex_t *re) {
    if (!re) return;
    if (re->code) pcre2_code_free(re->code);
    ph_free(re);
}

#endif /* PHOSPHOR_HAS_PCRE2 */

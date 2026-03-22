#ifndef PHOSPHOR_LOG_H
#define PHOSPHOR_LOG_H

#include "phosphor/types.h"

/* log levels ordered by severity */
typedef enum {
    PH_LOG_ERROR,
    PH_LOG_WARN,
    PH_LOG_INFO,
    PH_LOG_DEBUG,
    PH_LOG_TRACE
} ph_log_level_t;

void ph_log_set_level(ph_log_level_t level);
ph_log_level_t ph_log_get_level(void);

PH_PRINTF(2, 3)
void ph_log(ph_log_level_t level, const char *fmt, ...);

/*
 * output destinations:
 *   info  -> stdout
 *   all other levels -> stderr
 */
#define ph_log_error(...) ph_log(PH_LOG_ERROR, __VA_ARGS__)
#define ph_log_warn(...)  ph_log(PH_LOG_WARN,  __VA_ARGS__)
#define ph_log_info(...)  ph_log(PH_LOG_INFO,  __VA_ARGS__)
#define ph_log_debug(...) ph_log(PH_LOG_DEBUG, __VA_ARGS__)
#define ph_log_trace(...) ph_log(PH_LOG_TRACE, __VA_ARGS__)

#endif /* PHOSPHOR_LOG_H */

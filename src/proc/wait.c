#include "phosphor/proc.h"

int ph_proc_map_exit(const ph_proc_result_t *result) {
    if (!result) return 1;

    if (result->signaled) return 8;

    int code = result->exit_code;

    if (code >= 128) return 8;
    if (code >= 0 && code <= 7) return code;

    /* 8+ (but < 128) -> general */
    return 1;
}

#ifndef PHOSPHOR_TYPES_H
#define PHOSPHOR_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* result type for fallible operations */
typedef enum {
    PH_OK  =  0,
    PH_ERR = -1
} ph_result_t;

/* printf format attribute (GCC/Clang) */
#ifdef __GNUC__
#define PH_PRINTF(fmt_idx, arg_idx) \
    __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define PH_PRINTF(fmt_idx, arg_idx)
#endif

#endif /* PHOSPHOR_TYPES_H */

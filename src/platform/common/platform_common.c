#include "phosphor/platform.h"

double ph_clock_elapsed_ms(uint64_t start_ns, uint64_t end_ns) {
    if (end_ns <= start_ns) return 0.0;
    return (double)(end_ns - start_ns) / 1000000.0;
}

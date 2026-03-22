#include "phosphor/platform.h"

#ifdef PH_PLATFORM_MACOS
    #include <mach/mach_time.h>
#else
    #include <time.h>
#endif

uint64_t ph_clock_monotonic_ns(void) {
#ifdef PH_PLATFORM_MACOS
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0) {
        mach_timebase_info(&tb);
    }
    uint64_t ticks = mach_absolute_time();
    return ticks * tb.numer / tb.denom;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

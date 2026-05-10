#include "tick.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
# include <windows.h>
#elif defined(__APPLE__)
# include <mach/mach_time.h>
#endif

sc_tick
sc_tick_now(void) {
#if defined(__APPLE__)
    // On macOS < 10.12, clock_gettime(CLOCK_MONOTONIC) is not available.
    // Use mach_absolute_time() which is available on all macOS versions.
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t count = mach_absolute_time();
    // Convert to nanoseconds
    uint64_t ns = count * timebase_info.numer / timebase_info.denom;
    return SC_TICK_FROM_NS(ns);
#elif !defined(_WIN32)
    // Maximum sc_tick precision (microsecond)
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret) {
        abort();
    }

    return SC_TICK_FROM_SEC(ts.tv_sec) + SC_TICK_FROM_NS(ts.tv_nsec);
#else
    LARGE_INTEGER c;

    // On systems that run Windows XP or later, the function will always
    // succeed and will thus never return zero.
    // <https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter>
    // <https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency>

    BOOL ok = QueryPerformanceCounter(&c);
    assert(ok);
    (void) ok;

    LONGLONG counter = c.QuadPart;

    static LONGLONG frequency;
    if (!frequency) {
        // Initialize on first call
        LARGE_INTEGER f;
        ok = QueryPerformanceFrequency(&f);
        assert(ok);
        frequency = f.QuadPart;
        assert(frequency);
    }

    if (frequency % SC_TICK_FREQ == 0) {
        // Expected case (typically frequency = 10000000, i.e. 100ns precision)
        sc_tick div = frequency / SC_TICK_FREQ;
        return SC_TICK_FROM_US(counter / div);
    }

    // Split the division to avoid overflow
    sc_tick secs = SC_TICK_FROM_SEC(counter / frequency);
    sc_tick subsec = SC_TICK_FREQ * (counter % frequency) / frequency;
    return secs + subsec;
#endif
}

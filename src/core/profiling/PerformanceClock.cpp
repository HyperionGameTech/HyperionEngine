#include <core/profiling/PerformanceClock.hpp>

#include <core/utilities/Time.hpp>

#ifdef HYP_UNIX
#include <sys/time.h>
#elif defined (HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace hyperion {
namespace profiling {

uint64 PerformanceClock::Now()
{
#ifdef HYP_UNIX
    struct timeval tv;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return uint64(ts.tv_sec) * 1000000 + uint64(ts.tv_nsec) / 1000;
#else
    // @TODO 

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    return uint64(ft.dwHighDateTime) << 32 | ft.dwLowDateTime;
#endif
}

uint64 PerformanceClock::TimeSince(uint64 microseconds)
{
    const uint64 us = microseconds;
    const uint64 now = Now();

    return now - us;
}

PerformanceClock::PerformanceClock()
    : m_start_time_us(0),
      m_end_time_us(0)
{
}

void PerformanceClock::Start()
{
    m_start_time_us = Now();
    m_end_time_us = 0;
}

void PerformanceClock::Stop()
{
    m_end_time_us = Now();
}
    
} // namespace profiling
} // namespace hyperion
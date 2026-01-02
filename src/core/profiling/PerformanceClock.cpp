#include <core/profiling/PerformanceClock.hpp>

#include <core/utilities/Time.hpp>

#ifdef HYP_UNIX
#include <sys/time.h>
#elif defined(HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace Hyperion {
namespace profiling {

#ifdef HYP_WINDOWS
static double GetPCFreq_Internal()
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);

    return double(li.QuadPart) / 1000.0;
}

static double GetPCFreq()
{
    static double s_freq = GetPCFreq_Internal();
    return s_freq;
}
#endif

uint64 PerformanceClock::Now()
{
#ifdef HYP_UNIX
    struct timeval tv;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return uint64(ts.tv_sec) * 1000000 + uint64(ts.tv_nsec) / 1000;
#else
    /*FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);

    return uint64(ft.dwHighDateTime) << 32 | ft.dwLowDateTime;*/

    LARGE_INTEGER li;
    if (!QueryPerformanceCounter(&li))
    {
        return 0;
    }

    return li.QuadPart;
#endif
}

double PerformanceClock::TimeSince(uint64 timestamp)
{
    const uint64 now = Now();

    return double(now - timestamp) / GetPCFreq();
}

double PerformanceClock::ToMilliseconds(uint64 timestamp)
{
    return double(timestamp) / GetPCFreq();
}

PerformanceClock::PerformanceClock()
    : m_startTime(0),
      m_endTime(0)
{
}

void PerformanceClock::Start()
{
    m_startTime = Now();
    m_endTime = 0;
}

void PerformanceClock::Stop()
{
    m_endTime = Now();
}

double PerformanceClock::ElapsedMs() const
{
    return double((m_endTime == 0 ? Now() : m_endTime) - m_startTime) / GetPCFreq();
}

} // namespace profiling
} // namespace Hyperion
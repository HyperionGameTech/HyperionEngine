#include <core/utilities/Time.hpp>

#ifdef HYP_UNIX
#include <sys/time.h>
#elif defined(HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

namespace hyperion {
namespace utilities {

#pragma region TimeDiff

TimeDiff TimeDiff::operator+(const Time &other) const
    { return TimeDiff(milliseconds + other.m_value); }

TimeDiff &TimeDiff::operator+=(const Time &other)
    { milliseconds += other.m_value; return *this; }

TimeDiff TimeDiff::operator-(const Time &other) const
    { return TimeDiff(milliseconds + other.m_value); }

TimeDiff &TimeDiff::operator-=(const Time &other)
    { milliseconds += other.m_value; return *this; }

#pragma endregion TimeDiff

#pragma region Time

Time::Time()
    : m_value(Time::Now().m_value)
{
}

Time::Time(uint64 timestamp)
    : m_value(timestamp)
{
}

Time Time::Now()
{
#ifdef HYP_UNIX
    struct timeval tv;
    gettimeofday(&tv, 0);

    // convert to milliseconds
    return Time(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#elif defined(HYP_WINDOWS)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    // convert to milliseconds
    return Time((uint64(ft.dwHighDateTime) << 32 | ft.dwLowDateTime) / 10000);
#endif
}

#pragma endregion Time

} // namespace utilities
} // namespace hyperion
#include <core/system/Time.hpp>

#include <ctime>

namespace hyperion {
namespace sys {

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
    const std::time_t now = std::time(nullptr);

    return Time(uint64(now));
}

#pragma endregion Time

} // namespace sys
} // namespace hyperion
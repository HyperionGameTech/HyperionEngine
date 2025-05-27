#ifndef HYPERION_TIME_HPP
#define HYPERION_TIME_HPP

#include <core/Defines.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace utilities {

class Time;

struct HYP_API TimeDiff
{
    TimeDiff()
        : milliseconds(0)
    {
    }

    TimeDiff(int64 milliseconds)
        : milliseconds(milliseconds)
    {
    }

    TimeDiff(const TimeDiff& other) = default;
    TimeDiff& operator=(const TimeDiff& other) = default;
    TimeDiff(TimeDiff&& other) noexcept = default;
    TimeDiff& operator=(TimeDiff&& other) noexcept = default;
    ~TimeDiff() = default;

    HYP_FORCE_INLINE explicit operator int64() const
    {
        return milliseconds;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return milliseconds != 0;
    }

    HYP_FORCE_INLINE bool operator<(const TimeDiff& other) const
    {
        return milliseconds < other.milliseconds;
    }

    HYP_FORCE_INLINE bool operator<=(const TimeDiff& other) const
    {
        return milliseconds <= other.milliseconds;
    }

    HYP_FORCE_INLINE bool operator>(const TimeDiff& other) const
    {
        return milliseconds > other.milliseconds;
    }

    HYP_FORCE_INLINE bool operator>=(const TimeDiff& other) const
    {
        return milliseconds >= other.milliseconds;
    }

    HYP_FORCE_INLINE bool operator==(const TimeDiff& other) const
    {
        return milliseconds == other.milliseconds;
    }

    HYP_FORCE_INLINE bool operator!=(const TimeDiff& other) const
    {
        return milliseconds != other.milliseconds;
    }

    HYP_FORCE_INLINE TimeDiff operator+(const TimeDiff& other) const
    {
        return TimeDiff(milliseconds + other.milliseconds);
    }

    HYP_FORCE_INLINE TimeDiff& operator+=(const TimeDiff& other)
    {
        milliseconds += other.milliseconds;
        return *this;
    }

    HYP_FORCE_INLINE TimeDiff operator-(const TimeDiff& other) const
    {
        return TimeDiff(milliseconds - other.milliseconds);
    }

    HYP_FORCE_INLINE TimeDiff& operator-=(const TimeDiff& other)
    {
        milliseconds -= other.milliseconds;
        return *this;
    }

    TimeDiff operator+(const Time& other) const;
    TimeDiff& operator+=(const Time& other);
    TimeDiff operator-(const Time& other) const;

    TimeDiff& operator-=(const Time& other);

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(milliseconds);
    }

    int64 milliseconds;
};

HYP_STRUCT(Serialize = "bitwise")

class HYP_API Time
{
public:
    friend struct TimeDiff;

    Time();
    Time(uint64 timestamp);
    Time(const Time& other) = default;
    Time& operator=(const Time& other) = default;
    Time(Time&& other) noexcept = default;
    Time& operator=(Time&& other) noexcept = default;
    ~Time() = default;

    HYP_FORCE_INLINE explicit operator uint64() const
    {
        return m_value;
    }

    HYP_FORCE_INLINE bool operator<(const Time& other) const
    {
        return m_value < other.m_value;
    }

    HYP_FORCE_INLINE bool operator<=(const Time& other) const
    {
        return m_value <= other.m_value;
    }

    HYP_FORCE_INLINE bool operator>(const Time& other) const
    {
        return m_value > other.m_value;
    }

    HYP_FORCE_INLINE bool operator>=(const Time& other) const
    {
        return m_value >= other.m_value;
    }

    HYP_FORCE_INLINE bool operator==(const Time& other) const
    {
        return m_value == other.m_value;
    }

    HYP_FORCE_INLINE bool operator!=(const Time& other) const
    {
        return m_value != other.m_value;
    }

    HYP_FORCE_INLINE Time operator+(const TimeDiff& diff) const
    {
        return Time(m_value + diff.milliseconds);
    }

    HYP_FORCE_INLINE Time& operator+=(const TimeDiff& diff)
    {
        m_value += diff.milliseconds;
        return *this;
    }

    HYP_FORCE_INLINE TimeDiff operator-(const Time& other) const
    {
        return TimeDiff(m_value - other.m_value);
    }

    HYP_FORCE_INLINE Time operator-(const TimeDiff& diff) const
    {
        return Time(m_value - diff.milliseconds);
    }

    HYP_FORCE_INLINE Time& operator-=(const Time& other)
    {
        m_value -= other.m_value;
        return *this;
    }

    HYP_FORCE_INLINE Time& operator-=(const TimeDiff& diff)
    {
        m_value -= diff.milliseconds;
        return *this;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_value);
    }

    static Time Now();

private:
    uint64 m_value;
};

} // namespace utilities

using utilities::Time;
using utilities::TimeDiff;

} // namespace hyperion

#endif
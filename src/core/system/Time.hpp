#ifndef HYPERION_TIME_HPP
#define HYPERION_TIME_HPP

#include <core/Defines.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace sys {

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

    TimeDiff(const TimeDiff &other)                 = default;
    TimeDiff &operator=(const TimeDiff &other)      = default;
    TimeDiff(TimeDiff &&other) noexcept             = default;
    TimeDiff &operator=(TimeDiff &&other) noexcept  = default;
    ~TimeDiff()                                     = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator int64() const
        { return milliseconds; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return milliseconds != 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const TimeDiff &other) const
        { return milliseconds < other.milliseconds; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<=(const TimeDiff &other) const
        { return milliseconds <= other.milliseconds; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>(const TimeDiff &other) const
        { return milliseconds > other.milliseconds; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>=(const TimeDiff &other) const
        { return milliseconds >= other.milliseconds; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const TimeDiff &other) const
        { return milliseconds == other.milliseconds; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const TimeDiff &other) const
        { return milliseconds != other.milliseconds; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    TimeDiff operator+(const TimeDiff &other) const
        { return TimeDiff(milliseconds + other.milliseconds); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    TimeDiff &operator+=(const TimeDiff &other)
        { milliseconds += other.milliseconds; return *this; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    TimeDiff operator-(const TimeDiff &other) const
        { return TimeDiff(milliseconds - other.milliseconds); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    TimeDiff &operator-=(const TimeDiff &other)
        { milliseconds -= other.milliseconds; return *this; }

    [[nodiscard]]
    TimeDiff operator+(const Time &other) const;

    [[nodiscard]]
    TimeDiff &operator+=(const Time &other);

    [[nodiscard]]
    TimeDiff operator-(const Time &other) const;

    [[nodiscard]]
    TimeDiff &operator-=(const Time &other);

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return HashCode::GetHashCode(milliseconds); }

    int64   milliseconds;
};

class HYP_API Time
{
public:
    friend struct TimeDiff;

    Time();
    Time(uint64 timestamp);
    Time(const Time &other)                 = default;
    Time &operator=(const Time &other)      = default;
    Time(Time &&other) noexcept             = default;
    Time &operator=(Time &&other) noexcept  = default;
    ~Time()                                 = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator uint64() const
        { return m_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const Time &other) const
        { return m_value < other.m_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<=(const Time &other) const
        { return m_value <= other.m_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>(const Time &other) const
        { return m_value > other.m_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>=(const Time &other) const
        { return m_value >= other.m_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const Time &other) const
        { return m_value == other.m_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const Time &other) const
        { return m_value != other.m_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Time operator+(const TimeDiff &diff) const
        { return Time(m_value + diff.milliseconds); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Time &operator+=(const TimeDiff &diff)
        { m_value += diff.milliseconds; return *this; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    TimeDiff operator-(const Time &other) const
        { return TimeDiff(m_value - other.m_value); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Time operator-(const TimeDiff &diff) const
        { return Time(m_value - diff.milliseconds); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Time &operator-=(const Time &other)
        { m_value -= other.m_value; return *this; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Time &operator-=(const TimeDiff &diff)
        { m_value -= diff.milliseconds; return *this; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return HashCode::GetHashCode(m_value); }

    static Time Now();

private:
    uint64  m_value;
};

} // namespace sys

using sys::Time;
using sys::TimeDiff;

} // namespace hyperion

#endif
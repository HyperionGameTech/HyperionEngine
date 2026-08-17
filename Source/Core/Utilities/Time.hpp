#pragma once
#include <Core/Defines.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

namespace Hyperion {
namespace utilities {

struct Time;

struct CORE_API TimeDiff
{
    constexpr TimeDiff()
        : milliseconds(0)
    {
    }

    constexpr TimeDiff(int64 milliseconds)
        : milliseconds(milliseconds)
    {
    }

    constexpr TimeDiff(const TimeDiff& other) = default;
    TimeDiff& operator=(const TimeDiff& other) = default;
    
    constexpr TimeDiff(TimeDiff&& other) noexcept = default;
    TimeDiff& operator=(TimeDiff&& other) noexcept = default;

    ~TimeDiff() = default;

    HYP_FORCE_INLINE constexpr explicit operator int64() const
    {
        return milliseconds;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return milliseconds != 0;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const TimeDiff& other) const
    {
        return milliseconds < other.milliseconds;
    }

    HYP_FORCE_INLINE constexpr bool operator<=(const TimeDiff& other) const
    {
        return milliseconds <= other.milliseconds;
    }

    HYP_FORCE_INLINE constexpr bool operator>(const TimeDiff& other) const
    {
        return milliseconds > other.milliseconds;
    }

    HYP_FORCE_INLINE constexpr bool operator>=(const TimeDiff& other) const
    {
        return milliseconds >= other.milliseconds;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const TimeDiff& other) const
    {
        return milliseconds == other.milliseconds;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const TimeDiff& other) const
    {
        return milliseconds != other.milliseconds;
    }

    HYP_FORCE_INLINE constexpr TimeDiff operator+(const TimeDiff& other) const
    {
        return TimeDiff(milliseconds + other.milliseconds);
    }

    HYP_FORCE_INLINE TimeDiff& operator+=(const TimeDiff& other)
    {
        milliseconds += other.milliseconds;
        return *this;
    }

    HYP_FORCE_INLINE constexpr TimeDiff operator-(const TimeDiff& other) const
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

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(milliseconds);
    }

    int64 milliseconds;
};

struct CORE_API Time
{
    uint64 m_value;

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

    HYP_FORCE_INLINE uint64 ToMilliseconds() const
    {
        return m_value;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_value);
    }

    static Time Now();
};

} // namespace utilities

using utilities::Time;
using utilities::TimeDiff;

} // namespace Hyperion

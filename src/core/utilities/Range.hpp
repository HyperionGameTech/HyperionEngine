/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RANGE_HPP
#define HYPERION_RANGE_HPP

#include <core/math/MathUtil.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

template <class T>
class Range
{
public:
    static_assert(std::is_arithmetic_v<T>, "Range type must be arithmetic");

    using Iterator = T;
    using ConstIterator = const T;

    constexpr Range()
        : m_start {},
          m_end {}
    {
    }

    constexpr Range(T start, T end)
        : m_start(start),
          m_end(end)
    {
    }

    constexpr Range(const Range& other) = default;
    Range& operator=(const Range& other) = default;
    constexpr Range(Range&& other) = default;
    Range& operator=(Range&& other) = default;
    ~Range() = default;

    HYP_FORCE_INLINE explicit constexpr operator bool() const
    {
        return Distance() > 0;
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return Distance() <= 0;
    }

    HYP_FORCE_INLINE constexpr T GetStart() const
    {
        return m_start;
    }

    HYP_FORCE_INLINE void SetStart(T start)
    {
        m_start = start;
    }

    HYP_FORCE_INLINE constexpr T GetEnd() const
    {
        return m_end;
    }

    HYP_FORCE_INLINE void SetEnd(T end)
    {
        m_end = end;
    }

    HYP_FORCE_INLINE constexpr int64 Distance() const
    {
        return int64(m_end) - int64(m_start);
    }

    HYP_FORCE_INLINE constexpr int64 Step() const
    {
        return MathUtil::Sign(Distance());
    }

    HYP_FORCE_INLINE constexpr bool Includes(const T& value) const
    {
        return value >= m_start && value < m_end;
    }

    HYP_FORCE_INLINE constexpr Range operator|(const Range& other) const
    {
        return {
            MathUtil::Min(m_start, other.m_start),
            MathUtil::Max(m_end, other.m_end)
        };
    }

    HYP_FORCE_INLINE Range& operator|=(const Range& other)
    {
        m_start = MathUtil::Min(m_start, other.m_start);
        m_end = MathUtil::Max(m_end, other.m_end);

        return *this;
    }

    HYP_FORCE_INLINE constexpr Range operator&(const Range& other) const
    {
        return {
            MathUtil::Max(m_start, other.m_start),
            MathUtil::Min(m_end, other.m_end)
        };
    }

    HYP_FORCE_INLINE Range& operator&=(const Range& other)
    {
        m_start = MathUtil::Max(m_start, other.m_start);
        m_end = MathUtil::Min(m_end, other.m_end);

        return *this;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const Range& other) const
    {
        return Distance() < other.Distance();
    }

    HYP_FORCE_INLINE constexpr bool operator>(const Range& other) const
    {
        return Distance() > other.Distance();
    }

    HYP_FORCE_INLINE constexpr bool operator==(const Range& other) const
    {
        if (this == &other)
        {
            return true;
        }

        return m_start == other.m_start
            && m_end == other.m_end;
    }

    HYP_FORCE_INLINE bool operator!=(const Range& other) const
    {
        return !(*this == other);
    }

    /*! \brief Resets the range to an invalid state, with start set to the maximum safe value (e.g FLT_MAX) and end set to the minimum safe value (e.g -FLT_MAX). */
    HYP_FORCE_INLINE void Reset()
    {
        *this = Invalid();
    }

    /*! \brief Returns true if the range is valid, i.e. start is not the maximum safe value and end is not the minimum safe value. */
    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return m_start != MathUtil::MaxSafeValue<T>() || m_end != MathUtil::MinSafeValue<T>();
    }

    /*! \brief Returns an invalid range, with start set to the maximum safe value (e.g FLT_MAX) and end set to the minimum safe value (e.g -FLT_MAX). */
    static constexpr Range Invalid()
    {
        return Range(MathUtil::MaxSafeValue<T>(), MathUtil::MinSafeValue<T>());
    }

    HYP_DEF_STL_BEGIN_END(m_start, m_end)

private:
    T m_start, m_end;
};

} // namespace utilities

using utilities::Range;

} // namespace hyperion

#endif

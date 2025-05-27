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

    Range()
        : m_start {},
          m_end {}
    {
    }

    Range(T start, T end)
        : m_start(start),
          m_end(end)
    {
    }

    Range(const Range& other) = default;
    Range& operator=(const Range& other) = default;
    Range(Range&& other) = default;
    Range& operator=(Range&& other) = default;
    ~Range() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return Distance() > 0;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return Distance() <= 0;
    }

    HYP_FORCE_INLINE T GetStart() const
    {
        return m_start;
    }

    HYP_FORCE_INLINE void SetStart(T start)
    {
        m_start = start;
    }

    HYP_FORCE_INLINE T GetEnd() const
    {
        return m_end;
    }

    HYP_FORCE_INLINE void SetEnd(T end)
    {
        m_end = end;
    }

    HYP_FORCE_INLINE int64 Distance() const
    {
        return int64(m_end) - int64(m_start);
    }

    HYP_FORCE_INLINE int64 Step() const
    {
        return MathUtil::Sign(Distance());
    }

    HYP_FORCE_INLINE bool Includes(const T& value) const
    {
        return value >= m_start && value < m_end;
    }

    HYP_FORCE_INLINE Range operator|(const Range& other) const
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

    HYP_FORCE_INLINE Range operator&(const Range& other) const
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

    HYP_FORCE_INLINE bool operator<(const Range& other) const
    {
        return Distance() < other.Distance();
    }

    HYP_FORCE_INLINE bool operator>(const Range& other) const
    {
        return Distance() > other.Distance();
    }

    HYP_FORCE_INLINE bool operator==(const Range& other) const
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
        return !operator==(other);
    }

    HYP_FORCE_INLINE void Reset()
    {
        m_start = MathUtil::MaxSafeValue<T>();
        m_end = MathUtil::MinSafeValue<T>();
    }

    HYP_FORCE_INLINE bool Valid() const
    {
        return m_start != MathUtil::MaxSafeValue<T>() || m_end != MathUtil::MinSafeValue<T>();
    }

    HYP_DEF_STL_BEGIN_END(
        m_start,
        m_end)

private:
    T m_start, m_end;
};

} // namespace utilities

using utilities::Range;

} // namespace hyperion

#endif

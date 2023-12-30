#ifndef HYPERION_RANGE_H
#define HYPERION_RANGE_H

#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {

template <class T>
class Range {
public:
    using Iterator      = T;
    using ConstIterator = const T;

    Range()
        : m_start{}, m_end{} {}

    Range(const T &start, const T &end)
        : m_start(start), m_end(end) {}

    Range(T &&start, T &&end)
        : m_start(std::move(start)), m_end(std::move(end)) {}

    Range(const Range &other) = default;
    Range &operator=(const Range &other) = default;
    Range(Range &&other) = default;
    Range &operator=(Range &&other) = default;
    ~Range() = default;

    explicit operator Bool() const
        { return Distance() > 0; }

    const T &GetStart() const
        { return m_start; }

    void SetStart(const T &start)
        { m_start = start; }

    const T &GetEnd() const
        { return m_end; }

    void SetEnd(const T &end)
        { m_end = end; }

    Int64 Distance() const
        { return Int64(m_end) - Int64(m_start); }

    Int64 Step() const
        { return MathUtil::Sign(Distance()); }

    Bool Includes(const T &value) const
        { return value >= m_start && value < m_end; }

    Range operator|(const Range &other) const
    {
        return {
            MathUtil::Min(m_start, other.m_start),
            MathUtil::Max(m_end, other.m_end)
        };
    }

    Range &operator|=(const Range &other)
    {
        m_start = MathUtil::Min(m_start, other.m_start);
        m_end = MathUtil::Max(m_end, other.m_end);

        return *this;
    }

    Range operator&(const Range &other) const
    {
        return {
            MathUtil::Max(m_start, other.m_start),
            MathUtil::Min(m_end, other.m_end)
        };
    }

    Range &operator&=(const Range &other)
    {
        m_start = MathUtil::Max(m_start, other.m_start);
        m_end = MathUtil::Min(m_end, other.m_end);

        return *this;
    }

    Bool operator<(const Range &other) const { return Distance() < other.Distance(); }
    Bool operator>(const Range &other) const { return Distance() > other.Distance(); }

    Bool operator==(const Range &other) const
    {
        if (this == &other) {
            return true;
        }

        return m_start == other.m_start
            && m_end == other.m_end;
    }

    Bool operator!=(const Range &other) const { return !operator==(other); }

    void Reset()
    {
        m_start = MathUtil::MaxSafeValue<T>();
        m_end = MathUtil::MinSafeValue<T>();
    }

    Bool Valid() const
        { return m_start != MathUtil::MaxSafeValue<T>() || m_end != MathUtil::MinSafeValue<T>(); }

    HYP_DEF_STL_BEGIN_END(
        m_start,
        m_end
    )

private:
    T m_start, m_end;
};

} // namespace hyperion

#endif

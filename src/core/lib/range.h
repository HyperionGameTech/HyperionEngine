#ifndef HYPERION_RANGE_H
#define HYPERION_RANGE_H

#include <math/math_util.h>
#include <util/test/htest.h>

#include <atomic>

namespace hyperion {

template <class T>
class Range {
public:
    using iterator = T;

    Range()                             : m_start{}, m_end{} {}
    Range(const T &start, const T &end) : m_start(start), m_end(end) {}
    Range(T &&start, T &&end)           : m_start(std::move(start)), m_end(std::move(end)) {}
    Range(const Range &other) = default;
    Range &operator=(const Range &other) = default;
    Range(Range &&other) = default;
    Range &operator=(Range &&other) = default;
    ~Range() = default;

    inline explicit operator bool() const      { return Distance() > 0; }

    inline const T &GetStart() const           { return m_start; }
    inline void SetStart(const T &start)       { m_start = start; }
    inline const T &GetEnd()   const           { return m_end; }
    inline void SetEnd(const T &end)           { m_end = end; }

    inline int64_t Distance() const            { return static_cast<int64_t>(m_end) - static_cast<int64_t>(m_start); }
    inline int64_t Step() const                { return MathUtil::Sign(Distance()); }
    inline bool Includes(const T &value) const { return value >= m_start && value < m_end; }

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
        m_end   = MathUtil::Max(m_end, other.m_end);

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
        m_end   = MathUtil::Min(m_end, other.m_end);

        return *this;
    }

    bool operator<(const Range &other) const { return Distance() < other.Distance(); }
    bool operator>(const Range &other) const { return Distance() > other.Distance(); }

    bool operator==(const Range &other) const
    {
        if (this == &other) {
            return true;
        }

        return m_start == other.m_start
            && m_end == other.m_end;
    }

    bool operator!=(const Range &other) const { return !operator==(other); }
    
    Range Excluding(const T &value) const
    {
        const auto step = Step();

        if (value == m_start + step) {
            return Range{T(m_start + step), m_end};
        }

        if (value == m_end - step) {
            return Range{m_start, T(m_end - step)};
        }

        return Range{m_start, m_end};
    }

    void Reset()
    {
        m_start = MathUtil::MaxSafeValue<T>();
        m_end   = MathUtil::MinSafeValue<T>();
    }
    
    iterator begin() const { return m_start; }
    iterator end() const { return m_end; }

private:
    T m_start, m_end;
};

template <class T>
class AtomicRange {
public:
    static_assert(std::atomic<T>::is_always_lock_free);

    using iterator = T;

    AtomicRange()                             : m_start{}, m_end{} {}
    AtomicRange(const T &start, const T &end) : m_start(start), m_end(end) {}
    AtomicRange(T &&start, T &&end)           : m_start(std::move(start)), m_end(std::move(end)) {}
    AtomicRange(const AtomicRange &other)     : m_start(other.m_start.load()), m_end(other.m_end.load()) {}

    AtomicRange &operator=(const AtomicRange &other)
    {
        m_start = other.m_start.load();
        m_end   = other.m_end.load();

        return *this;
    }

    AtomicRange(AtomicRange &&other) = default;
    AtomicRange &operator=(AtomicRange &&other) = default;
    ~AtomicRange() = default;

    inline explicit operator bool() const      { return Distance() > 0; }

    inline T GetStart() const           { return m_start.load(); }
    inline void SetStart(T start)       { m_start = start; }
    inline T GetEnd() const             { return m_end.load(); }
    inline void SetEnd(T end)           { m_end = end; }

    inline int64_t Distance() const            { return int64_t(m_end) - int32_t(m_start); }
    inline int64_t Step() const                { return MathUtil::Sign(Distance()); }
    inline bool Includes(const T &value) const { return value >= m_start && value < m_end; }

    template <class OtherRange>
    AtomicRange operator|(const OtherRange &other) const
    {
        return {
            MathUtil::Min(m_start.load(), other.GetStart()),
            MathUtil::Max(m_end.load(), other.GetEnd())
        };
    }
    
    template <class OtherRange>
    AtomicRange &operator|=(const OtherRange &other)
    {
        m_start = MathUtil::Min(m_start.load(), other.GetStart());
        m_end   = MathUtil::Max(m_end.load(), other.GetEnd());

        return *this;
    }
    
    template <class OtherRange>
    AtomicRange operator&(const OtherRange &other) const
    {
        return {
            MathUtil::Max(m_start.load(), other.GetStart()),
            MathUtil::Min(m_end.load(), other.GetEnd())
        };
    }
    
    template <class OtherRange>
    AtomicRange &operator&=(const OtherRange &other)
    {
        m_start = MathUtil::Max(m_start.load(), other.GetStart());
        m_end   = MathUtil::Min(m_end.load(), other.GetEnd());

        return *this;
    }

    bool operator<(const AtomicRange &other) const { return Distance() < other.Distance(); }
    bool operator>(const AtomicRange &other) const { return Distance() > other.Distance(); }

    bool operator==(const AtomicRange &other) const
    {
        if (this == &other) {
            return true;
        }

        return m_start == other.m_start
            && m_end == other.m_end;
    }

    bool operator!=(const AtomicRange &other) const { return !operator==(other); }
    
    AtomicRange Excluding(const T &value) const
    {
        const auto step  = Step(),
                   _start = m_start.load(),
                   _end   = m_end.load();

        if (value == _start + step) {
            return AtomicRange{T(_start + step), _end};
        }

        if (value == _end - step) {
            return AtomicRange{_start, T(_end - step)};
        }

        return AtomicRange{_start, _end};
    }
    
    iterator begin() const { return m_start; }
    iterator end() const { return m_end; }

private:
    std::atomic<T> m_start, m_end;
};

namespace test {

template<>
class TestClass<Range<int>> : public TestClassBase {
public:
    TestClass()
    {
        Describe(HYP_METHOD(Range<int>::Distance), [](auto &it) {
            it("returns false if the given element is out of range", [](auto &expect) {
               HYP_EXPECT(!(Range<int>{5, 12}).Includes(2));
               HYP_EXPECT((Range<int>{5, 12}).Includes(8));
            });
        });

        Describe(HYP_METHOD(Range<int>::Excluding), [](Unit &it) -> void {
            it("removes value from inclusive range", [](Case &expect) {
                HYP_EXPECT((Range<int>{0, 9}).Excluding(8) == (Range<int>{0, 8}));
                HYP_EXPECT((Range<int>{0, 9}).Excluding(7) != (Range<int>{0, 7}));
            });
        });
    }
};

} // namespace test

} // namespace hyperion

#endif

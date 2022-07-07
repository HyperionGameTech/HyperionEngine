#ifndef HYPERION_V2_LIB_FIXED_ARRAY_H
#define HYPERION_V2_LIB_FIXED_ARRAY_H

#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

template <class T, size_t Sz>
class FixedArray {
    T m_data[Sz];

public:
    using Iterator      = T *;
    using ConstIterator = const T *;

    FixedArray();

    FixedArray(T const (&items)[Sz])
    {
        for (size_t i = 0; i < Sz; i++) {
            m_data[i] = items[i];
        }
    }

    FixedArray &operator=(T const (&items)[Sz])
    {
        for (size_t i = 0; i < Sz; i++) {
            m_data[i] = items[i];
        }

        return *this;
    }

    FixedArray(const std::initializer_list<T> &items)
    {
        if (items.size() <= Sz) {
            std::copy(
                items.begin(),
                items.end(),
                std::begin(m_data)
            );
        } else {
            std::copy(
                items.begin(),
                items.begin() + Sz,
                std::begin(m_data)
            );
        }
    }

    FixedArray(const FixedArray &other);
    FixedArray &operator=(const FixedArray &other);
    FixedArray(FixedArray &&other) noexcept;
    FixedArray &operator=(FixedArray &&other) noexcept;
    ~FixedArray();

    HYP_FORCE_INLINE
    T &operator[](UInt index)               { return m_data[index]; }

    HYP_FORCE_INLINE
    const T &operator[](UInt index) const   { return m_data[index]; }

    HYP_FORCE_INLINE
    size_t Size() const                     { return Sz; }

    HYP_FORCE_INLINE
    bool Empty() const                      { return Sz == 0; }

    HYP_FORCE_INLINE
    bool Any() const                        { return Sz != 0; }

    HYP_FORCE_INLINE
    T *Data()                               { return static_cast<T *>(m_data); }

    HYP_FORCE_INLINE
    const T *Data() const                   { return static_cast<const T *>(m_data); }

    HYP_DEF_STL_BEGIN_END(&m_data[0], &m_data[Sz])
};

template <class T, size_t Sz>
FixedArray<T, Sz>::FixedArray()
    : m_data{}
{
}

template <class T, size_t Sz>
FixedArray<T, Sz>::FixedArray(const FixedArray &other)
{
    for (size_t i = 0; i < Sz; i++) {
        m_data[i] = other.m_data[i];
    }
}

template <class T, size_t Sz>
auto FixedArray<T, Sz>::operator=(const FixedArray &other) -> FixedArray&
{
    for (size_t i = 0; i < Sz; i++) {
        m_data[i] = other.m_data[i];
    }

    return *this;
}

template <class T, size_t Sz>
FixedArray<T, Sz>::FixedArray(FixedArray &&other) noexcept
{
    for (size_t i = 0; i < Sz; i++) {
        m_data[i] = std::move(other.m_data[i]);
    }
}

template <class T, size_t Sz>
auto FixedArray<T, Sz>::operator=(FixedArray &&other) noexcept -> FixedArray&
{
    for (size_t i = 0; i < Sz; i++) {
        m_data[i] = std::move(other.m_data[i]);
    }

    return *this;
}

template <class T, size_t Sz>
FixedArray<T, Sz>::~FixedArray() = default;

// deduction guide
template <typename Tp, typename ...Args>
FixedArray(Tp, Args...) -> FixedArray<std::enable_if_t<(std::is_same_v<Tp, Args> && ...), Tp>, 1 + sizeof...(Args)>;

} // namespace hyperion

#endif

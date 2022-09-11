#ifndef HYPERION_V2_LIB_FIXED_ARRAY_H
#define HYPERION_V2_LIB_FIXED_ARRAY_H

#include "ContainerBase.hpp"
#include <math/MathUtil.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

template <class T, SizeType Sz>
class FixedArray : public ContainerBase<FixedArray<T, Sz>, UInt>
{
    T m_values[MathUtil::Max(Sz, 1)];

public:
    using Base = ContainerBase<FixedArray<T, Sz>, UInt>;

    using Iterator = T *;
    using ConstIterator = const T *;

    static constexpr SizeType size = Sz;

    FixedArray();

    template <class ... Args>
    constexpr FixedArray(Args &&... args)
        : m_values { std::forward<Args>(args)... }
    {
    }

    constexpr FixedArray(const T *ary, SizeType count)
    {
        for (SizeType i = 0; i < MathUtil::Min(Sz, count); i++) {
            m_values = ary[i];
        }
    }

    FixedArray(const FixedArray &other);
    FixedArray &operator=(const FixedArray &other);
    FixedArray(FixedArray &&other) noexcept;
    FixedArray &operator=(FixedArray &&other) noexcept;
    ~FixedArray();

    template <class Function>
    FixedArray Map(Function &&fn) const
    {
        FixedArray result;
        SizeType index = 0;

        for (auto it = Begin(); it != End(); ++it) {
            result[index++] = fn(*it);
        }

        return result;
    }

    HYP_FORCE_INLINE
    T &operator[](typename FixedArray::Base::KeyType index)
        { return m_values[index]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &operator[](typename FixedArray::Base::KeyType index) const
        { return m_values[index]; }

    HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return Sz; }

    [[nodiscard]] HYP_FORCE_INLINE
    bool Empty() const
        { return Sz == 0; }

    [[nodiscard]] HYP_FORCE_INLINE
    bool Any() const
        { return Sz != 0; }

    template <class Lambda>
    [[nodiscard]] bool Any(Lambda &&lambda) const
        { return Base::Any(std::forward<Lambda>(lambda)); }

    template <class Lambda>
    [[nodiscard]] bool Every(Lambda &&lambda) const
        { return Base::Every(std::forward<Lambda>(lambda)); }

    [[nodiscard]] HYP_FORCE_INLINE
    T *Data()
        { return static_cast<T *>(m_values); }

    [[nodiscard]] HYP_FORCE_INLINE
    const T *Data() const
        { return static_cast<const T *>(m_values); }

    [[nodiscard]] HYP_FORCE_INLINE
    T &Front()
        { return m_values[0]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &Front() const
        { return m_values[0]; }

    [[nodiscard]] HYP_FORCE_INLINE
    T &Back()
        { return m_values[Sz - 1]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &Back() const
        { return m_values[Sz - 1]; }

    HYP_DEF_STL_BEGIN_END(&m_values[0], &m_values[Sz])
};

template <class T, SizeType Sz>
FixedArray<T, Sz>::FixedArray()
    : m_values{}
{
}

template <class T, SizeType Sz>
FixedArray<T, Sz>::FixedArray(const FixedArray &other)
{
    for (SizeType i = 0; i < Sz; i++) {
        m_values[i] = other.m_values[i];
    }
}

template <class T, SizeType Sz>
auto FixedArray<T, Sz>::operator=(const FixedArray &other) -> FixedArray&
{
    for (SizeType i = 0; i < Sz; i++) {
        m_values[i] = other.m_values[i];
    }

    return *this;
}

template <class T, SizeType Sz>
FixedArray<T, Sz>::FixedArray(FixedArray &&other) noexcept
{
    for (SizeType i = 0; i < Sz; i++) {
        m_values[i] = std::move(other.m_values[i]);
    }
}

template <class T, SizeType Sz>
auto FixedArray<T, Sz>::operator=(FixedArray &&other) noexcept -> FixedArray&
{
    for (SizeType i = 0; i < Sz; i++) {
        m_values[i] = std::move(other.m_values[i]);
    }

    return *this;
}

template <class T, SizeType Sz>
FixedArray<T, Sz>::~FixedArray() = default;

// deduction guide
template <typename Tp, typename ...Args>
FixedArray(Tp, Args...) -> FixedArray<std::enable_if_t<(std::is_same_v<Tp, Args> && ...), Tp>, 1 + sizeof...(Args)>;

} // namespace hyperion

#endif

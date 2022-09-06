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
class FixedArray : public ContainerBase<FixedArray<T, Sz>, UInt> {
    T m_data[MathUtil::Max(Sz, 1)];

public:
    using Iterator = T *;
    using ConstIterator = const T *;

    static constexpr SizeType size = Sz;

    FixedArray();

    template <class ... Args>
    constexpr FixedArray(Args &&... args)
        : m_data { std::forward<Args>(args)... }
    {
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
        { return m_data[index]; }

    [[nodiscard]] HYP_FORCE_INLINE
    const T &operator[](typename FixedArray::Base::KeyType index) const
        { return m_data[index]; }

    HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return Sz; }

    [[nodiscard]] HYP_FORCE_INLINE
    bool Empty() const
        { return Sz == 0; }

    [[nodiscard]] HYP_FORCE_INLINE
    bool Any() const
        { return Sz != 0; }

    [[nodiscard]] HYP_FORCE_INLINE
    T *Data()
        { return static_cast<T *>(m_data); }

    [[nodiscard]] HYP_FORCE_INLINE
    const T *Data() const
        { return static_cast<const T *>(m_data); }

    HYP_DEF_STL_BEGIN_END(&m_data[0], &m_data[Sz])
};

template <class T, SizeType Sz>
FixedArray<T, Sz>::FixedArray()
    : m_data{}
{
}

template <class T, SizeType Sz>
FixedArray<T, Sz>::FixedArray(const FixedArray &other)
{
    for (SizeType i = 0; i < Sz; i++) {
        m_data[i] = other.m_data[i];
    }
}

template <class T, SizeType Sz>
auto FixedArray<T, Sz>::operator=(const FixedArray &other) -> FixedArray&
{
    for (SizeType i = 0; i < Sz; i++) {
        m_data[i] = other.m_data[i];
    }

    return *this;
}

template <class T, SizeType Sz>
FixedArray<T, Sz>::FixedArray(FixedArray &&other) noexcept
{
    for (SizeType i = 0; i < Sz; i++) {
        m_data[i] = std::move(other.m_data[i]);
    }
}

template <class T, SizeType Sz>
auto FixedArray<T, Sz>::operator=(FixedArray &&other) noexcept -> FixedArray&
{
    for (SizeType i = 0; i < Sz; i++) {
        m_data[i] = std::move(other.m_data[i]);
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

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FIXED_ARRAY_HPP
#define HYPERION_FIXED_ARRAY_HPP

#include <core/containers/ContainerBase.hpp>
#include <math/MathUtil.hpp>
#include <core/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

namespace containers {
namespace detail {

template <class T, SizeType Sz>
class FixedArray;

template <class T, SizeType Sz>
class FixedArrayImpl;

} // namespace detail

// #define FIXED_ARRAY_IMPL_METHOD(method_name, ...) \
//     containers::detail::FixedArrayImpl(&m_values[0]).method_name(__VA_ARGS__)

template <class T, SizeType Sz>
class FixedArray
{
public:
    static constexpr bool is_contiguous = true;

    T m_values[MathUtil::Max(Sz, 1)];

    using Iterator = T *;
    using ConstIterator = const T *;

    using KeyType = SizeType;

    static constexpr SizeType size = Sz;

    template <class OtherType, SizeType OtherSize>
    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator==(const FixedArray<OtherType, OtherSize> &other) const
    {
        if (std::addressof(other) == this) {
            return true;
        }

        if (size != other.size) {
            return false;
        }

        auto it = Begin();
        auto other_it = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++other_it) {
            if (!(*it == *other_it)) {
                return false;
            }
        }

        return true;
    }

    template <class OtherType, SizeType OtherSize>
    HYP_NODISCARD HYP_FORCE_INLINE
    bool operator!=(const FixedArray<OtherType, OtherSize> &other) const
        { return !(*this == other); }

    template <class Function>
    HYP_NODISCARD HYP_FORCE_INLINE
    FixedArray Map(Function &&fn) const
    {
        const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
        return impl.Map(std::forward<Function>(fn));
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool Contains(const T &value) const
    {
        const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
        return impl.Contains(value);
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    T &At(KeyType index)
        { AssertThrow(index < Sz); return m_values[index]; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const T &At(KeyType index) const
        { AssertThrow(index < Sz); return m_values[index]; }

    HYP_NODISCARD HYP_FORCE_INLINE
    T &operator[](KeyType index)
        { return m_values[index]; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const T &operator[](KeyType index) const
        { return m_values[index]; }

    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return Sz; }

    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr SizeType ByteSize() const
        { return Sz * sizeof(T); }

    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr bool Empty() const
        { return Sz == 0; }

    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr bool Any() const
        { return Sz != 0; }
        
    template <class Lambda>
    HYP_NODISCARD HYP_FORCE_INLINE
    bool Any(Lambda &&lambda) const
    {
        if constexpr (Sz == 0) {
            return false;
        } else {
            const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            return impl.Any(std::forward<Lambda>(lambda));
        }
    }

    template <class Lambda>
    HYP_NODISCARD HYP_FORCE_INLINE
    bool Every(Lambda &&lambda) const
    {
        const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
        return impl.Every(std::forward<Lambda>(lambda));
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    auto Sum() const
    {
        if constexpr (Sz == 0) {
            return T();
        } else {
            const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);

            return impl.Sum();
        }
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    auto Avg() const
    {
        if constexpr (Sz == 0) {
            return T();
        } else {
            const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            return impl.Avg();
        }
    }

    template <class ConstIterator>
    HYP_NODISCARD HYP_FORCE_INLINE
    KeyType IndexOf(ConstIterator iter) const
    {
        if constexpr (Sz == 0) {
            return KeyType(-1);
        } else {
            const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            return impl.IndexOf(iter);
        }
    }

    template <class TaskSystem, class Lambda>
    HYP_FORCE_INLINE
    void ParallelForEach(TaskSystem &task_system, Lambda &&lambda) const
    {
        if constexpr (Sz != 0) {
            const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            impl.ParallelForEach(task_system, std::forward<Lambda>(lambda));
        }
    }

    template <class OtherContainer>
    HYP_NODISCARD HYP_FORCE_INLINE
    bool CompareBitwise(const OtherContainer &other) const
    {
        if constexpr (Sz != OtherContainer::size) {
            return false;
        } else {
            const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            return impl.CompareBitwise(other);
        }
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    T *Data()
        { return static_cast<T *>(m_values); }

    HYP_NODISCARD HYP_FORCE_INLINE
    const T *Data() const
        { return static_cast<const T *>(m_values); }

    HYP_NODISCARD HYP_FORCE_INLINE
    T &Front()
        { return m_values[0]; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const T &Front() const
        { return m_values[0]; }

    HYP_NODISCARD HYP_FORCE_INLINE
    T &Back()
        { return m_values[Sz - 1]; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const T &Back() const
        { return m_values[Sz - 1]; }

    HYP_DEF_STL_BEGIN_END(&m_values[0], &m_values[Sz])
    
    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        const containers::detail::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
        return impl.GetHashCode();
    }
};

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::FixedArray()
//     : m_values{}
// {
// }

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::FixedArray(const FixedArray &other)
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = other.m_values[i];
//     }
// }

// template <class T, SizeType Sz>
// auto FixedArray<T, Sz>::operator=(const FixedArray &other) -> FixedArray&
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = other.m_values[i];
//     }

//     return *this;
// }

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::FixedArray(FixedArray &&other) noexcept
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = std::move(other.m_values[i]);
//     }
// }

// template <class T, SizeType Sz>
// auto FixedArray<T, Sz>::operator=(FixedArray &&other) noexcept -> FixedArray&
// {
//     for (SizeType i = 0; i < Sz; i++) {
//         m_values[i] = std::move(other.m_values[i]);
//     }

//     return *this;
// }

// template <class T, SizeType Sz>
// FixedArray<T, Sz>::~FixedArray() = default;


namespace detail {

template <class T, SizeType Sz>
class FixedArrayImpl : public ContainerBase<FixedArrayImpl<T, Sz>, uint>
{
public:
    T *ptr;

    static constexpr bool is_contiguous = true;

    using Iterator      = T *;
    using ConstIterator = const T *;

    FixedArrayImpl(T *ptr)
        : ptr(ptr)
    {
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return Sz; }

    HYP_DEF_STL_BEGIN_END(ptr, ptr + Sz)
};

} // namespace detail

// deduction guide
template <typename Tp, typename ...Args>
FixedArray(Tp, Args...) -> FixedArray<std::enable_if_t<(std::is_same_v<Tp, Args> && ...), Tp>, 1 + sizeof...(Args)>;

} // namespace containers

template <class T, SizeType N>
using FixedArray = containers::FixedArray<T, N>;

template <class T, SizeType N>
constexpr uint ArraySize(const FixedArray<T, N> &)
{
    return N;
}

} // namespace hyperion

#endif

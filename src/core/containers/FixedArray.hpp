/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FIXED_ARRAY_HPP
#define HYPERION_FIXED_ARRAY_HPP

#include <core/containers/ContainerBase.hpp>

#include <core/utilities/Span.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

namespace containers {
template <class T, SizeType Sz>
class FixedArray;

template <class T, SizeType Sz>
class FixedArrayImpl;

/*! \brief FixedArray is a fixed-size array container that provides a contiguous block of memory for storing elements.
 *  It is useful for scenarios where the size of the array is known at compile time and does not change.
 *  \tparam T The type of elements stored in the fixed array.
 *  \tparam Sz The size of the fixed array. */
template <class T, SizeType Sz>
class FixedArray
{
public:
    static constexpr bool is_contiguous = true;

    T m_values[Sz > 1 ? Sz : 1];

    using Iterator = T*;
    using ConstIterator = const T*;

    using KeyType = SizeType;
    using ValueType = T;

    static constexpr SizeType size = Sz;

    template <class OtherType, SizeType OtherSize>
    HYP_FORCE_INLINE bool operator==(const FixedArray<OtherType, OtherSize>& other) const
    {
        if constexpr (Sz != OtherSize)
        {
            return false;
        }

        if (this == &other)
        {
            return true;
        }

        auto it = Begin();
        auto other_it = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++other_it)
        {
            if (!(*it == *other_it))
            {
                return false;
            }
        }

        return true;
    }

    template <class OtherType, SizeType OtherSize>
    HYP_FORCE_INLINE bool operator!=(const FixedArray<OtherType, OtherSize>& other) const
    {
        if constexpr (Sz != OtherSize)
        {
            return true;
        }

        if (this == &other)
        {
            return false;
        }

        auto it = Begin();
        auto other_it = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++other_it)
        {
            if (!(*it == *other_it))
            {
                return true;
            }
        }

        return false;
    }

    HYP_FORCE_INLINE bool Contains(const T& value) const
    {
        const containers::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
        return impl.Contains(value);
    }

    HYP_FORCE_INLINE T& operator[](KeyType index)
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE const T& operator[](KeyType index) const
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return Sz;
    }

    HYP_FORCE_INLINE constexpr SizeType ByteSize() const
    {
        return Sz * sizeof(T);
    }

    HYP_FORCE_INLINE constexpr bool Empty() const
    {
        return Sz == 0;
    }

    HYP_FORCE_INLINE constexpr bool Any() const
    {
        return Sz != 0;
    }

    HYP_FORCE_INLINE auto Sum() const
    {
        if constexpr (Sz == 0)
        {
            return T();
        }
        else
        {
            const containers::FixedArrayImpl<const T, Sz> impl(&m_values[0]);

            return impl.Sum();
        }
    }

    HYP_FORCE_INLINE auto Avg() const
    {
        if constexpr (Sz == 0)
        {
            return T();
        }
        else
        {
            const containers::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            return impl.Avg();
        }
    }

    template <class ConstIterator>
    HYP_FORCE_INLINE KeyType IndexOf(ConstIterator iter) const
    {
        if constexpr (Sz == 0)
        {
            return KeyType(-1);
        }
        else
        {
            const containers::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            return impl.IndexOf(iter);
        }
    }

    template <class OtherContainer>
    HYP_FORCE_INLINE bool CompareBitwise(const OtherContainer& other) const
    {
        if constexpr (Sz != OtherContainer::size)
        {
            return false;
        }
        else
        {
            const containers::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
            return impl.CompareBitwise(other);
        }
    }

    HYP_FORCE_INLINE T* Data()
    {
        return static_cast<T*>(m_values);
    }

    HYP_FORCE_INLINE const T* Data() const
    {
        return static_cast<const T*>(m_values);
    }

    HYP_FORCE_INLINE T& Front()
    {
        return m_values[0];
    }

    HYP_FORCE_INLINE const T& Front() const
    {
        return m_values[0];
    }

    HYP_FORCE_INLINE T& Back()
    {
        return m_values[Sz - 1];
    }

    HYP_FORCE_INLINE const T& Back() const
    {
        return m_values[Sz - 1];
    }

    /*! \brief Creates a Span<T> from the FixedArray's data.
     *  \return A Span<T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE operator Span<T>()
    {
        return Span<T>(&m_values[0], Sz);
    }

    /*! \brief Creates a Span<T> from the FixedArray's data.
     *  \return A Span<T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE operator Span<const T>() const
    {
        return Span<const T>(&m_values[0], Sz);
    }

    /*! \brief Creates a Span<T> from the FixedArray's data.
     *  \return A Span<T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE Span<T> ToSpan()
    {
        return Span<T>(Data(), Size());
    }

    /*! \brief Creates a Span<const T> from the FixedArray's data.
     *  \return A Span<const T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE Span<const T> ToSpan() const
    {
        return Span<const T>(Data(), Size());
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        const containers::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
        return impl.GetHashCode();
    }

    HYP_DEF_STL_BEGIN_END(&m_values[0], &m_values[Sz])
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

template <class T, SizeType Sz>
class FixedArrayImpl : public ContainerBase<FixedArrayImpl<T, Sz>, uint32>
{
public:
    T* ptr;

    static constexpr bool is_contiguous = true;

    using Iterator = T*;
    using ConstIterator = const T*;

    FixedArrayImpl(T* ptr)
        : ptr(ptr)
    {
    }

    HYP_NODISCARD HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return Sz;
    }

    HYP_DEF_STL_BEGIN_END(ptr, ptr + Sz)
};

// deduction guide
template <typename Tp, typename... Args>
FixedArray(Tp, Args...) -> FixedArray<std::enable_if_t<(std::is_same_v<Tp, Args> && ...), Tp>, 1 + sizeof...(Args)>;

} // namespace containers

template <class T, SizeType N>
using FixedArray = containers::FixedArray<T, N>;

template <class T, SizeType N>
constexpr uint32 ArraySize(const FixedArray<T, N>&)
{
    return N;
}

template <class T, SizeType N>
constexpr inline FixedArray<T, N> MakeFixedArray(const T (&values)[N])
{
    FixedArray<T, N> result;

    for (SizeType i = 0; i < N; i++)
    {
        result[i] = values[i];
    }

    return result;
}

template <class T, SizeType N>
constexpr inline FixedArray<T, N> MakeFixedArray(T* _begin, T* _end)
{
    FixedArray<T, N> result;

    for (SizeType i = 0; i < N && _begin != _end; i++, ++_begin)
    {
        result[i] = *_begin;
    }

    return result;
}

} // namespace hyperion

#endif

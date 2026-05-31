/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/ContainerBase.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

#include <algorithm>
#include <utility>

namespace Hyperion {

namespace containers {
template <class T, size_t Sz>
class FixedArray;

template <class T, size_t Sz>
class FixedArrayImpl;

/*! \brief FixedArray is a fixed-size array container that provides a contiguous block of memory for storing elements.
 *  It is useful for scenarios where the size of the array is known at compile time and does not change.
 *  \tparam T The type of elements stored in the fixed array.
 *  \tparam Sz The size of the fixed array. */
template <class T, size_t Sz>
class FixedArray
{
public:
    static constexpr bool isContiguous = true;

    T m_values[Sz > 1 ? Sz : 1];

    using Iterator = T*;
    using ConstIterator = const T*;

    using KeyType = size_t;
    using ValueType = T;

    static constexpr size_t size = Sz;

    template <class OtherType, size_t OtherSize>
    HYP_FORCE_INLINE constexpr bool operator==(const FixedArray<OtherType, OtherSize>& other) const
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
        auto otherIt = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++otherIt)
        {
            if (!(*it == *otherIt))
            {
                return false;
            }
        }

        return true;
    }

    template <class OtherType, size_t OtherSize>
    HYP_FORCE_INLINE constexpr bool operator!=(const FixedArray<OtherType, OtherSize>& other) const
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
        auto otherIt = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++otherIt)
        {
            if (!(*it == *otherIt))
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

    HYP_FORCE_INLINE constexpr T& operator[](KeyType index)
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE constexpr const T& operator[](KeyType index) const
    {
        return m_values[index];
    }

    HYP_FORCE_INLINE constexpr size_t Size() const
    {
        return Sz;
    }

    HYP_FORCE_INLINE constexpr size_t ByteSize() const
    {
        return sizeof(m_values);
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

    HYP_FORCE_INLINE constexpr T* Data()
    {
        return static_cast<T*>(m_values);
    }

    HYP_FORCE_INLINE constexpr const T* Data() const
    {
        return static_cast<const T*>(m_values);
    }

    HYP_FORCE_INLINE constexpr T& Front()
    {
        return m_values[0];
    }

    HYP_FORCE_INLINE constexpr const T& Front() const
    {
        return m_values[0];
    }

    HYP_FORCE_INLINE constexpr T& Back()
    {
        return m_values[Sz - 1];
    }

    HYP_FORCE_INLINE constexpr const T& Back() const
    {
        return m_values[Sz - 1];
    }

    /*! \brief Creates a Span<T> from the FixedArray's data.
     *  \return A Span<T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE constexpr operator Span<T>()
    {
        return Span<T>(Begin(), End());
    }

    /*! \brief Creates a Span<T> from the FixedArray's data.
     *  \return A Span<T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE constexpr operator Span<const T>() const
    {
        return Span<const T>(Begin(), End());
    }

    /*! \brief Creates a Span<T> from the FixedArray's data.
     *  \return A Span<T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE constexpr Span<T> ToSpan()
    {
        return Span<T>(Begin(), End());
    }

    /*! \brief Creates a Span<const T> from the FixedArray's data.
     *  \return A Span<const T> of the FixedArray's data. */
    HYP_NODISCARD HYP_FORCE_INLINE constexpr Span<const T> ToSpan() const
    {
        return Span<const T>(Begin(), End());
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        const containers::FixedArrayImpl<const T, Sz> impl(&m_values[0]);
        return impl.GetHashCode();
    }

    HYP_DEF_STL_BEGIN_END_CONSTEXPR(m_values, m_values + Sz)
};

// template <class T, size_t Sz>
// FixedArray<T, Sz>::FixedArray()
//     : m_values{}
// {
// }

// template <class T, size_t Sz>
// FixedArray<T, Sz>::FixedArray(const FixedArray &other)
// {
//     for (size_t i = 0; i < Sz; i++) {
//         m_values[i] = other.m_values[i];
//     }
// }

// template <class T, size_t Sz>
// auto FixedArray<T, Sz>::operator=(const FixedArray &other) -> FixedArray&
// {
//     for (size_t i = 0; i < Sz; i++) {
//         m_values[i] = other.m_values[i];
//     }

//     return *this;
// }

// template <class T, size_t Sz>
// FixedArray<T, Sz>::FixedArray(FixedArray &&other) noexcept
// {
//     for (size_t i = 0; i < Sz; i++) {
//         m_values[i] = std::move(other.m_values[i]);
//     }
// }

// template <class T, size_t Sz>
// auto FixedArray<T, Sz>::operator=(FixedArray &&other) noexcept -> FixedArray&
// {
//     for (size_t i = 0; i < Sz; i++) {
//         m_values[i] = std::move(other.m_values[i]);
//     }

//     return *this;
// }

// template <class T, size_t Sz>
// FixedArray<T, Sz>::~FixedArray() = default;

template <class T, size_t Sz>
class FixedArrayImpl : public ContainerBase<FixedArrayImpl<T, Sz>, uint32>
{
public:
    T* ptr;

    static constexpr bool isContiguous = true;

    using Iterator = T*;
    using ConstIterator = const T*;

    FixedArrayImpl(T* ptr)
        : ptr(ptr)
    {
    }

    HYP_NODISCARD HYP_FORCE_INLINE constexpr size_t Size() const
    {
        return Sz;
    }

    HYP_DEF_STL_BEGIN_END_CONSTEXPR(ptr, ptr + Sz)
};

// deduction guide
template <typename Tp, typename... Args>
FixedArray(Tp, Args...) -> FixedArray<std::enable_if_t<(std::is_same_v<Tp, Args> && ...), Tp>, 1 + sizeof...(Args)>;

} // namespace containers

template <class T, size_t N>
using FixedArray = containers::FixedArray<T, N>;

template <class T, size_t N>
constexpr uint32 ArraySize(const FixedArray<T, N>&)
{
    return N;
}

template <class T, size_t N>
constexpr inline FixedArray<T, N> MakeFixedArray(const T (&values)[N])
{
    FixedArray<T, N> result;

    for (size_t i = 0; i < N; i++)
    {
        result[i] = values[i];
    }

    return result;
}

template <class... Ts>
constexpr inline auto MakeFixedArray(Ts&&... values)
{
    return FixedArray<std::common_type_t<Ts...>, sizeof...(Ts)> { std::forward<Ts>(values)... };
}

template <class T, size_t Sz>
struct IsFixedArray<Hyperion::containers::FixedArray<T, Sz>> : std::true_type
{
};

} // namespace Hyperion

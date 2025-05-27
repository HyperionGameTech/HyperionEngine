/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SPAN_HPP
#define HYPERION_SPAN_HPP

#include <core/Defines.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace utilities {

template <class T, class T2 = void>
struct Span;

template <class T>
struct Span<T, std::enable_if_t<!std::is_const_v<T>>>
{
    using Type = T;

    using Iterator = Type*;
    using ConstIterator = Type*;

    Type* first;
    Type* last;

    constexpr Span()
        : first(nullptr),
          last(nullptr)
    {
    }

    constexpr Span(std::nullptr_t)
        : first(nullptr),
          last(nullptr)
    {
    }

    constexpr Span(const Span& other) = default;
    Span& operator=(const Span& other) = default;

    constexpr Span(Span&& other) noexcept
        : first(other.first),
          last(other.last)
    {
        other.first = nullptr;
        other.last = nullptr;
    }

    Span& operator=(Span&& other) noexcept
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        first = other.first;
        last = other.last;

        other.first = nullptr;
        other.last = nullptr;

        return *this;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_convertible_v<std::add_pointer_t<OtherT>, std::add_pointer_t<T>>>>
    constexpr Span(const Span<OtherT>& other)
        : first(other.first),
          last(other.last)
    {
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_convertible_v<std::add_pointer_t<OtherT>, std::add_pointer_t<T>>>>
    Span& operator=(const Span<OtherT>& other)
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        first = other.first;
        last = other.last;

        return *this;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_convertible_v<std::add_pointer_t<OtherT>, std::add_pointer_t<T>>>>
    constexpr Span(Span<OtherT>&& other)
        : first(other.first),
          last(other.last)
    {
        other.first = nullptr;
        other.last = nullptr;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_convertible_v<std::add_pointer_t<OtherT>, std::add_pointer_t<T>>>>
    Span& operator=(Span<OtherT>&& other)
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        first = other.first;
        last = other.last;

        other.first = nullptr;
        other.last = nullptr;

        return *this;
    }

    constexpr Span(Type* first, Type* last)
        : first(first),
          last(last)
    {
    }

    constexpr Span(Type* first, SizeType size)
        : first(first),
          last(first + size)
    {
    }

    template <SizeType Size>
    constexpr Span(Type (&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    template <SizeType Size>
    constexpr Span(Type (&&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    constexpr ~Span() = default;

    HYP_FORCE_INLINE constexpr bool operator==(const Span& other) const
    {
        return first == other.first && last == other.last;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const Span& other) const
    {
        return first != other.first || last != other.last;
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return ptrdiff_t(last - first) <= 0;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return ptrdiff_t(last - first) > 0;
    }

    HYP_FORCE_INLINE constexpr Type& operator[](SizeType index) const
    {
        return first[index];
    }

    HYP_FORCE_INLINE Span operator+(ptrdiff_t amount) const
    {
        return Span(first + amount, last);
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return SizeType(last - first);
    }

    HYP_FORCE_INLINE constexpr Type* Data() const
    {
        return first;
    }

    HYP_FORCE_INLINE Span Slice(SizeType offset, SizeType count) const
    {
        if (offset >= Size())
        {
            return Span();
        }

        if (count == 0)
        {
            return Span();
        }

        if (offset + count > Size())
        {
            count = Size() - offset;
        }

        return Span(first + offset, first + offset + count);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(reinterpret_cast<const char*>(Begin()), reinterpret_cast<const char*>(End()));
    }

    HYP_FORCE_INLINE constexpr operator Span<const Type>() const
    {
        return Span<const Type>(first, last);
    }

    HYP_DEF_STL_BEGIN_END_CONSTEXPR(
        first,
        last)
};

template <class T>
struct Span<T, std::enable_if_t<std::is_const_v<T>>>
{
    using Type = std::remove_const_t<T>;

    using Iterator = const Type*;
    using ConstIterator = const Type*;

    const Type* first;
    const Type* last;

    constexpr Span()
        : first(nullptr),
          last(nullptr)
    {
    }

    constexpr Span(std::nullptr_t)
        : first(nullptr),
          last(nullptr)
    {
    }

    constexpr Span(const Span& other) = default;
    Span& operator=(const Span& other) = default;

    constexpr Span(Span&& other) noexcept
        : first(other.first),
          last(other.last)
    {
        other.first = nullptr;
        other.last = nullptr;
    }

    Span& operator=(Span&& other) noexcept
    {
        if (std::addressof(other) == this)
        {
            return *this;
        }

        first = other.first;
        last = other.last;

        other.first = nullptr;
        other.last = nullptr;

        return *this;
    }

    constexpr Span(const Type* first, const Type* last)
        : first(first),
          last(last)
    {
    }

    constexpr Span(const Type* first, SizeType size)
        : first(first),
          last(first + size)
    {
    }

    template <SizeType Size>
    constexpr Span(Type (&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    template <SizeType Size>
    constexpr Span(const Type (&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    template <SizeType Size>
    constexpr Span(Type (&&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    template <SizeType Size>
    constexpr Span(const Type (&&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    constexpr Span(std::initializer_list<T> initializer_list)
        : Span(initializer_list.begin(), initializer_list.end())
    {
    }

    constexpr ~Span() = default;

    HYP_FORCE_INLINE constexpr bool operator==(const Span& other) const
    {
        return first == other.first && last == other.last;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const Span& other) const
    {
        return first != other.first || last != other.last;
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return ptrdiff_t(last - first) <= 0;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return ptrdiff_t(last - first) > 0;
    }

    HYP_FORCE_INLINE constexpr const Type& operator[](SizeType index) const
    {
        return first[index];
    }

    HYP_FORCE_INLINE Span operator+(ptrdiff_t amount) const
    {
        return Span(first + amount, last);
    }

    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return SizeType(last - first);
    }

    HYP_FORCE_INLINE constexpr const Type* Data() const
    {
        return first;
    }

    HYP_FORCE_INLINE Span Slice(SizeType offset, SizeType count = SizeType(-1)) const
    {
        if (offset >= Size())
        {
            return Span();
        }

        if (count == 0)
        {
            return Span();
        }

        const SizeType max_size = Size() - offset;

        if (count > max_size)
        {
            count = max_size;
        }

        return Span(first + offset, first + offset + count);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(reinterpret_cast<const char*>(Begin()), reinterpret_cast<const char*>(End()));
    }

    HYP_DEF_STL_BEGIN_END_CONSTEXPR(
        first,
        last)
};

using ByteView = Span<ubyte>;
using ConstByteView = Span<const ubyte>;

} // namespace utilities

template <class T>
using Span = utilities::Span<T>;

using ByteView = utilities::ByteView;
using ConstByteView = utilities::ConstByteView;

} // namespace hyperion

#endif
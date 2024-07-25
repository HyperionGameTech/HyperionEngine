/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SPAN_HPP
#define HYPERION_SPAN_HPP

#include <core/Defines.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace utilities {

template <class T>
struct Span
{
    using Iterator = T *;
    using ConstIterator = const T *;

    T   *first;
    T   *last;

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

    constexpr Span(const Span &other) = default;

    template <class OtherT>
    constexpr Span(const Span<OtherT> &other)
        : first(other.first),
          last(other.last)
    {
    }

    Span &operator=(const Span &other) = default;

    template <class OtherT, typename = std::enable_if_t<std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<OtherT>>>>
    Span &operator=(const Span<OtherT> &other)
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        first = other.first;
        last = other.last;

        return *this;
    }

    constexpr Span(Span &&other) noexcept
        : first(other.first),
          last(other.last)
    {
        other.first = nullptr;
        other.last = nullptr;
    }

    template <class OtherT, typename = std::enable_if_t<std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<OtherT>>>>
    constexpr Span(Span<OtherT> &&other) noexcept
        : first(other.first),
          last(other.last)
    {
        other.first = nullptr;
        other.last = nullptr;
    }

    Span &operator=(Span &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        first = other.first;
        last = other.last;

        other.first = nullptr;
        other.last = nullptr;

        return *this;
    }

    template <class OtherT, typename = std::enable_if_t<std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<OtherT>>>>
    Span &operator=(Span<OtherT> &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        first = other.first;
        last = other.last;

        other.first = nullptr;
        other.last = nullptr;

        return *this;
    }

    constexpr Span(T *first, T *last)
        : first(first),
          last(last)
    {
    }

    constexpr Span(T *first, SizeType size)
        : first(first),
          last(first + size)
    {
    }

    template <SizeType Size>
    constexpr Span(T (&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    template <SizeType Size>
    constexpr Span(T (&&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    constexpr ~Span() = default;

    HYP_FORCE_INLINE constexpr bool operator==(const Span &other) const
        { return first == other.first && last == other.last; }

    HYP_FORCE_INLINE constexpr bool operator!=(const Span &other) const
        { return first != other.first || last != other.last; }

    HYP_FORCE_INLINE constexpr bool operator!() const
        { return ptrdiff_t(last - first) <= 0; }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
        { return ptrdiff_t(last - first) > 0; }

    HYP_FORCE_INLINE constexpr T &operator[](SizeType index)
        { return first[index]; }

    HYP_FORCE_INLINE constexpr const T &operator[](SizeType index) const
        { return first[index]; }

    HYP_FORCE_INLINE Span operator+(ptrdiff_t amount) const
        { return Span(first + amount, last); }

    HYP_FORCE_INLINE constexpr SizeType Size() const
        { return SizeType(last - first); }

    HYP_FORCE_INLINE constexpr T *Data()
        { return first; }

    HYP_FORCE_INLINE constexpr const T *Data() const
        { return first; }

    HYP_FORCE_INLINE Span Slice(SizeType offset, SizeType count) const
    {
        if (offset >= Size()) {
            return Span();
        }

        if (count == 0) {
            return Span();
        }

        if (offset + count > Size()) {
            count = Size() - offset;
        }

        return Span(first + offset, first + offset + count);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(reinterpret_cast<const char *>(Begin()), reinterpret_cast<const char *>(End())); }

    HYP_DEF_STL_BEGIN_END_CONSTEXPR(
        first,
        last
    )
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
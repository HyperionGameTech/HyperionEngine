/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIB_SPAN_HPP
#define HYPERION_V2_LIB_SPAN_HPP

#include <core/Defines.hpp>
#include <Types.hpp>

namespace hyperion {

template <class T>
struct Span
{
    using Iterator = T *;
    using ConstIterator = const T *;

    T *first;
    T *last;

    Span()
        : first(nullptr),
          last(nullptr)
    {
    }

    Span(const Span &other)
        : first(other.first),
          last(other.last)
    {
    }

    Span &operator=(const Span &other)
    {
        first = other.first;
        last = other.last;

        return *this;
    }

    Span(Span &&other) noexcept
        : first(other.first),
          last(other.last)
    {
        other.first = nullptr;
        other.last = nullptr;
    }

    Span &operator=(Span &&other) noexcept
    {
        first = other.first;
        last = other.last;

        other.first = nullptr;
        other.last = nullptr;

        return *this;
    }

    ~Span() = default;

    Span(T *first, T *last)
        : first(first),
          last(last)
    {
    }

    Span(T *first, SizeType size)
        : first(first),
          last(first + size)
    {
    }

    template <SizeType Size>
    Span(const T (&ary)[Size])
        : first(&ary[0]),
          last(&ary[Size])
    {
    }

    HYP_FORCE_INLINE
    bool operator==(const Span &other) const
        { return first == other.first && last == other.last; }

    HYP_FORCE_INLINE
    bool operator!=(const Span &other) const
        { return first != other.first || last != other.last; }

    HYP_FORCE_INLINE
    bool operator!() const
        { return ptrdiff_t(last - first) <= 0; }

    HYP_FORCE_INLINE
    explicit operator bool() const
        { return ptrdiff_t(last - first) > 0; }

    HYP_FORCE_INLINE
    T &operator[](SizeType index)
        { return first[index]; }

    HYP_FORCE_INLINE
    const T &operator[](SizeType index) const
        { return first[index]; }

    HYP_FORCE_INLINE
    SizeType Size() const
        { return SizeType(last - first); }

    HYP_FORCE_INLINE
    T *Data()
        { return first; }

    HYP_FORCE_INLINE
    const T *Data() const
        { return first; }

    HYP_DEF_STL_BEGIN_END(
        first,
        last
    )
};

} // namespace hyperion

#endif
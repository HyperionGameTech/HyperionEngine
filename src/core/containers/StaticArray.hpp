/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STATIC_ARRAY_HPP
#define HYPERION_STATIC_ARRAY_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

#include <array>

namespace hyperion {
namespace containers {

template <class T, SizeType Size>
struct StaticArray
{
    static constexpr SizeType size = Size;

    using HeldType = T;

    std::array<T, Size> items;

    using Iterator = typename std::array<T, Size>::iterator;
    using ConstIterator = typename std::array<T, Size>::const_iterator;

    constexpr const T& operator[](SizeType index) const
    {
        return items[index];
    }

    template <auto OtherStaticArray>
    constexpr auto Concat() const
    {
        static_assert(std::is_same_v<typename decltype(OtherStaticArray)::HeldType, HeldType>, "Held types differ, cannot concat static arrays");

        if constexpr (Size == 0 && decltype(OtherStaticArray)::size == 0)
        {
            return StaticArray<T, 0> {};
        }
        else if constexpr (Size == 0)
        {
            return OtherStaticArray;
        }
        else if constexpr (decltype(OtherStaticArray)::size == 0)
        {
            return StaticArray<T, Size> { items };
        }
        else
        {
            return ConcatImpl<OtherStaticArray>(std::make_index_sequence<Size + decltype(OtherStaticArray)::size>());
        }
    }

    constexpr Iterator Begin()
    {
        return items.begin();
    }

    constexpr Iterator End()
    {
        return items.end();
    }

    constexpr ConstIterator Begin() const
    {
        return items.begin();
    }

    constexpr ConstIterator End() const
    {
        return items.end();
    }

    constexpr Iterator begin()
    {
        return items.begin();
    }

    constexpr Iterator end()
    {
        return items.end();
    }

    constexpr ConstIterator begin() const
    {
        return items.begin();
    }

    constexpr ConstIterator end() const
    {
        return items.end();
    }

    /// impl
    template <auto OtherStaticArray, SizeType... Indices>
    constexpr auto ConcatImpl(std::index_sequence<Indices...>) const -> StaticArray<T, Size + decltype(OtherStaticArray)::size>
    {
        return {
            { (Indices < Size ? items[Indices] : OtherStaticArray.items[Indices - Size])... }
        };
    }
};

// Concat

template <auto... Arrays>
struct ConcatStaticArrays;

template <auto Last>
struct ConcatStaticArrays<Last>
{
    static constexpr auto value = Last;
};

template <auto First, auto... Rest>
struct ConcatStaticArrays<First, Rest...>
{
    static constexpr auto value = First.template Concat<ConcatStaticArrays<Rest...>::value>();
};

} // namespace containers

template <class T, SizeType Size>
using StaticArray = containers::StaticArray<T, Size>;

using containers::ConcatStaticArrays;

} // namespace hyperion

#endif
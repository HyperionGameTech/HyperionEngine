/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STATIC_MAP_HPP
#define HYPERION_STATIC_MAP_HPP

#include <core/Defines.hpp>

#include <array>

namespace hyperion {
namespace containers {

template <class Key, class Value, SizeType Size>
struct StaticMap
{
    static constexpr SizeType size = Size;

    using KeyType = Key;
    using ValueType = Value;

    std::array<std::pair<Key, Value>, Size> pairs;

    using Iterator = typename std::array<std::pair<Key, Value>, Size>::iterator;
    using ConstIterator = typename std::array<std::pair<Key, Value>, Size>::const_iterator;

    // constexpr const Value &operator[](const Key &key) const
    // {
    //     for (auto it = pairs.begin(); it != pairs.end(); ++it) {
    //         if (it->first == key) {
    //             return it->second;
    //         }
    //     }

    //     throw std::range_error("Key not found");
    // }

    template <auto OtherStaticMap>
    constexpr auto Concat() const
    {
        static_assert(std::is_same_v<decltype(OtherStaticMap)::KeyType, KeyType>, "Key types differ, cannot concat static maps");
        static_assert(std::is_same_v<decltype(OtherStaticMap)::ValueType, ValueType>, "Value types differ, cannot concat static maps");

        if constexpr (Size == 0 && decltype(OtherStaticMap)::size == 0)
        {
            return StaticMap<Key, Value, 0> {};
        }
        else if constexpr (Size == 0)
        {
            return OtherStaticMap;
        }
        else if constexpr (decltype(OtherStaticMap)::size == 0)
        {
            return StaticMap<Key, Value, Size> { pairs };
        }
        else
        {
            return ConcatImpl<OtherStaticMap>(std::make_index_sequence<Size + decltype(OtherStaticMap)::size>());
        }
    }

    [[nodiscard]] constexpr Iterator Begin()
    {
        return pairs.begin();
    }

    [[nodiscard]] constexpr Iterator End()
    {
        return pairs.end();
    }

    [[nodiscard]] constexpr ConstIterator Begin() const
    {
        return pairs.begin();
    }

    [[nodiscard]] constexpr ConstIterator End() const
    {
        return pairs.end();
    }

    [[nodiscard]] constexpr Iterator begin()
    {
        return pairs.begin();
    }

    [[nodiscard]] constexpr Iterator end()
    {
        return pairs.end();
    }

    [[nodiscard]] constexpr ConstIterator begin() const
    {
        return pairs.begin();
    }

    [[nodiscard]] constexpr ConstIterator end() const
    {
        return pairs.end();
    }

    /// impl
    template <auto OtherStaticMap, SizeType... Indices>
    constexpr auto ConcatImpl(std::index_sequence<Indices...>) const -> StaticMap<Key, Value, Size + decltype(OtherStaticMap)::size>
    {
        return {
            { (Indices < Size ? pairs[Indices] : OtherStaticMap.pairs[Indices - Size])... }
        };
    }
};

// Concat

template <auto... Maps>
struct ConcatStaticMaps;

template <auto Last>
struct ConcatStaticMaps<Last>
{
    static constexpr auto value = Last;
};

template <auto First, auto... Rest>
struct ConcatStaticMaps<First, Rest...>
{
    static constexpr auto value = First.template Concat<ConcatStaticMaps<Rest...>::value>();
};

} // namespace containers

template <class Key, class Value, SizeType Size>
using StaticMap = containers::StaticMap<Key, Value, Size>;

using containers::ConcatStaticMaps;

} // namespace hyperion

#endif
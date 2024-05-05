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
    std::array<std::pair<Key, Value>, Size> pairs;

    using Iterator = typename std::array<std::pair<Key, Value>, Size>::iterator;
    using ConstIterator = typename std::array<std::pair<Key, Value>, Size>::const_iterator;
    
    constexpr const Value &operator[](const Key &key) const
    {
        for (auto it = pairs.begin(); it != pairs.end(); ++it) {
            if (it->first == key) {
                return it->second;
            }
        }

        throw std::range_error("Key not found");
    }

    [[nodiscard]] constexpr Iterator Begin() { return pairs.begin(); }
    [[nodiscard]] constexpr Iterator End() { return pairs.end(); }
    [[nodiscard]] constexpr ConstIterator Begin() const { return pairs.begin(); }
    [[nodiscard]] constexpr ConstIterator End() const { return pairs.end(); }
    [[nodiscard]] constexpr Iterator begin() { return pairs.begin(); }
    [[nodiscard]] constexpr Iterator end() { return pairs.end(); }
    [[nodiscard]] constexpr ConstIterator begin() const  { return pairs.begin(); }
    [[nodiscard]] constexpr ConstIterator end() const { return pairs.end(); }
};
} // namespace containers

template <class Key, class Value, SizeType Size>
using StaticMap = containers::StaticMap<Key, Value, Size>;

} // namespace hyperion

#endif
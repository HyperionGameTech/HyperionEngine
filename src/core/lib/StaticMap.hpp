#ifndef HYPERION_STATIC_MAP_H
#define HYPERION_STATIC_MAP_H

#include <util/Defines.hpp>

#include <array>
#include <utility>

namespace hyperion {

template <class Key, class Value, size_t Size>
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

} // namespace hyperion

#endif
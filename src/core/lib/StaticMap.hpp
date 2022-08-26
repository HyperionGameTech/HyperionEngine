#ifndef HYPERION_STATIC_MAP_H
#define HYPERION_STATIC_MAP_H

namespace hyperion {

template <class Key, class Value, size_t Size>
struct StaticMap
{
    std::array<std::pair<Key, Value>, Size> pairs;
    
    constexpr Value &operator[](const Key &key) const
    {
        const auto it = std::find_if(
            pairs.begin(),
            pairs.end(),
            [&key](const auto &v) {
                return v.first == key;
            });

        if (it != pairs.end()) {
            return it->second;
        }

        throw std::range_error("Key not found");
    }
};

} // namespace hyperion

#endif
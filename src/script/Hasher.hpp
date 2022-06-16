#ifndef HASHER_HPP
#define HASHER_HPP

#include <cstdint>

namespace hyperion {

using HashFnv1 = uint32_t;

constexpr HashFnv1 hash_fnv_1(const char *str)
{
    constexpr uint32_t PRIME = 16777619u;
    constexpr uint32_t OFFSET_BASIS = 2166136261u;

    uint32_t hash = OFFSET_BASIS;

    char c = 0;

    while ((c = *str++)) {
        hash *= PRIME;
        hash ^= c;
    }

    return hash;
}

} // namespace hyperion

#endif
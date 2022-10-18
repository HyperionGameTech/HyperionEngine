#ifndef HYPERION_HASHER_HPP
#define HYPERION_HASHER_HPP

#include <Types.hpp>

namespace hyperion {

using HashFNV1 = UInt32;

constexpr HashFNV1 hash_fnv_1(const char *str)
{
    constexpr UInt32 prime = 16777619u;
    constexpr UInt32 offset_basis = 2166136261u;

    UInt32 hash = offset_basis;

    char c = 0;

    while ((c = *str++)) {
        hash *= prime;
        hash ^= c;
    }

    return hash;
}

} // namespace hyperion

#endif
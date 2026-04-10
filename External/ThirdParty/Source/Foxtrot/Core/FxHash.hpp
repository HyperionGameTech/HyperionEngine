#pragma once

#include "FxTypes.hpp"

#include <Core/FxSlice.hpp>

#define FX_HASH32_FNV1A_INIT 0x811C9DC5
// #define FX_HASH_FNV1A_PRIME  0x01000193

#define FX_HASH64_FNV1A_INIT (0xCBF29CE484222325ULL)

using FxHash32 = uint32;
using FxHash64 = uint64;

static constexpr FxHash64 FxHashNull64 = UINT64_MAX;
static constexpr FxHash32 FxHashNull32 = UINT32_MAX;

/////////////////////////////////////
// 32-bit hashing functions
/////////////////////////////////////


template <typename TObj>
inline constexpr FxHash32 FxHashData32(const FxSlice<TObj>& slice, FxHash32 thash = FX_HASH32_FNV1A_INIT)
{
    uint8* buffer_start = reinterpret_cast<uint8*>(slice.pData);
    uint8* buffer_end = buffer_start + slice.Size;

    while (buffer_start < buffer_end) {
        /* xor the bottom with the current octet */
        thash ^= static_cast<FxHash32>(*buffer_start);

        /* multiply by the 32 bit FNV magic prime mod 2^32 */
        thash += (thash << 1) + (thash << 4) + (thash << 7) + (thash << 8) + (thash << 24);

        ++buffer_start;
    }

    /* return our new hash value */
    return thash;
}

template <typename TObj>
inline FxHash32 FxHashObj32(const TObj& obj, FxHash32 thash = FX_HASH32_FNV1A_INIT)
{
    const uint8* start = reinterpret_cast<const uint8*>(&obj);
    const uint8* end = start + sizeof(TObj);

    while (start < end) {
        /* xor the bottom with the current octet */
        thash ^= static_cast<FxHash32>(*start);

        /* multiply by the 64 bit FNV magic prime mod 2^64 */
        thash += (thash << 1) + (thash << 4) + (thash << 7) + (thash << 8) + (thash << 24);

        ++start;
    }

    return thash;
}

/**
 * Hashes a string at compile time using FNV-1a.
 *
 * Source to algorithm: http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
 */
inline constexpr FxHash32 FxHashStr32(const char* str, FxHash32 thash = FX_HASH32_FNV1A_INIT)
{
    uint8 ch;
    while ((ch = static_cast<uint8>(*(str++)))) {
        thash ^= static_cast<FxHash32>(ch);
        thash += (thash << 1) + (thash << 4) + (thash << 7) + (thash << 8) + (thash << 24);
    }

    return thash;
}


/////////////////////////////////////
// 64-bit hashing functions
/////////////////////////////////////


template <typename TObj>
inline constexpr FxHash64 FxHashData64(const FxSlice<TObj>& slice, FxHash64 thash = FX_HASH64_FNV1A_INIT)
{
    uint8* buffer_start = reinterpret_cast<uint8*>(slice.pData);
    uint8* buffer_end = buffer_start + slice.Size;

    /*
     * FNV-1a hash each octet of the buffer
     */
    while (buffer_start < buffer_end) {
        /* xor the bottom with the current octet */
        thash ^= static_cast<FxHash64>(*buffer_start);

        /* multiply by the 64 bit FNV magic prime mod 2^64 */
        thash += (thash << 1) + (thash << 4) + (thash << 5) + (thash << 7) + (thash << 8) + (thash << 40);

        ++buffer_start;
    }

    return thash;
}


template <typename TObj>
inline FxHash64 FxHashObj64(const TObj& obj, FxHash64 thash = FX_HASH64_FNV1A_INIT)
{
    const uint8* start = reinterpret_cast<const uint8*>(&obj);
    const uint8* end = start + sizeof(TObj);


    while (start < end) {
        /* xor the bottom with the current octet */
        thash ^= static_cast<FxHash64>(*start);

        /* multiply by the 64 bit FNV magic prime mod 2^64 */
        thash += (thash << 1) + (thash << 4) + (thash << 5) + (thash << 7) + (thash << 8) + (thash << 40);

        ++start;
    }

    return thash;
}

inline constexpr FxHash64 FxHashStr64(const char* str, FxHash64 thash = FX_HASH64_FNV1A_INIT)
{
    uint8 ch = 0;
    while ((ch = *str)) {
        /* xor the bottom with the current octet */
        thash ^= static_cast<FxHash64>(ch);

        /* multiply by the 64 bit FNV magic prime mod 2^64 */
        thash += (thash << 1) + (thash << 4) + (thash << 5) + (thash << 7) + (thash << 8) + (thash << 40);

        ++str;
    }

    return thash;
}


struct FxHash64Stl
{
    std::size_t operator()(FxHash64 key) const { return key; }
};


struct FxHash32Stl
{
    std::size_t operator()(FxHash32 key) const { return key; }
};

#ifndef HYPERION_CONSTANTS_H
#define HYPERION_CONSTANTS_H

#include <util/Defines.hpp>
#include <Types.hpp>

#include <type_traits>
#include <utility>

namespace hyperion {

constexpr uint8 engine_major_version = 1;
constexpr uint8 engine_minor_version = 0;
constexpr uint8 engine_patch_version = 0;
constexpr uint32 engine_version = (engine_major_version << 16) | (engine_minor_version << 8) | engine_patch_version;
constexpr uint64 engine_binary_magic_number = (uint64(0x505948) << 32) | engine_version;

constexpr uint max_frames_in_flight = 2;
constexpr uint num_async_rendering_command_buffers = 4;
constexpr uint num_async_compute_command_buffers = 1;

constexpr uint max_bound_reflection_probes = 16;
constexpr uint max_bound_ambient_probes = 1024;
constexpr uint max_bound_light_field_probes = max_bound_ambient_probes;
constexpr uint max_bound_point_shadow_maps = 16;
constexpr uint max_bound_environment_maps = 1;

constexpr uint num_gbuffer_textures = 6;

constexpr bool use_indexed_array_for_object_data = true;
// perform occlusion culling using indirect draw
constexpr bool use_draw_indirect = true;
constexpr bool use_parallel_rendering = HYP_FEATURES_PARALLEL_RENDERING;

template <class ...T>
constexpr bool resolution_failure = false;

template <class T>
using NormalizedType = std::remove_cv_t<std::decay_t<T>>;

template <class T>
constexpr bool IsPODType = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

template <class T, SizeType = sizeof(T)>
std::true_type implementation_exists_impl(T *);

std::false_type implementation_exists_impl(...);

template <class T>
constexpr bool implementation_exists = decltype(implementation_exists_impl(std::declval<T*>()))::value;

template <class T>
using RemoveConstPointer = std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<T>>>;

static inline bool IsBigEndian()
{
    constexpr union { uint32 i; uint8 ch[sizeof(uint32)]; } u = { 0x01020304 };

    return u.ch[0] == 1;
}

static inline bool IsLittleEndian() { return !IsBigEndian(); }

constexpr uint8 SwapEndianness(uint8 value)
{
    return value;
}

constexpr uint16 SwapEndianness(uint16 value)
{
    return uint16(((value >> 8u) & 0xffu) | ((value & 0xffu) << 8u));
}

constexpr uint32 SwapEndianness(uint32 value)
{
    return ((value & 0xff000000u) >> 24u) |
        ((value & 0x00ff0000u) >>  8u) |
        ((value & 0x0000ff00u) <<  8u) |
        ((value & 0x000000ffu) << 24u);
}

constexpr uint64 SwapEndianness(uint64 value)
{
    return ((value & 0xff00000000000000ull) >> 56ull) |
        ((value & 0x00ff000000000000ull) >> 40ull) |
        ((value & 0x0000ff0000000000ull) >> 24ull) |
        ((value & 0x000000ff00000000ull) >> 8ull) |
        ((value & 0x00000000ff000000ull) << 8ull) |
        ((value & 0x0000000000ff0000ull) << 24ull) |
        ((value & 0x000000000000ff00ull) << 40ull) |
        ((value & 0x00000000000000ffull) << 56ull);
}

constexpr int8 SwapEndianness(int8 value)
{
    return value;
}

constexpr int16 SwapEndianness(int16 value)
{
    union { uint16 u; int16 i; };
    i = value;

    u = SwapEndianness(u);

    return i;
}

constexpr int32 SwapEndianness(int32 value)
{
    union { uint32 u; int32 i; };
    i = value;

    u = SwapEndianness(u);

    return i;
}

constexpr int64 SwapEndianness(int64 value)
{
    union { uint64 u; int64 i; };
    i = value;

    u = SwapEndianness(u);

    return i;
}

} // namespace hyperion

#endif

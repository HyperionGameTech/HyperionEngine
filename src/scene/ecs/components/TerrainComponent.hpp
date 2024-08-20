/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_TERRAIN_COMPONENT_HPP
#define HYPERION_ECS_TERRAIN_COMPONENT_HPP

#include <math/Vector3.hpp>
#include <math/Extent.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/Defines.hpp>

#include <HashCode.hpp>

namespace hyperion {

using TerrainPatchCoord = Vec2i;

enum class TerrainPatchState
{
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

struct TerrainPatchNeighbor
{
    TerrainPatchCoord coord { };

    HYP_FORCE_INLINE Vec2f GetCenter() const
        { return Vec2f(coord) - 0.5f; }
};

struct TerrainPatchInfo
{
    Extent3D                            extent;
    TerrainPatchCoord                   coord { 0, 0 };
    Vec3f                               scale { 1.0f };
    TerrainPatchState                   state { TerrainPatchState::UNLOADED };
    float                               unload_timer { 0.0f };
    FixedArray<TerrainPatchNeighbor, 8> neighbors { };

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(Vec3u(extent));
        hash_code.Add(coord);
        hash_code.Add(scale);
        hash_code.Add(state);

        return hash_code;
    }
};

using TerrainComponentFlags = uint32;

enum TerrainComponentFlagBits : TerrainComponentFlags
{
    TERRAIN_COMPONENT_FLAG_NONE = 0x0,
    TERRAIN_COMPONENT_FLAG_INIT = 0x1
};

HYP_STRUCT()
struct TerrainPatchComponent
{
    HYP_FIELD()
    TerrainPatchInfo    patch_info;

    HYP_FORCE_INLINE Vec2f GetCenter() const
        { return Vec2f(patch_info.coord) - 0.5f; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(patch_info);

        return hash_code;
    }
};

HYP_STRUCT()
struct TerrainComponent
{
    HYP_FIELD()
    uint32                  seed = 0;

    HYP_FIELD()
    Extent3D                patch_size = { 32, 32, 32 };

    HYP_FIELD()
    Vec3f                   scale = Vec3f::One();

    HYP_FIELD()
    float                   max_distance = 2.0f;

    HYP_FIELD()
    TerrainComponentFlags   flags = TERRAIN_COMPONENT_FLAG_NONE;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(seed);
        hash_code.Add(Vec3u(patch_size));
        hash_code.Add(scale);
        hash_code.Add(max_distance);

        return hash_code;
    }
};

} // namespace hyperion

#endif
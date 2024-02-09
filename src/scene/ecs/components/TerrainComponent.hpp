#ifndef HYPERION_V2_ECS_TERRAIN_COMPONENT_HPP
#define HYPERION_V2_ECS_TERRAIN_COMPONENT_HPP

#include <math/Vector3.hpp>
#include <math/Extent.hpp>

#include <core/lib/FixedArray.hpp>

namespace hyperion::v2 {

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

    Vec2f GetCenter() const { return Vec2f(coord) - 0.5f; }
};

struct TerrainPatchInfo
{
    Extent3D                            extent;
    TerrainPatchCoord                   coord { 0, 0 };
    Vec3f                               scale { 1.0f };
    TerrainPatchState                   state { TerrainPatchState::UNLOADED };
    float                               unload_timer { 0.0f };
    FixedArray<TerrainPatchNeighbor, 8> neighbors { };
};

using TerrainComponentFlags = uint32;

enum TerrainComponentFlagBits : TerrainComponentFlags
{
    TERRAIN_COMPONENT_FLAG_NONE = 0x0,
    TERRAIN_COMPONENT_FLAG_INIT = 0x1
};

struct TerrainPatchComponent
{
    TerrainPatchInfo    patch_info;

    Vec2f GetCenter() const
        { return Vec2f(patch_info.coord) - 0.5f; }
};

struct TerrainComponent
{
    uint32                  seed = 0;
    Extent3D                patch_size = { 32, 32, 32 };
    Vec3f                   camera_position = Vec3f::zero;
    Vec3f                   scale = Vec3f::one;
    float                   max_distance = 2.0f;

    TerrainComponentFlags   flags = TERRAIN_COMPONENT_FLAG_NONE;
};

} // namespace hyperion::v2

#endif
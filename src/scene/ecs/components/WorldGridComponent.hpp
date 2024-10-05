/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_WORLD_GRID_COMPONENT_HPP
#define HYPERION_ECS_WORLD_GRID_COMPONENT_HPP

#include <math/Vector2.hpp>

#include <scene/world_grid/WorldGridSubsystem.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component)
struct WorldGridPatchComponent
{
    HYP_FIELD(Serialize, Property="PatchInfo")
    WorldGridPatchInfo  patch_info;

    HYP_FORCE_INLINE Vec2f GetCenter() const
        { return Vec2f(patch_info.coord) - 0.5f; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(patch_info);

        return hash_code;
    }
};

} // namespace hyperion

#endif
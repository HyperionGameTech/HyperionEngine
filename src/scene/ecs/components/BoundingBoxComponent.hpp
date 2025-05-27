/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP
#define HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP

#include <core/math/BoundingBox.hpp>
#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Size = 64, Editor = false)

struct BoundingBoxComponent
{
    HYP_FIELD(Property = "LocalAABB", Serialize = true)
    BoundingBox local_aabb;

    HYP_FIELD(Property = "WorldAABB", Serialize = true)
    BoundingBox world_aabb;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(local_aabb);
        hash_code.Add(world_aabb);

        return hash_code;
    }
};

} // namespace hyperion

#endif
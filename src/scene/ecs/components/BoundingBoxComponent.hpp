/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP
#define HYPERION_ECS_BOUNDING_BOX_COMPONENT_HPP

#include <math/BoundingBox.hpp>
#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component)
struct BoundingBoxComponent
{
    HYP_FIELD(SerializeAs=LocalAABB)
    BoundingBox local_aabb;

    HYP_FIELD(SerializeAs=WorldAABB)
    BoundingBox world_aabb;

    HYP_FIELD()
    HashCode    transform_hash_code;

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
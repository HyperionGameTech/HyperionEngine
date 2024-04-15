/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_BOUNDING_BOX_COMPONENT_HPP
#define HYPERION_V2_ECS_BOUNDING_BOX_COMPONENT_HPP

#include <math/BoundingBox.hpp>
#include <HashCode.hpp>

namespace hyperion::v2 {

struct BoundingBoxComponent
{
    BoundingBox local_aabb;
    BoundingBox world_aabb;

    HashCode    transform_hash_code;
};

} // namespace hyperion::v2

#endif
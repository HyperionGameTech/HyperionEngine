#ifndef HYPERION_V2_ECS_BOUNDING_BOX_COMPONENT_HPP
#define HYPERION_V2_ECS_BOUNDING_BOX_COMPONENT_HPP

#include <math/BoundingBox.hpp>

namespace hyperion::v2 {

struct BoundingBoxComponent
{
    BoundingBox aabb;
};

} // namespace hyperion::v2

#endif
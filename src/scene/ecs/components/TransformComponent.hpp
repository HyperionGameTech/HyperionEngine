#ifndef HYPERION_V2_ECS_TRANSFORM_COMPONENT_HPP
#define HYPERION_V2_ECS_TRANSFORM_COMPONENT_HPP

#include <math/Transform.hpp>

namespace hyperion::v2 {

struct TransformComponent
{
    Transform   transform;
    Matrix4     previous_transform_matrix;
};

} // namespace hyperion::v2

#endif
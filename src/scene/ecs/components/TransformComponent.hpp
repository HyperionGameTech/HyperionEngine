/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_TRANSFORM_COMPONENT_HPP
#define HYPERION_V2_ECS_TRANSFORM_COMPONENT_HPP

#include <math/Transform.hpp>

namespace hyperion::v2 {

struct TransformComponent
{
    Transform   transform;
};

static_assert(sizeof(TransformComponent) == 112, "TransformComponent must be 112 bytes to match C# struct size");

} // namespace hyperion::v2

#endif
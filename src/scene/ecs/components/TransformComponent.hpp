/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_TRANSFORM_COMPONENT_HPP
#define HYPERION_ECS_TRANSFORM_COMPONENT_HPP

#include <math/Transform.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT()
struct TransformComponent
{
    HYP_FIELD(SerializeAs=Transform, EditorProperty="Transform")
    Transform   transform;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(transform);

        return hash_code;
    }
};

static_assert(sizeof(TransformComponent) == 112, "TransformComponent must be 112 bytes to match C# struct size");

} // namespace hyperion

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Transform.hpp>

#include <core/reflection/ObjectMacros.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Label = "Transform Component", Description = "Holds the translation, rotation, and scale of a node in a scene.", Editor = false, Serialize = false)
struct TransformComponent
{
    HYP_STRUCT_BODY(TransformComponent);

    HYP_FIELD(Property = "Translation")
    Vec3f translation;

    HYP_FIELD(Property = "Rotation")
    Quaternion rotation;

    HYP_FIELD(Property = "Scale")
    Vec3f scale;

    HYP_FORCE_INLINE Mat4f GetMatrix() const
    {
        return Transform(translation, scale, rotation).GetMatrix();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(translation);
        hashCode.Add(rotation);
        hashCode.Add(scale);

        return hashCode;
    }
};

} // namespace hyperion

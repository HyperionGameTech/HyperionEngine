/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Transform.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

HYP_STRUCT(Component, Editor = false, Serialize = false,
    Label = "Transform Component",
    Description = "Holds the translation, rotation, and scale of a node in a scene.")
struct TransformComponent
{
    HYP_STRUCT_BODY(TransformComponent);

    HYP_FIELD(Property = "Translation")
    Vec3f translation;

    HYP_FIELD(Property = "Rotation")
    Quat4f rotation;

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

} // namespace Hyperion

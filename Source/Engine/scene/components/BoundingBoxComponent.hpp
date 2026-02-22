/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/math/BoundingBox.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {

HYP_STRUCT(Component, Editor = false, Serialize = false)
struct BoundingBoxComponent
{
    HYP_STRUCT_BODY(BoundingBoxComponent);

    HYP_FIELD(Property = "WorldAABB")
    BoundingBox worldAabb;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(worldAabb);

        return hashCode;
    }
};

} // namespace Hyperion

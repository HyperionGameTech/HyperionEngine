/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/BoundingBox.hpp>
#include <core/HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Editor = false)
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

} // namespace hyperion

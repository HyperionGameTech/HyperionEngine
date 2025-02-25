/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_BVH_COMPONENT_HPP
#define HYPERION_ECS_BVH_COMPONENT_HPP

#include <scene/BVH.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Editor=false)
struct BVHComponent
{
    HYP_FIELD()
    BVHNode     bvh;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode();
    }
};

} // namespace hyperion

#endif
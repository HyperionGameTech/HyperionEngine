/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_RIGID_BODY_COMPONENT_HPP
#define HYPERION_ECS_RIGID_BODY_COMPONENT_HPP

#include <core/Handle.hpp>
#include <physics/RigidBody.hpp>
#include <physics/PhysicsMaterial.hpp>

#include <HashCode.hpp>

namespace hyperion {

using RigidBodyComponentFlags = uint32;

enum RigidBodyComponentFlagBits : RigidBodyComponentFlags
{
    RIGID_BODY_COMPONENT_FLAG_NONE   = 0x0,
    RIGID_BODY_COMPONENT_FLAG_INIT   = 0x1,
    RIGID_BODY_COMPONENT_FLAG_DIRTY  = 0x2
};

struct RigidBodyComponent
{
    Handle<physics::RigidBody>  rigid_body;
    physics::PhysicsMaterial    physics_material;

    RigidBodyComponentFlags     flags = RIGID_BODY_COMPONENT_FLAG_NONE;
    HashCode                    transform_hash_code;

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(rigid_body);
        hash_code.Add(physics_material);

        return hash_code;
    }
};

} // namespace hyperion

#endif
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

HYP_STRUCT()
struct RigidBodyComponent
{
    HYP_FIELD(SerializeAs=RigidBody)
    Handle<physics::RigidBody>  rigid_body;

    HYP_FIELD(SerializeAs=PhysicsMaterial)
    physics::PhysicsMaterial    physics_material;

    HYP_FIELD()
    RigidBodyComponentFlags     flags = RIGID_BODY_COMPONENT_FLAG_NONE;

    HYP_FIELD()
    HashCode                    transform_hash_code;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(rigid_body);
        hash_code.Add(physics_material);

        return hash_code;
    }
};

} // namespace hyperion

#endif
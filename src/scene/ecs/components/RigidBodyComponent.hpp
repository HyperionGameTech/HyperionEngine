#ifndef HYPERION_V2_ECS_RIGID_BODY_COMPONENT_HPP
#define HYPERION_V2_ECS_RIGID_BODY_COMPONENT_HPP

#include <core/Handle.hpp>
#include <physics/RigidBody.hpp>
#include <physics/PhysicsMaterial.hpp>

#include <HashCode.hpp>

namespace hyperion::v2 {

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
};

} // namespace hyperion::v2

#endif
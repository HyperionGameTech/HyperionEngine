/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>
#include <physics/RigidBody.hpp>
#include <physics/PhysicsMaterial.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

using RigidBodyComponentFlags = uint32;

enum RigidBodyComponentFlagBits : RigidBodyComponentFlags
{
    RIGID_BODY_COMPONENT_FLAG_NONE = 0x0,
    RIGID_BODY_COMPONENT_FLAG_INIT = 0x1,
    RIGID_BODY_COMPONENT_FLAG_DIRTY = 0x2
};

HYP_STRUCT(Component, Label = "Rigid Body Component", Description = "Controls the properties of an object with rigid body physics.", Editor = true)
struct RigidBodyComponent
{
    HYP_FIELD(Serialize, Property = "RigidBody")
    Handle<RigidBody> rigidBody;

    HYP_FIELD(Serialize, Property = "PhysicsMaterial")
    PhysicsMaterial physicsMaterial;

    HYP_FIELD()
    RigidBodyComponentFlags flags = RIGID_BODY_COMPONENT_FLAG_NONE;

    HYP_FIELD()
    HashCode transformHashCode;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(rigidBody);
        hashCode.Add(physicsMaterial);

        return hashCode;
    }
};

} // namespace hyperion

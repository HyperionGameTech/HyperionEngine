/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/HashCode.hpp>

#include <physics/RigidBody.hpp>
#include <physics/PhysicsMaterial.hpp>

namespace Hyperion {

HYP_ENUM()
enum class RigidBodyComponentFlags : uint32
{
    NONE = 0x0,
    INIT = 0x1,
    DIRTY = 0x2
};

HYP_MAKE_ENUM_FLAGS(RigidBodyComponentFlags)

HYP_STRUCT(Component, Label = "Rigid Body Component", Description = "Controls the properties of an object with rigid body physics.", Editor = true)
struct RigidBodyComponent
{
    HYP_STRUCT_BODY(RigidBodyComponent);

    HYP_FIELD(Property = "RigidBody")
    Handle<RigidBody> rigidBody;

    HYP_FIELD(Property = "PhysicsMaterial")
    PhysicsMaterial physicsMaterial;

    HYP_FIELD(Transient)
    EnumFlags<RigidBodyComponentFlags> flags = RigidBodyComponentFlags::NONE;

    HYP_FIELD(Transient)
    HashCode transformHashCode;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(rigidBody);
        hashCode.Add(physicsMaterial);

        return hashCode;
    }
};

} // namespace Hyperion

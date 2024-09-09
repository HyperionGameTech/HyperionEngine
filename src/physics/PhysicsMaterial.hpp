/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PHYSICS_MATERIAL_HPP
#define HYPERION_PHYSICS_MATERIAL_HPP

#include <core/Defines.hpp>

#include <HashCode.hpp>

namespace hyperion {
namespace physics {

HYP_STRUCT()
struct PhysicsMaterial
{
    HYP_FIELD(SerializeAs=Mass)
    float mass = 0.0f;

    HYP_FORCE_INLINE float GetMass() const
        { return mass; }

    HYP_FORCE_INLINE void SetMass(float value)
        { mass = value; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(mass);

        return hash_code;
    }
};

} // namespace physics

using physics::PhysicsMaterial;

} // namespace hyperion

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PHYSICS_MATERIAL_HPP
#define HYPERION_PHYSICS_MATERIAL_HPP

#include <HashCode.hpp>

namespace hyperion {

class Engine;

} // namespace hyperion

namespace hyperion::physics {

struct PhysicsMaterial
{
    float mass = 0.0f;

    float GetMass() const
        { return mass; }

    void SetMass(float value)
        { mass = value; }

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(mass);

        return hash_code;
    }
};

} // namespace hyperion::physics

#endif
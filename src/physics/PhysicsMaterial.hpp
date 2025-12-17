/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

namespace hyperion {

HYP_STRUCT()
struct PhysicsMaterial
{
    HYP_STRUCT_BODY(PhysicsMaterial);

    HYP_FIELD(Serialize, Property = "Mass")
    float mass = 0.0f;

    HYP_FORCE_INLINE float GetMass() const
    {
        return mass;
    }

    HYP_FORCE_INLINE void SetMass(float value)
    {
        mass = value;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;

        hashCode.Add(mass);

        return hashCode;
    }
};

} // namespace hyperion

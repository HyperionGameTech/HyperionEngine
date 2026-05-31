/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_STRUCT()
struct PhysicsMaterial
{
    HYP_STRUCT_BODY(PhysicsMaterial);

    HYP_FIELD(Property = "Mass", Serialize)
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

} // namespace Hyperion

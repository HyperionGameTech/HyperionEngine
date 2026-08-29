/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_STRUCT()
struct PhysicsMaterial
{
    HYP_STRUCT_BODY(PhysicsMaterial);

    HYP_FIELD(Property = "Mass", Serialize)
    float mass = 0.0f;

    HYP_FIELD(Property = "Friction", Serialize)
    float friction = 0.5f;

    HYP_FIELD(Property = "Restitution", Serialize)
    float restitution = 0.0f;


    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;
        hashCode.Add(mass);
        hashCode.Add(friction);
        hashCode.Add(restitution);

        return hashCode;
    }
};

} // namespace Hyperion

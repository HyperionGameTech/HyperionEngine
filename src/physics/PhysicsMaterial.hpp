#ifndef HYPERION_V2_PHYSICS_MATERIAL_HPP
#define HYPERION_V2_PHYSICS_MATERIAL_HPP

#include <Types.hpp>

namespace hyperion::v2 {

class Engine;

} // namespace hyperion::v2

namespace hyperion::v2::physics {

struct PhysicsMaterial
{
    float mass;

    float GetMass() const
        { return mass; }

    void SetMass(float value)
        { mass = value; }
};

} // namespace hyperion::v2::physics

#endif
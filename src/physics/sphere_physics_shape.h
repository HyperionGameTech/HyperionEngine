#ifndef APEX_PHYSICS_SPHERE_PHYSICS_SHAPE_H
#define APEX_PHYSICS_SPHERE_PHYSICS_SHAPE_H

#include "physics_shape.h"

namespace hyperion {
namespace physics {
class SpherePhysicsShape : public PhysicsShape {
public:
    SpherePhysicsShape(double radius);
    SpherePhysicsShape(const SpherePhysicsShape &other);
    virtual ~SpherePhysicsShape() override;

    SpherePhysicsShape &operator=(const SpherePhysicsShape &other) = delete;

    inline double GetRadius() const { return m_radius; }
    inline void SetRadius(double radius) { m_radius = radius; }

    virtual BoundingBox GetBoundingBox() override;

private:
    double m_radius;
};
} // namespace physics
} // namespace hyperion

#endif

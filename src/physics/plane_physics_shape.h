#ifndef APEX_PHYSICS_PLANE_PHYSICS_SHAPE_H
#define APEX_PHYSICS_PLANE_PHYSICS_SHAPE_H

#include "physics_shape.h"

namespace hyperion {
namespace physics {

class PlanePhysicsShape : public PhysicsShape {
public:
    PlanePhysicsShape(const Vector3 &direction, double offset);
    PlanePhysicsShape(const PlanePhysicsShape &other);
    virtual ~PlanePhysicsShape() override;

    PlanePhysicsShape &operator=(const PlanePhysicsShape &other) = delete;

    inline const Vector3 &GetDirection() const { return m_direction; }
    inline Vector3 &GetDirection() { return m_direction; }
    inline void SetDirection(const Vector3 &direction) { m_direction = direction; }
    inline double GetOffset() const { return m_offset; }
    inline void SetOffset(double offset) { m_offset = offset; }

    virtual BoundingBox GetBoundingBox() override;

private:
    Vector3 m_direction;
    double m_offset;
};

} // namespace physics
} // namespace hyperion

#endif

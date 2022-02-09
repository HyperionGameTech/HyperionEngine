#ifndef APEX_PHYSICS_BOX_PHYSICS_SHAPE_H
#define APEX_PHYSICS_BOX_PHYSICS_SHAPE_H

#include "physics_shape.h"
#include "../math/vector3.h"

namespace hyperion {
class BoundingBox;
namespace physics {

class BoxPhysicsShape : public PhysicsShape {
public:
    BoxPhysicsShape(const Vector3 &dimensions);
    BoxPhysicsShape(const BoundingBox &aabb);
    BoxPhysicsShape(const BoxPhysicsShape &other);
    virtual ~BoxPhysicsShape() override;

    BoxPhysicsShape &operator=(const BoxPhysicsShape &other) = delete;

    inline const Vector3 &GetDimensions() const { return m_dimensions; }
    inline Vector3 &GetDimensions() { return m_dimensions; }
    inline void SetDimensions(const Vector3 &dimensions) { m_dimensions = dimensions; }

    virtual BoundingBox GetBoundingBox() override;

private:
    Vector3 m_dimensions;
};

} // namespace physics
} // namespace hyperion

#endif

#ifndef APEX_PHYSICS_PHYSICS_SHAPE_H
#define APEX_PHYSICS_PHYSICS_SHAPE_H

#include "../math/vector3.h"
#include "../math/matrix4.h"
#include "../math/ray.h"
#include "../math/bounding_box.h"

class btCollisionShape;

namespace hyperion {
namespace physics {

// forward declarations
class BoxPhysicsShape;
class SpherePhysicsShape;
class PlanePhysicsShape;

enum PhysicsShapeType {
    PhysicsShape_box,
    PhysicsShape_sphere,
    PhysicsShape_plane,
};

// abstract physics shape
class PhysicsShape {
    friend class RigidBody;
public:
    PhysicsShape(PhysicsShapeType type);
    PhysicsShape(const PhysicsShape &other) = delete;
    PhysicsShape &operator=(const PhysicsShape &other) = delete;
    virtual ~PhysicsShape();

    inline const Matrix4 &GetTransform() const { return m_transform; }
    inline Matrix4 &GetTransform() { return m_transform; }
    inline void SetTransform(const Matrix4 &transform) { m_transform = transform; }
    inline PhysicsShapeType GetType() const { return m_type; }

    Vector3 GetAxis(unsigned int index) const;

    virtual BoundingBox GetBoundingBox() = 0;

protected:
    Matrix4 m_transform;

    btCollisionShape *m_collision_shape;

private:
    const PhysicsShapeType m_type;
};

} // namespace physics
} // namespace hyperion

#endif

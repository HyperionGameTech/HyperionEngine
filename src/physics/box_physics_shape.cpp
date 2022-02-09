#include "box_physics_shape.h"
#include "sphere_physics_shape.h"
#include "plane_physics_shape.h"
#include "../math/bounding_box.h"
#include "../util.h"

#include "../bullet_math_util.h"
#include "btBulletDynamicsCommon.h"

#include <cassert>

namespace hyperion {
namespace physics {

BoxPhysicsShape::BoxPhysicsShape(const Vector3 &dimensions)
    : PhysicsShape(PhysicsShape_box),
      m_dimensions(dimensions)
{
    m_collision_shape = new btBoxShape(ToBulletVector(m_dimensions));
}

BoxPhysicsShape::BoxPhysicsShape(const BoundingBox &aabb)
    : PhysicsShape(PhysicsShape_box),
      m_dimensions(aabb.GetDimensions())
{
    m_collision_shape = new btBoxShape(ToBulletVector(m_dimensions));
}

BoxPhysicsShape::BoxPhysicsShape(const BoxPhysicsShape &other)
    : PhysicsShape(PhysicsShape_box),
      m_dimensions(other.m_dimensions)
{
    m_collision_shape = new btBoxShape(ToBulletVector(m_dimensions));
}

BoxPhysicsShape::~BoxPhysicsShape()
{
    delete m_collision_shape;
}

BoundingBox BoxPhysicsShape::GetBoundingBox()
{
    BoundingBox bounds(m_dimensions * -0.5, m_dimensions * 0.5);

    BoundingBox bounding_box;

    for (const auto &corner : bounds.GetCorners()) {
        bounding_box.Extend(corner * m_transform);
    }

    return bounding_box;
}

} // namespace physics
} // namespace hyperion

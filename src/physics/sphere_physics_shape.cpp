#include "sphere_physics_shape.h"
#include "box_physics_shape.h"
#include "plane_physics_shape.h"

#include "../bullet_math_util.h"
#include "btBulletDynamicsCommon.h"
namespace hyperion {
namespace physics {
SpherePhysicsShape::SpherePhysicsShape(double radius)
    : PhysicsShape(PhysicsShape_sphere),
      m_radius(radius)
{
    m_collision_shape = new btSphereShape(m_radius);
}

SpherePhysicsShape::SpherePhysicsShape(const SpherePhysicsShape &other)
    : PhysicsShape(PhysicsShape_sphere),
      m_radius(other.m_radius)
{
    m_collision_shape = new btSphereShape(m_radius);
}

SpherePhysicsShape::~SpherePhysicsShape()
{
    delete m_collision_shape;
}

BoundingBox SpherePhysicsShape::GetBoundingBox()
{
    BoundingBox bounding_box;

    // @TODO

    return bounding_box;
}

} // namespace physics
} // namespace hyperion

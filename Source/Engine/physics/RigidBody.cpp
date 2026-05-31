/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <physics/RigidBody.hpp>
#include <physics/PhysicsWorld.hpp>
#include <physics/PhysicsShape.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#include <RigidBody.generated.inl>

namespace Hyperion {

PhysicsMaterial& GetDefaultPhysicsMaterial()
{
    static PhysicsMaterial s_defaultPhysicsMaterial;
    return s_defaultPhysicsMaterial;
}

PhysicsShape& GetDefaultPhysicsShape()
{
    static BoxPhysicsShape s_defaultPhysicsShape(Name::Invalid(), BoundingBox());
    return s_defaultPhysicsShape;
}

RigidBody::RigidBody()
    : shape(&GetDefaultPhysicsShape()),
      physicsMaterial(&GetDefaultPhysicsMaterial()),
      m_isKinematic(true)
{
}

RigidBody::~RigidBody()
{
}

} // namespace Hyperion

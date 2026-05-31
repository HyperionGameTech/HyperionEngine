/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Physics/RigidBody.hpp>
#include <Physics/PhysicsWorld.hpp>
#include <Physics/PhysicsShape.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>

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

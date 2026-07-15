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

const Handle<PhysicsShape>& GetDefaultPhysicsShape()
{
    static Handle<BoxPhysicsShape> s_defaultPhysicsShape = MakeHandle<BoxPhysicsShape>(Name::Invalid(), BoundingBox());
    return s_defaultPhysicsShape;
}

RigidBody::RigidBody()
    : shape(GetDefaultPhysicsShape().Get()),
      physicsMaterial(&GetDefaultPhysicsMaterial()),
      m_isKinematic(true)
{
}

RigidBody::~RigidBody()
{
}

} // namespace Hyperion

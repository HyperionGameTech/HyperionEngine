/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <physics/RigidBody.hpp>
#include <physics/PhysicsWorld.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

#include <RigidBody.generated.inl>

namespace Hyperion {

static inline PhysicsWorld* GetPhysicsWorld()
{
    World* currentWorld = g_engineDriver->GetCurrentWorld();

    if (!currentWorld)
    {
        return nullptr;
    }

    return static_cast<PhysicsWorld*>(currentWorld->GetPhysicsWorld().Get());
}

RigidBody::RigidBody()
    : RigidBody(nullptr, {})
{
}

RigidBody::RigidBody(const PhysicsMaterial& physicsMaterial)
    : RigidBody(nullptr, physicsMaterial)
{
}

RigidBody::RigidBody(const Handle<PhysicsShape>& shape, const PhysicsMaterial& physicsMaterial)
    : ObjectBase(),
      m_shape(shape),
      m_physicsMaterial(physicsMaterial),
      m_isKinematic(true)
{
}

RigidBody::~RigidBody()
{
}

void RigidBody::Init()
{
    SetReady(true);
}

void RigidBody::SetShape(const Handle<PhysicsShape>& shape)
{
    m_shape = shape;

    GetPhysicsWorld()->GetAdapter().OnChangePhysicsShape(this);
}

void RigidBody::SetPhysicsMaterial(const PhysicsMaterial& physicsMaterial)
{
    m_physicsMaterial = physicsMaterial;

    GetPhysicsWorld()->GetAdapter().OnChangePhysicsMaterial(this);
}

void RigidBody::ApplyForce(const Vector3& force)
{
    GetPhysicsWorld()->GetAdapter().ApplyForceToBody(this, force);
}

} // namespace Hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <physics/RigidBody.hpp>

#include <core/object/HypClassUtils.hpp>

#include <scene/World.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion::physics {

RigidBody::RigidBody()
    : RigidBody(nullptr, {})
{
}

RigidBody::RigidBody(const PhysicsMaterial& physicsMaterial)
    : RigidBody(nullptr, physicsMaterial)
{
}

RigidBody::RigidBody(const Handle<PhysicsShape>& shape, const PhysicsMaterial& physicsMaterial)
    : HypObjectBase(),
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

    if (IsInitCalled())
    {
        g_engineDriver->GetCurrentWorld()->GetPhysicsWorld().GetAdapter().OnChangePhysicsShape(this);
    }
}

void RigidBody::SetPhysicsMaterial(const PhysicsMaterial& physicsMaterial)
{
    m_physicsMaterial = physicsMaterial;

    if (IsInitCalled())
    {
        g_engineDriver->GetCurrentWorld()->GetPhysicsWorld().GetAdapter().OnChangePhysicsMaterial(this);
    }
}

void RigidBody::ApplyForce(const Vector3& force)
{
    g_engineDriver->GetCurrentWorld()->GetPhysicsWorld().GetAdapter().ApplyForceToBody(this, force);
}

} // namespace hyperion::physics

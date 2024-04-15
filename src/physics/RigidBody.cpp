/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <physics/RigidBody.hpp>

#include <Engine.hpp>

namespace hyperion::v2::physics {

RigidBody::~RigidBody()
{
}

void RigidBody::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    // do nothing
}

void RigidBody::SetShape(RC<PhysicsShape> &&shape)
{
    m_shape = std::move(shape);

    if (IsInitCalled()) {
        g_engine->GetWorld()->GetPhysicsWorld().GetAdapter().OnChangePhysicsShape(this);
    }
}

void RigidBody::SetPhysicsMaterial(const PhysicsMaterial &physics_material)
{
    m_physics_material = physics_material;

    if (IsInitCalled()) {
        g_engine->GetWorld()->GetPhysicsWorld().GetAdapter().OnChangePhysicsMaterial(this);
    }
}

void RigidBody::ApplyForce(const Vector3 &force)
{
    g_engine->GetWorld()->GetPhysicsWorld().GetAdapter().ApplyForceToBody(this, force);
}

} // namespace hyperion::v2::physics
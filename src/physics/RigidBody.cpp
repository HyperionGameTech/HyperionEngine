/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <physics/RigidBody.hpp>

#include <core/object/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion::physics {

HYP_DEFINE_CLASS(RigidBody);

RigidBody::RigidBody()
    : RigidBody(nullptr, { })
{
}

RigidBody::RigidBody(const PhysicsMaterial &physics_material)
    : RigidBody(nullptr, physics_material)
{
}

RigidBody::RigidBody(RC<PhysicsShape> &&shape, const PhysicsMaterial &physics_material)
    : BasicObject(),
      m_shape(std::move(shape)),
      m_physics_material(physics_material),
      m_is_kinematic(true)
{
}

RigidBody::~RigidBody()
{
}

void RigidBody::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();
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

} // namespace hyperion::physics
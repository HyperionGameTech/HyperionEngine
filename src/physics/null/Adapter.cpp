/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <physics/null/Adapter.hpp>
#include <physics/PhysicsWorld.hpp>
#include <physics/RigidBody.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Quaternion.hpp>

namespace hyperion::physics {

NullPhysicsAdapter::NullPhysicsAdapter() = default;

NullPhysicsAdapter::~NullPhysicsAdapter() = default;

void NullPhysicsAdapter::Init(PhysicsWorldBase *world)
{
}

void NullPhysicsAdapter::Teardown(PhysicsWorldBase *world)
{
}

void NullPhysicsAdapter::Tick(PhysicsWorldBase *world, GameCounter::TickUnitHighPrec delta)
{
}

void NullPhysicsAdapter::OnRigidBodyAdded(const Handle<RigidBody> &rigid_body)
{
}

void NullPhysicsAdapter::OnRigidBodyRemoved(const Handle<RigidBody> &rigid_body)
{
}

void NullPhysicsAdapter::OnChangePhysicsShape(RigidBody *rigid_body)
{
}

void NullPhysicsAdapter::OnChangePhysicsMaterial(RigidBody *rigid_body)
{
}

void NullPhysicsAdapter::ApplyForceToBody(const RigidBody *rigid_body, const Vector3 &force)
{
}


} // namespace hyperion::physics
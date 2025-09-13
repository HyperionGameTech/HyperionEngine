/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <physics/null/Adapter.hpp>
#include <physics/PhysicsWorld.hpp>
#include <physics/RigidBody.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Quaternion.hpp>

namespace hyperion {

NullPhysicsAdapter::NullPhysicsAdapter() = default;

NullPhysicsAdapter::~NullPhysicsAdapter() = default;

void NullPhysicsAdapter::Init(PhysicsWorldBase* world)
{
}

void NullPhysicsAdapter::Teardown(PhysicsWorldBase* world)
{
}

void NullPhysicsAdapter::Tick(PhysicsWorldBase* world, double delta)
{
}

void NullPhysicsAdapter::OnRigidBodyAdded(const Handle<RigidBody>& rigidBody)
{
}

void NullPhysicsAdapter::OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody)
{
}

void NullPhysicsAdapter::OnChangePhysicsShape(RigidBody* rigidBody)
{
}

void NullPhysicsAdapter::OnChangePhysicsMaterial(RigidBody* rigidBody)
{
}

void NullPhysicsAdapter::ApplyForceToBody(const RigidBody* rigidBody, const Vector3& force)
{
}

} // namespace hyperion
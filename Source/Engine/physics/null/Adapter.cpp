/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <physics/null/Adapter.hpp>
#include <physics/PhysicsWorld.hpp>
#include <physics/RigidBody.hpp>

#include <Core/memory/UniquePtr.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Quaternion.hpp>

namespace Hyperion {

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

} // namespace Hyperion

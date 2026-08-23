/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Physics/Null/NullPhysicsAdapter.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/RigidBody.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Quat4f.hpp>

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

void NullPhysicsAdapter::SetRigidBodyTransform(const Handle<RigidBody>& rigidBody, const Transform& transform)
{
}

void NullPhysicsAdapter::OnChangePhysicsShape(RigidBody* rigidBody)
{
}

void NullPhysicsAdapter::OnChangePhysicsMaterial(RigidBody* rigidBody)
{
}

void NullPhysicsAdapter::ApplyForceToBody(const RigidBody* rigidBody, const Vec3f& force)
{
}

void NullPhysicsAdapter::OnCharacterControllerAdded(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle)
{
}

void NullPhysicsAdapter::OnCharacterControllerRemoved(SharedPtr<void>& physicsHandle)
{
}

void NullPhysicsAdapter::SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity)
{
}

void NullPhysicsAdapter::ApplyCharacterJump(const SharedPtr<void>& physicsHandle)
{
}

void NullPhysicsAdapter::StepCharacterController(const SharedPtr<void>& physicsHandle, float deltaTime)
{
}

void NullPhysicsAdapter::SetCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation)
{
}

void NullPhysicsAdapter::GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround)
{
}

} // namespace Hyperion

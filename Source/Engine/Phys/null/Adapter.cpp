/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <physics/null/Adapter.hpp>
#include <physics/PhysicsWorld.hpp>
#include <physics/RigidBody.hpp>

#include <Core/memory/UniquePtr.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Quat4f.hpp>

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

void NullPhysicsAdapter::ApplyForceToBody(const RigidBody* rigidBody, const Vec3f& force)
{
}

void NullPhysicsAdapter::OnCharacterControllerAdded(const CharacterControllerConfig& config, RC<void>& outPhysicsHandle)
{
}

void NullPhysicsAdapter::OnCharacterControllerRemoved(RC<void>& physicsHandle)
{
}

void NullPhysicsAdapter::SetCharacterWalkDirection(const RC<void>& physicsHandle, const Vec3f& velocity)
{
}

void NullPhysicsAdapter::ApplyCharacterJump(const RC<void>& physicsHandle)
{
}

void NullPhysicsAdapter::GetCharacterState(const RC<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround)
{
}

} // namespace Hyperion

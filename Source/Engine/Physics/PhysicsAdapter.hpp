/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Transform.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Containers/Array.hpp>

#include <Physics/PhysicsMemory.hpp>

namespace Hyperion {

class PhysicsWorldBase;
class RigidBody;
class PhysicsShape;

struct CharacterControllerConfig
{
    Handle<PhysicsShape> shape;
    Vec3f startTranslation;
    float stepHeight = 0.35f;
    float maxSlopeAngle = 45.0f;
    float jumpSpeed = 4.9f;
    float fallSpeed = 55.0f;

    float groundAcceleration = 12.0f;
    float airAcceleration = 3.0f;
    float friction = 8.0f;
    float stopSpeed = 2.5f;

    float jumpCutGravityMultiplier = 2.2f;
    float apexGravityMultiplier = 0.85f;
    float fallGravityMultiplier = 1.8f;

    // Self explanatory name (picture it)
    float coyoteTime = 0.15f;

    // Grace period during which a jump requested before landing is still honored.
    float jumpBufferTime = 0.15f;

    // Max. follow speed for shadow body
    float shadowMaxSpeed = 60.0f;
    // Instead of following, shadow body will teleport at this distance.
    float shadowTeleportDistance = 0.5f;

    float pushMassLimit = 350.0f;
    float maxPushSpeed = 1.5f;
    float pushSpeedScale = 1.0f;

    // Dynamic bodies lighter than this aren't trusted as moving-platform footing
    // (Jolt only) - see JoltPhysicsAdapter::StepCharacterController.
    float minGroundSupportMass = 20.0f;
};

template <class DerivedAdapter>
class PhysicsAdapter
{
public:
    DerivedAdapter* GetDerivedAdapter()
    {
        return static_cast<DerivedAdapter*>(this);
    }

    const DerivedAdapter* GetDerivedAdapter() const
    {
        return static_cast<const DerivedAdapter*>(this);
    }

    void Init(PhysicsWorldBase* world)
    {
        GetDerivedAdapter()->DerivedAdapter::Init(world);
    }

    void Teardown(PhysicsWorldBase* world)
    {
        GetDerivedAdapter()->DerivedAdapter::Teardown(world);
    }

    void Tick(PhysicsWorldBase* world, double delta)
    {
        GetDerivedAdapter()->DerivedAdapter::Tick(world, delta);
    }

    void OnRigidBodyAdded(const Handle<RigidBody>& rigidBody)
    {
        GetDerivedAdapter()->DerivedAdapter::OnRigidBodyAdded(rigidBody);
    }

    void OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody)
    {
        GetDerivedAdapter()->DerivedAdapter::OnRigidBodyRemoved(rigidBody);
    }

    void SetRigidBodyTransform(const Handle<RigidBody>& rigidBody, const Transform& transform)
    {
        GetDerivedAdapter()->DerivedAdapter::SetRigidBodyTransform(rigidBody, transform);
    }

    void MoveRigidBodyKinematic(const Handle<RigidBody>& rigidBody, const Transform& transform, float deltaTime)
    {
        GetDerivedAdapter()->DerivedAdapter::MoveRigidBodyKinematic(rigidBody, transform, deltaTime);
    }

    void SetRigidBodyKinematic(const Handle<RigidBody>& rigidBody, bool isKinematic)
    {
        GetDerivedAdapter()->DerivedAdapter::SetRigidBodyKinematic(rigidBody, isKinematic);
    }

    void SetRigidBodyCharacterGhostCollidable(const Handle<RigidBody>& rigidBody, bool collidable)
    {
        GetDerivedAdapter()->DerivedAdapter::SetRigidBodyCharacterGhostCollidable(rigidBody, collidable);
    }

    void OnChangePhysicsShape(RigidBody* rigidBody)
    {
        GetDerivedAdapter()->DerivedAdapter::OnChangePhysicsShape(rigidBody);
    }

    void OnChangePhysicsMaterial(RigidBody* rigidBody)
    {
        GetDerivedAdapter()->DerivedAdapter::OnChangePhysicsMaterial(rigidBody);
    }

    void ApplyForceToBody(const RigidBody* rigidBody, const Vector3& force)
    {
        GetDerivedAdapter()->DerivedAdapter::ApplyForceToBody(rigidBody, force);
    }

    void OnCharacterControllerAdded(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle)
    {
        GetDerivedAdapter()->DerivedAdapter::OnCharacterControllerAdded(config, outPhysicsHandle);
    }

    void OnCharacterControllerRemoved(SharedPtr<void>& physicsHandle)
    {
        GetDerivedAdapter()->DerivedAdapter::OnCharacterControllerRemoved(physicsHandle);
    }

    void SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity)
    {
        GetDerivedAdapter()->DerivedAdapter::SetCharacterWalkDirection(physicsHandle, velocity);
    }

    void ApplyCharacterJump(const SharedPtr<void>& physicsHandle, bool jumpRequested, bool jumpHeld)
    {
        GetDerivedAdapter()->DerivedAdapter::ApplyCharacterJump(physicsHandle, jumpRequested, jumpHeld);
    }

    void StepCharacterController(const SharedPtr<void>& physicsHandle, float deltaTime)
    {
        GetDerivedAdapter()->DerivedAdapter::StepCharacterController(physicsHandle, deltaTime);
    }

    void SetCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation)
    {
        GetDerivedAdapter()->DerivedAdapter::SetCharacterTranslation(physicsHandle, translation);
    }

    void NudgeCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation)
    {
        GetDerivedAdapter()->DerivedAdapter::NudgeCharacterTranslation(physicsHandle, translation);
    }

    void GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround)
    {
        GetDerivedAdapter()->DerivedAdapter::GetCharacterState(physicsHandle, outTranslation, outIsOnGround);
    }

    void GetCharacterTouchedRigidBodies(const SharedPtr<void>& physicsHandle, Array<Handle<RigidBody>, PhysicsAllocator>& out)
    {
        GetDerivedAdapter()->DerivedAdapter::GetCharacterTouchedRigidBodies(physicsHandle, out);
    }
};

} // namespace Hyperion

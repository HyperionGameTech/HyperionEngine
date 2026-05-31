/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Memory/RefCountedPtr.hpp>

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
    float jumpSpeed = 10.0f;
    float fallSpeed = 55.0f;
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

    void OnCharacterControllerAdded(const CharacterControllerConfig& config, RC<void>& outPhysicsHandle)
    {
        GetDerivedAdapter()->DerivedAdapter::OnCharacterControllerAdded(config, outPhysicsHandle);
    }

    void OnCharacterControllerRemoved(RC<void>& physicsHandle)
    {
        GetDerivedAdapter()->DerivedAdapter::OnCharacterControllerRemoved(physicsHandle);
    }

    void SetCharacterWalkDirection(const RC<void>& physicsHandle, const Vec3f& velocity)
    {
        GetDerivedAdapter()->DerivedAdapter::SetCharacterWalkDirection(physicsHandle, velocity);
    }

    void ApplyCharacterJump(const RC<void>& physicsHandle)
    {
        GetDerivedAdapter()->DerivedAdapter::ApplyCharacterJump(physicsHandle);
    }

    void GetCharacterState(const RC<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround)
    {
        GetDerivedAdapter()->DerivedAdapter::GetCharacterState(physicsHandle, outTranslation, outIsOnGround);
    }
};

} // namespace Hyperion

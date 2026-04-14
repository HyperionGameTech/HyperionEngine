/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>
#include <Core/math/Vector3.hpp>

namespace Hyperion {

class PhysicsWorldBase;
class RigidBody;
class CharacterController;

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

    void OnCharacterControllerAdded(const Handle<CharacterController>& characterController)
    {
        GetDerivedAdapter()->DerivedAdapter::OnCharacterControllerAdded(characterController);
    }

    void OnCharacterControllerRemoved(const Handle<CharacterController>& characterController)
    {
        GetDerivedAdapter()->DerivedAdapter::OnCharacterControllerRemoved(characterController);
    }

    void SetCharacterWalkDirection(CharacterController* characterController, const Vec3f& velocity)
    {
        GetDerivedAdapter()->DerivedAdapter::SetCharacterWalkDirection(characterController, velocity);
    }

    void ApplyCharacterJump(CharacterController* characterController)
    {
        GetDerivedAdapter()->DerivedAdapter::ApplyCharacterJump(characterController);
    }
};

} // namespace Hyperion

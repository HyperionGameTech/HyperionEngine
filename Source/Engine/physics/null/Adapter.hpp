/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <physics/Adapter.hpp>

namespace Hyperion {

class HYP_API NullPhysicsAdapter : public PhysicsAdapter<NullPhysicsAdapter>
{
public:
    NullPhysicsAdapter();
    ~NullPhysicsAdapter();

    void Init(PhysicsWorldBase* world);
    void Teardown(PhysicsWorldBase* world);
    void Tick(PhysicsWorldBase* world, double delta);

    void OnRigidBodyAdded(const Handle<RigidBody>& rigidBody);
    void OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody);

    void OnChangePhysicsShape(RigidBody* rigidBody);
    void OnChangePhysicsMaterial(RigidBody* rigidBody);

    void ApplyForceToBody(const RigidBody* rigidBody, const Vec3f& force);

    void OnCharacterControllerAdded(const Handle<CharacterController>& characterController);
    void OnCharacterControllerRemoved(const Handle<CharacterController>& characterController);
    void SetCharacterWalkDirection(CharacterController* characterController, const Vec3f& velocity);
    void ApplyCharacterJump(CharacterController* characterController);
};

} // namespace Hyperion

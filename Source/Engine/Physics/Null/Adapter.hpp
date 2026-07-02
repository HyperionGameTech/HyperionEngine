/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Physics/Adapter.hpp>

namespace Hyperion {

class NullPhysicsAdapter : public PhysicsAdapter<NullPhysicsAdapter>
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

    void OnCharacterControllerAdded(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle);
    void OnCharacterControllerRemoved(SharedPtr<void>& physicsHandle);
    void SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity);
    void ApplyCharacterJump(const SharedPtr<void>& physicsHandle);
    void GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround);
};

} // namespace Hyperion

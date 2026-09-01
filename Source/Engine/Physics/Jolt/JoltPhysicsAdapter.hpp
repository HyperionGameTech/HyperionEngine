/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Physics/PhysicsAdapter.hpp>
#include <Physics/PhysicsMemory.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Set.hpp>

namespace JPH {
class PhysicsSystem;
class JobSystem;
class TempAllocator;
class BroadPhaseLayerInterface;
class ObjectLayerPairFilter;
class ObjectVsBroadPhaseLayerFilter;
class BodyFilter;
class CharacterVsCharacterCollisionSimple;
}

namespace Hyperion {

class JoltPhysicsAdapter : public PhysicsAdapter<JoltPhysicsAdapter>
{
public:
    JoltPhysicsAdapter();
    ~JoltPhysicsAdapter();

    void Init(PhysicsWorldBase* world);
    void Teardown(PhysicsWorldBase* world);
    void Tick(PhysicsWorldBase* world, double delta);

    void OnRigidBodyAdded(const Handle<RigidBody>& rigidBody);
    void OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody);
    void SetRigidBodyTransform(const Handle<RigidBody>& rigidBody, const Transform& transform);
    void MoveRigidBodyKinematic(const Handle<RigidBody>& rigidBody, const Transform& transform, float deltaTime);
    void SetRigidBodyKinematic(const Handle<RigidBody>& rigidBody, bool isKinematic);
    void SetRigidBodyCharacterGhostCollidable(const Handle<RigidBody>& rigidBody, bool collidable);

    void OnChangePhysicsShape(RigidBody* rigidBody);
    void OnChangePhysicsMaterial(RigidBody* rigidBody);

    void ApplyForceToBody(const RigidBody* rigidBody, const Vec3f& force);

    void OnCharacterControllerAdded(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle);
    void OnCharacterControllerRemoved(SharedPtr<void>& physicsHandle);
    void SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity);
    void ApplyCharacterJump(const SharedPtr<void>& physicsHandle);
    void StepCharacterController(const SharedPtr<void>& physicsHandle, float deltaTime);
    void SetCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation);
    void GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround);

private:
    JPH::PhysicsSystem* m_physicsSystem;
    JPH::JobSystem* m_jobSystem;
    JPH::TempAllocator* m_tempAllocator;
    JPH::BroadPhaseLayerInterface* m_broadPhaseLayerInterface;
    JPH::ObjectLayerPairFilter* m_objectLayerPairFilter;
    JPH::ObjectVsBroadPhaseLayerFilter* m_objectVsBroadPhaseLayerFilter;
    JPH::BodyFilter* m_characterBodyFilter;
    JPH::CharacterVsCharacterCollisionSimple* m_characterVsCharacterCollision;
    Array<SharedPtr<void>, PhysicsAllocator> m_characterControllers;
    Set<uint32, PhysicsAllocator> m_ghostNonCollidableBodyIds;
    bool m_needsBroadphaseOptimize;
};

} // namespace Hyperion

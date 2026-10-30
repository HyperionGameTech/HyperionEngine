/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Physics/PhysicsAdapter.hpp>
#include <Physics/PhysicsMemory.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
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
class ContactListener;
}

namespace Hyperion {

// JPH::ObjectLayer values
namespace JoltLayers {

static constexpr uint16 NON_MOVING = 0;
static constexpr uint16 MOVING = 1;
// collides with everything but characters
static constexpr uint16 RAGDOLL = 2;
static constexpr uint16 NUM_LAYERS = 3;

} // namespace JoltLayers

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
    void ApplyCharacterJump(const SharedPtr<void>& physicsHandle, bool jumpRequested, bool jumpHeld);
    void StepCharacterController(const SharedPtr<void>& physicsHandle, float deltaTime);
    void SetCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation);
    void NudgeCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation);
    void GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround);
    void GetCharacterMotionState(const SharedPtr<void>& physicsHandle, CharacterMotionState& outMotionState);
    void SetCharacterMotionState(const SharedPtr<void>& physicsHandle, const CharacterMotionState& motionState);
    void GetCharacterTouchedRigidBodies(const SharedPtr<void>& physicsHandle, Array<Handle<RigidBody>, PhysicsAllocator>& out);

    bool CastRayStatic(const Vec3f& origin, const Vec3f& direction, float maxDistance, PhysicsRayHit& outHit);

    HYP_FORCE_INLINE JPH::PhysicsSystem* GetPhysicsSystem() const
    {
        return m_physicsSystem;
    }

    HYP_FORCE_INLINE JPH::TempAllocator* GetTempAllocator() const
    {
        return m_tempAllocator;
    }

    // collision steps of the current (or last) update, which step listeners see one at a time
    HYP_FORCE_INLINE uint32 GetNumCollisionSteps() const
    {
        return m_numCollisionSteps;
    }

    // called from physics threads during updates; add and remove between them
    ENGINE_API void AddContactListener(JPH::ContactListener* listener);
    ENGINE_API void RemoveContactListener(JPH::ContactListener* listener);

private:
    JPH::PhysicsSystem* m_physicsSystem;
    JPH::JobSystem* m_jobSystem;
    JPH::TempAllocator* m_tempAllocator;
    JPH::BroadPhaseLayerInterface* m_broadPhaseLayerInterface;
    JPH::ObjectLayerPairFilter* m_objectLayerPairFilter;
    JPH::ObjectVsBroadPhaseLayerFilter* m_objectVsBroadPhaseLayerFilter;
    JPH::BodyFilter* m_characterBodyFilter;
    JPH::CharacterVsCharacterCollisionSimple* m_characterVsCharacterCollision;
    JPH::ContactListener* m_contactListeners;
    uint32 m_numCollisionSteps;
    Array<SharedPtr<void>, PhysicsAllocator> m_characterControllers;
    Set<uint32, PhysicsAllocator> m_ghostNonCollidableBodyIds;
    Map<uint32, Handle<RigidBody>, PhysicsAllocator> m_bodyIdToRigidBody;
    bool m_needsBroadphaseOptimize;
};

} // namespace Hyperion

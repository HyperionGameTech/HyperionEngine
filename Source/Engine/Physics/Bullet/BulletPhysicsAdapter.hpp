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

struct btDbvtBroadphase;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDynamicsWorld;
class btRigidBody;
class btDiscreteDynamicsWorld;
struct btOverlapFilterCallback;

namespace Hyperion {

class BulletPhysicsAdapter : public PhysicsAdapter<BulletPhysicsAdapter>
{
public:
    BulletPhysicsAdapter();
    ~BulletPhysicsAdapter();

    void Init(PhysicsWorldBase* world);
    void Teardown(PhysicsWorldBase* world);
    void Tick(PhysicsWorldBase* world, double delta);

    void OnRigidBodyAdded(const Handle<RigidBody>& rigidBody);
    void OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody);
    void SetRigidBodyTransform(const Handle<RigidBody>& rigidBody, const Transform& transform);
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
    void UpdateCharacterShadowBodies(double simDelta);
    void AdvanceCharacterShadowBodies(float substepDelta);

    btDbvtBroadphase* m_broadphase;
    btDefaultCollisionConfiguration* m_collisionConfiguration;
    btCollisionDispatcher* m_dispatcher;
    btSequentialImpulseConstraintSolver* m_solver;
    btDiscreteDynamicsWorld* m_dynamicsWorld;
    btOverlapFilterCallback* m_characterOverlapFilter;
    Array<SharedPtr<void>, PhysicsAllocator> m_characterControllers;
    Set<btRigidBody*, PhysicsAllocator> m_ghostNonCollidableBodies;
};

} // namespace Hyperion

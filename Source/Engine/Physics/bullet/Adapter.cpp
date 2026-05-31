/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/RigidBody.hpp>
#include <Physics/PhysicsShape.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/math/Vector3.hpp>
#include <Core/math/Quat4f.hpp>

#if defined(HYP_BULLET) && HYP_BULLET
#include <Physics/bullet/Adapter.hpp>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"

namespace Hyperion {

static inline btVector3 ToBtVector(const Vec3f& vec)
{
    return btVector3(vec.x, vec.y, vec.z);
}

static inline Vec3f FromBtVector(const btVector3& vec)
{
    return Vec3f(vec.x(), vec.y(), vec.z());
}

static inline btQuaternion ToBtQuaternion(const Quat4f& quat)
{
    return btQuaternion(quat.x, quat.y, quat.z, quat.w);
}

static inline Quat4f FromBtQuaternion(const btQuaternion& quat)
{
    return Quat4f(quat.x(), quat.y(), quat.z(), quat.w());
}

struct RigidBodyInternalData
{
    RC<btRigidBody> rigidBody;
    RC<btMotionState> motionState;
};

struct CharacterControllerInternalData
{
    RC<btPairCachingGhostObject> ghostObject;
    RC<btCapsuleShape> capsuleShape;
    RC<btKinematicCharacterController> kcc;
};

static RC<btCollisionShape> CreatePhysicsShapeHandle(PhysicsShape* physicsShape)
{
    Assert(physicsShape != nullptr);

    switch (physicsShape->GetType())
    {
    case PhysicsShapeType::BOX:
        return MakeRefCountedPtr<btBoxShape>(ToBtVector(static_cast<BoxPhysicsShape*>(physicsShape)->GetAABB().GetExtent() * 0.5f));
    case PhysicsShapeType::SPHERE:
        return MakeRefCountedPtr<btSphereShape>(static_cast<SpherePhysicsShape*>(physicsShape)->GetSphere().GetRadius());
    case PhysicsShapeType::PLANE:
        return MakeRefCountedPtr<btStaticPlaneShape>(
            ToBtVector(static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().GetXYZ()),
            static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().w);
    case PhysicsShapeType::CONVEX_HULL:
        static_assert(sizeof(btScalar) == sizeof(float), "sizeof(btScalar) must be sizeof(float) for reinterpret_cast to be safe");

        return MakeRefCountedPtr<btConvexHullShape>(
            reinterpret_cast<const btScalar*>(static_cast<ConvexHullPhysicsShape*>(physicsShape)->GetVertexData()),
            static_cast<ConvexHullPhysicsShape*>(physicsShape)->NumVertices(),
            sizeof(float) * 3);
    default:
        HYP_UNREACHABLE();
    }
}

BulletPhysicsAdapter::BulletPhysicsAdapter()
    : m_broadphase(nullptr),
      m_collisionConfiguration(nullptr),
      m_dispatcher(nullptr),
      m_solver(nullptr),
      m_dynamicsWorld(nullptr)
{
}

BulletPhysicsAdapter::~BulletPhysicsAdapter()
{
    Assert(m_collisionConfiguration == nullptr);
    Assert(m_dynamicsWorld == nullptr);
    Assert(m_dispatcher == nullptr);
    Assert(m_broadphase == nullptr);
    Assert(m_solver == nullptr);
}

void BulletPhysicsAdapter::Init(PhysicsWorldBase* world)
{
    Assert(m_collisionConfiguration == nullptr);
    Assert(m_dynamicsWorld == nullptr);
    Assert(m_dispatcher == nullptr);
    Assert(m_broadphase == nullptr);
    Assert(m_solver == nullptr);

    m_collisionConfiguration = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);
    m_broadphase = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver();
    m_dynamicsWorld = new btDiscreteDynamicsWorld(
        m_dispatcher,
        m_broadphase,
        m_solver,
        m_collisionConfiguration);

    m_dynamicsWorld->setGravity(btVector3(
        world->GetGravity().x,
        world->GetGravity().y,
        world->GetGravity().z));

    // Required for btKinematicCharacterController ghost objects
    m_dynamicsWorld->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
}

void BulletPhysicsAdapter::Teardown(PhysicsWorldBase* world)
{
    delete m_dynamicsWorld;
    m_dynamicsWorld = nullptr;

    delete m_solver;
    m_solver = nullptr;

    delete m_broadphase;
    m_broadphase = nullptr;

    delete m_dispatcher;
    m_dispatcher = nullptr;

    delete m_collisionConfiguration;
    m_collisionConfiguration = nullptr;
}

void BulletPhysicsAdapter::Tick(PhysicsWorldBase* world, double delta)
{
    Assert(m_dynamicsWorld != nullptr);

    m_dynamicsWorld->stepSimulation(delta);

    for (Handle<RigidBody>& rigidBody : world->GetRigidBodies())
    {
        RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetHandle());

        btTransform btTransform;
        internalData->motionState->getWorldTransform(btTransform);

        Transform rigidBodyTransform = rigidBody->GetTransform();
        rigidBodyTransform.GetTranslation() = FromBtVector(btTransform.getOrigin());
        rigidBodyTransform.GetRotation() = FromBtQuaternion(btTransform.getRotation()).Inverse();

        rigidBody->SetTransform(rigidBodyTransform);
    }
}

void BulletPhysicsAdapter::OnRigidBodyAdded(const Handle<RigidBody>& rigidBody)
{
    Assert(m_dynamicsWorld != nullptr);

    Assert(rigidBody.IsValid());
    Assert(rigidBody->shape != nullptr, "No PhysicsShape on RigidBody!");

    if (!rigidBody->shape->GetHandle())
    {
        rigidBody->shape->SetHandle(CreatePhysicsShapeHandle(rigidBody->shape));
    }

    btVector3 localInertia(0, 0, 0);

    if (rigidBody->IsKinematic() && rigidBody->physicsMaterial->GetMass() != 0.0f)
    {
        static_cast<btCollisionShape*>(rigidBody->shape->GetHandle())
            ->calculateLocalInertia(rigidBody->physicsMaterial->GetMass(), localInertia);
    }

    RC<RigidBodyInternalData> internalData = MakeRefCountedPtr<RigidBodyInternalData>();

    btTransform btTransform;
    btTransform.setIdentity();
    btTransform.setOrigin(ToBtVector(rigidBody->GetTransform().GetTranslation()));
    btTransform.setRotation(ToBtQuaternion(rigidBody->GetTransform().GetRotation()));
    internalData->motionState = MakeRefCountedPtr<btDefaultMotionState>(btTransform);

    btRigidBody::btRigidBodyConstructionInfo constructionInfo(
        rigidBody->physicsMaterial->GetMass(),
        internalData->motionState.Get(),
        static_cast<btCollisionShape*>(rigidBody->shape->GetHandle()),
        localInertia);

    internalData->rigidBody = MakeRefCountedPtr<btRigidBody>(constructionInfo);
    internalData->rigidBody->setActivationState(DISABLE_DEACTIVATION); // TEMP
    internalData->rigidBody->setWorldTransform(btTransform);

    m_dynamicsWorld->addRigidBody(internalData->rigidBody.Get());

    rigidBody->SetHandle(std::move(internalData));
}

void BulletPhysicsAdapter::OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_dynamicsWorld != nullptr);

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetHandle());
    Assert(internalData != nullptr);

    m_dynamicsWorld->removeRigidBody(internalData->rigidBody.Get());
}

void BulletPhysicsAdapter::OnChangePhysicsShape(RigidBody* rigidBody)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_dynamicsWorld != nullptr);

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetHandle());
    Assert(internalData != nullptr);

    Assert(internalData->rigidBody != nullptr);

    btVector3 localInertia = internalData->rigidBody->getLocalInertia();

    if (rigidBody->shape != nullptr && rigidBody->shape->GetHandle() != nullptr)
    {
        if (rigidBody->IsKinematic() && rigidBody->physicsMaterial->GetMass() >= 0.00001f)
        {
            static_cast<btCollisionShape*>(rigidBody->shape->GetHandle())
                ->calculateLocalInertia(rigidBody->physicsMaterial->GetMass(), localInertia);
        }
    }

    internalData->rigidBody->setMassProps(
        rigidBody->physicsMaterial->GetMass(),
        localInertia);
}

void BulletPhysicsAdapter::OnChangePhysicsMaterial(RigidBody* rigidBody)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_dynamicsWorld != nullptr);

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetHandle());
    Assert(internalData != nullptr);

    Assert(internalData->rigidBody != nullptr);

    if (!rigidBody->shape->GetHandle())
    {
        rigidBody->shape->SetHandle(CreatePhysicsShapeHandle(rigidBody->shape));
    }

    internalData->rigidBody->setCollisionShape(static_cast<btCollisionShape*>(rigidBody->shape->GetHandle()));
}

void BulletPhysicsAdapter::ApplyForceToBody(const RigidBody* rigidBody, const Vec3f& force)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_dynamicsWorld != nullptr);

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetHandle());
    Assert(internalData != nullptr);

    Assert(internalData->rigidBody != nullptr);

    internalData->rigidBody->activate();
    internalData->rigidBody->applyCentralForce(ToBtVector(force));
}

void BulletPhysicsAdapter::OnCharacterControllerAdded(const CharacterControllerConfig& config, RC<void>& outPhysicsHandle)
{
    Assert(m_dynamicsWorld != nullptr);

    if (!config.shape || config.shape->GetType() != PhysicsShapeType::CAPSULE)
    {
        HYP_LOG(Physics, Error, "CharacterController requires a valid CapsulePhysicsShape");
        return;
    }

    CapsulePhysicsShape* capsuleShape = static_cast<CapsulePhysicsShape*>(config.shape.Get());

    RC<CharacterControllerInternalData> internalData = MakeRefCountedPtr<CharacterControllerInternalData>();
    internalData->capsuleShape = MakeRefCountedPtr<btCapsuleShape>(capsuleShape->GetRadius(), capsuleShape->GetHeight());
    internalData->ghostObject = MakeRefCountedPtr<btPairCachingGhostObject>();

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(ToBtVector(config.startTranslation));
    internalData->ghostObject->setWorldTransform(startTransform);
    internalData->ghostObject->setCollisionShape(internalData->capsuleShape.Get());
    internalData->ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    internalData->kcc = MakeRefCountedPtr<btKinematicCharacterController>(
        internalData->ghostObject.Get(),
        internalData->capsuleShape.Get(),
        config.stepHeight);

    internalData->kcc->setMaxSlope(btRadians(config.maxSlopeAngle));
    internalData->kcc->setJumpSpeed(config.jumpSpeed);
    internalData->kcc->setFallSpeed(config.fallSpeed);
    internalData->kcc->setGravity(btVector3(0.0f, -btFabs(m_dynamicsWorld->getGravity().y()), 0.0f));

    m_dynamicsWorld->addCollisionObject(
        internalData->ghostObject.Get(),
        btBroadphaseProxy::CharacterFilter,
        btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);

    m_dynamicsWorld->addAction(internalData->kcc.Get());

    outPhysicsHandle = std::move(internalData);
}

void BulletPhysicsAdapter::OnCharacterControllerRemoved(RC<void>& physicsHandle)
{
    Assert(m_dynamicsWorld != nullptr);

    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    m_dynamicsWorld->removeAction(internalData->kcc.Get());
    m_dynamicsWorld->removeCollisionObject(internalData->ghostObject.Get());
    physicsHandle.Reset();
}

void BulletPhysicsAdapter::SetCharacterWalkDirection(const RC<void>& physicsHandle, const Vec3f& velocity)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    internalData->kcc->setWalkDirection(ToBtVector(velocity));
}

void BulletPhysicsAdapter::ApplyCharacterJump(const RC<void>& physicsHandle)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    if (internalData->kcc->onGround())
    {
        internalData->kcc->jump(btVector3(0.0f, 1.0f, 0.0f));
    }
}

void BulletPhysicsAdapter::GetCharacterState(const RC<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    outTranslation = FromBtVector(internalData->ghostObject->getWorldTransform().getOrigin());
    outIsOnGround = internalData->kcc->onGround();
}

} // namespace Hyperion

#endif // HYP_BULLET_PHYSICS

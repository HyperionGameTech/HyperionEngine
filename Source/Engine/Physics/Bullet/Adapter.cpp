/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/RigidBody.hpp>
#include <Physics/PhysicsShape.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Quat4f.hpp>

#if defined(HYP_BULLET) && HYP_BULLET
#include <Physics/Bullet/Adapter.hpp>

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
    SharedPtr<btRigidBody> rigidBody;
    SharedPtr<btMotionState> motionState;
};

struct CharacterControllerInternalData
{
    SharedPtr<btPairCachingGhostObject> ghostObject;
    SharedPtr<btCapsuleShape> capsuleShape;
    SharedPtr<btKinematicCharacterController> kcc;
};

static SharedPtr<btCollisionShape> CreatePhysicsShapeHandle(PhysicsShape* physicsShape)
{
    Assert(physicsShape != nullptr);

    switch (physicsShape->GetType())
    {
    case PhysicsShapeType::Box:
        return MakeShared<btBoxShape>(ToBtVector(static_cast<BoxPhysicsShape*>(physicsShape)->GetAABB().GetExtent() * 0.5f));
    case PhysicsShapeType::Sphere:
        return MakeShared<btSphereShape>(static_cast<SpherePhysicsShape*>(physicsShape)->GetSphere().GetRadius());
    case PhysicsShapeType::Plane:
        return MakeShared<btStaticPlaneShape>(
            ToBtVector(static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().GetXYZ()),
            static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().w);
    case PhysicsShapeType::ConvexHull:
    {
        static_assert(sizeof(btScalar) == sizeof(float), "sizeof(btScalar) must be sizeof(float) for reinterpret_cast to be safe");

        ConvexHullPhysicsShape* shapeCasted = static_cast<ConvexHullPhysicsShape*>(physicsShape);

        // ensure data remains resident while we copy it
        TSharedResLock lock(*shapeCasted);

        AssertDebug(shapeCasted->NumVertices() > 0);

        return MakeShared<btConvexHullShape>(
            shapeCasted->GetVertexData(),
            shapeCasted->NumVertices(),
            sizeof(float) * 3);
    }
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
        RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());

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

    if (!rigidBody->shape->GetInternalData())
    {
        rigidBody->shape->SetInternalData(CreatePhysicsShapeHandle(rigidBody->shape));
    }

    btVector3 localInertia(0, 0, 0);

    if (rigidBody->IsKinematic() && rigidBody->physicsMaterial->GetMass() != 0.0f)
    {
        static_cast<btCollisionShape*>(rigidBody->shape->GetInternalData())
            ->calculateLocalInertia(rigidBody->physicsMaterial->GetMass(), localInertia);
    }

    SharedPtr<RigidBodyInternalData> internalData = MakeShared<RigidBodyInternalData>();

    btTransform btTransform;
    btTransform.setIdentity();
    btTransform.setOrigin(ToBtVector(rigidBody->GetTransform().GetTranslation()));
    btTransform.setRotation(ToBtQuaternion(rigidBody->GetTransform().GetRotation()));
    internalData->motionState = MakeShared<btDefaultMotionState>(btTransform);

    btRigidBody::btRigidBodyConstructionInfo constructionInfo(
        rigidBody->physicsMaterial->GetMass(),
        internalData->motionState.Get(),
        static_cast<btCollisionShape*>(rigidBody->shape->GetInternalData()),
        localInertia);

    internalData->rigidBody = MakeShared<btRigidBody>(constructionInfo);
    internalData->rigidBody->setActivationState(DISABLE_DEACTIVATION); // TEMP
    internalData->rigidBody->setWorldTransform(btTransform);

    m_dynamicsWorld->addRigidBody(internalData->rigidBody.Get());

    rigidBody->SetInternalData(std::move(internalData));
}

void BulletPhysicsAdapter::OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_dynamicsWorld != nullptr);

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());
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

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());
    Assert(internalData != nullptr);

    Assert(internalData->rigidBody != nullptr);

    btVector3 localInertia = internalData->rigidBody->getLocalInertia();

    if (rigidBody->shape != nullptr && rigidBody->shape->GetInternalData() != nullptr)
    {
        if (rigidBody->IsKinematic() && rigidBody->physicsMaterial->GetMass() >= 0.00001f)
        {
            static_cast<btCollisionShape*>(rigidBody->shape->GetInternalData())
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

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());
    Assert(internalData != nullptr);

    Assert(internalData->rigidBody != nullptr);

    if (!rigidBody->shape->GetInternalData())
    {
        rigidBody->shape->SetInternalData(CreatePhysicsShapeHandle(rigidBody->shape));
    }

    internalData->rigidBody->setCollisionShape(static_cast<btCollisionShape*>(rigidBody->shape->GetInternalData()));
}

void BulletPhysicsAdapter::ApplyForceToBody(const RigidBody* rigidBody, const Vec3f& force)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_dynamicsWorld != nullptr);

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());
    Assert(internalData != nullptr);

    Assert(internalData->rigidBody != nullptr);

    internalData->rigidBody->activate();
    internalData->rigidBody->applyCentralForce(ToBtVector(force));
}

void BulletPhysicsAdapter::OnCharacterControllerAdded(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle)
{
    Assert(m_dynamicsWorld != nullptr);

    if (!config.shape || !config.shape->IsA<CapsulePhysicsShape>())
    {
        HYP_LOG(Physics, Error, "CharacterController requires a valid CapsulePhysicsShape");
        return;
    }

    CapsulePhysicsShape* capsuleShape = StaticCast<CapsulePhysicsShape>(config.shape);

    SharedPtr<CharacterControllerInternalData> internalData = MakeShared<CharacterControllerInternalData>();
    internalData->capsuleShape = MakeShared<btCapsuleShape>(capsuleShape->GetRadius(), capsuleShape->GetHeight());
    internalData->ghostObject = MakeShared<btPairCachingGhostObject>();

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(ToBtVector(config.startTranslation));
    internalData->ghostObject->setWorldTransform(startTransform);
    internalData->ghostObject->setCollisionShape(internalData->capsuleShape.Get());
    internalData->ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    internalData->kcc = MakeShared<btKinematicCharacterController>(
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

    outPhysicsHandle = internalData;
}

void BulletPhysicsAdapter::OnCharacterControllerRemoved(SharedPtr<void>& physicsHandle)
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

void BulletPhysicsAdapter::SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    internalData->kcc->setWalkDirection(ToBtVector(velocity));
}

void BulletPhysicsAdapter::ApplyCharacterJump(const SharedPtr<void>& physicsHandle)
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

void BulletPhysicsAdapter::GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround)
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

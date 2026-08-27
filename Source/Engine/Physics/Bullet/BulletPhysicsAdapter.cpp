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

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/MathUtil.hpp>

#if defined(HYP_BULLET) && HYP_BULLET
#include <Physics/Bullet/BulletPhysicsAdapter.hpp>

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
    SharedPtr<btCollisionShape> collisionShape;
};

struct CharacterControllerInternalData
{
    SharedPtr<btPairCachingGhostObject> ghostObject;
    SharedPtr<btCapsuleShape> capsuleShape;
    SharedPtr<btKinematicCharacterController> kcc;
    Vec3f walkVelocity;
    float pushSpeedScale = 1.0f;
    float maxPushSpeed = 3.0f;
    float pushMassLimit = 350.0f;

    SharedPtr<btRigidBody> shadowBody;
};

static void ApplyCharacterPush(CharacterControllerInternalData& internalData, btCollisionWorld* collisionWorld, float substepDelta)
{
    const btVector3 walkDirection = ToBtVector(internalData.walkVelocity);
    const btScalar walkSpeed2 = walkDirection.length2();

    if (walkSpeed2 <= SIMD_EPSILON)
    {
        return;
    }

    const btScalar walkSpeed = btSqrt(walkSpeed2);
    const btVector3 sweepDirection = walkDirection / walkSpeed;

    constexpr btScalar PushProbeSkin = 0.1f;
    const btScalar probeDistance = walkSpeed * substepDelta + PushProbeSkin;

    btTransform sweepFrom = internalData.ghostObject->getWorldTransform();
    btTransform sweepTo = sweepFrom;
    sweepTo.setOrigin(sweepFrom.getOrigin() + sweepDirection * probeDistance);

    btCollisionWorld::ClosestConvexResultCallback sweepCallback(sweepFrom.getOrigin(), sweepTo.getOrigin());
    sweepCallback.m_collisionFilterGroup = internalData.ghostObject->getBroadphaseHandle()->m_collisionFilterGroup;
    sweepCallback.m_collisionFilterMask = internalData.ghostObject->getBroadphaseHandle()->m_collisionFilterMask;

    internalData.ghostObject->convexSweepTest(
        internalData.capsuleShape.Get(),
        sweepFrom,
        sweepTo,
        sweepCallback,
        collisionWorld->getDispatchInfo().m_allowedCcdPenetration);

    if (!sweepCallback.hasHit())
    {
        return;
    }

    btRigidBody* body = const_cast<btRigidBody*>(btRigidBody::upcast(sweepCallback.m_hitCollisionObject));

    if (!body || body->isStaticOrKinematicObject() || body->getInvMass() <= 0.0f)
    {
        return;
    }

    const btScalar mass = btScalar(1.0) / body->getInvMass();

    if (mass > internalData.pushMassLimit)
    {
        return;
    }

    const btVector3 hitNormal = sweepCallback.m_hitNormalWorld;

    if (hitNormal.dot(sweepDirection) >= btScalar(-0.0001))
    {
        return;
    }

    btVector3 pushDirection = sweepDirection;
    pushDirection.setY(0.0f);

    const btScalar pushDirectionLength2 = pushDirection.length2();

    if (pushDirectionLength2 <= SIMD_EPSILON)
    {
        return;
    }

    pushDirection /= btSqrt(pushDirectionLength2);

    const btScalar targetSpeed = btMin(walkSpeed * internalData.pushSpeedScale, internalData.maxPushSpeed);
    const btScalar currentSpeed = body->getLinearVelocity().dot(pushDirection);

    if (currentSpeed >= targetSpeed)
    {
        return;
    }

    constexpr btScalar MaxPushAcceleration = 15.0f;
    const btScalar deltaSpeed = btMin(targetSpeed - currentSpeed, MaxPushAcceleration * substepDelta);

    body->activate(true);
    body->applyCentralImpulse(pushDirection * (mass * deltaSpeed));
}

struct OffsetCollisionShape final : btCompoundShape
{
    SharedPtr<btCollisionShape> childShape;

    OffsetCollisionShape(SharedPtr<btCollisionShape> childShape, const btTransform& childTransform)
        : btCompoundShape(),
          childShape(std::move(childShape))
    {
        addChildShape(childTransform, this->childShape.Get());
    }

    ~OffsetCollisionShape() override
    {
        if (childShape != nullptr)
        {
            removeChildShape(childShape.Get());
        }
    }
};

static SharedPtr<btCollisionShape> CreatePhysicsShapeHandle(PhysicsShape* physicsShape, const Vec3f& scale)
{
    Assert(physicsShape != nullptr);

    switch (physicsShape->GetType())
    {
    case PhysicsShapeType::Box:
    {
        const BoundingBox& aabb = static_cast<BoxPhysicsShape*>(physicsShape)->GetAABB();
        SharedPtr<btBoxShape> boxShape = MakeSharedWithAllocator<btBoxShape, PhysicsAllocator>(ToBtVector(aabb.GetExtent() * 0.5f * scale));

        // btBoxShape is centered on the body origin. If the AABB is off-center, embed the box in a
        // compound shifted by the AABB's center so the collision matches the true bounds.
        const Vec3f center = aabb.GetCenter() * scale;

        if (center.LengthSquared() < MathUtil::epsilonF)
        {
            return boxShape;
        }

        btTransform childTransform;
        childTransform.setIdentity();
        childTransform.setOrigin(ToBtVector(center));

        return MakeSharedWithAllocator<OffsetCollisionShape, PhysicsAllocator>(std::move(boxShape), childTransform);
    }
    case PhysicsShapeType::Sphere:
    {
        const BoundingSphere& sphere = static_cast<SpherePhysicsShape*>(physicsShape)->GetSphere();

        // Non-uniform scale can't be represented by a single radius.
        // calculate the max to ensure coverage.
        const float radiusScale = MathUtil::Max(MathUtil::Abs(scale.x), MathUtil::Abs(scale.y), MathUtil::Abs(scale.z));
        SharedPtr<btSphereShape> sphereShape = MakeSharedWithAllocator<btSphereShape, PhysicsAllocator>(sphere.GetRadius() * radiusScale);

        // btSphereShape is centered on the body origin; shift it to the sphere's local center if any.
        const Vec3f center = sphere.GetCenter() * scale;

        if (center.LengthSquared() < MathUtil::epsilonF)
        {
            return sphereShape;
        }

        btTransform childTransform;
        childTransform.setIdentity();
        childTransform.setOrigin(ToBtVector(center));

        return MakeSharedWithAllocator<OffsetCollisionShape, PhysicsAllocator>(std::move(sphereShape), childTransform);
    }
    case PhysicsShapeType::Plane:
        return MakeSharedWithAllocator<btStaticPlaneShape, PhysicsAllocator>(
            ToBtVector(static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().GetXYZ()),
            static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().w);
    case PhysicsShapeType::ConvexHull:
    {
        static_assert(sizeof(btScalar) == sizeof(float), "sizeof(btScalar) must be sizeof(float) for reinterpret_cast to be safe");

        ConvexHullPhysicsShape* shapeCasted = static_cast<ConvexHullPhysicsShape*>(physicsShape);

        // ensure data remains resident while we copy it
        TSharedResLock lock(*shapeCasted);

        AssertDebug(shapeCasted->NumVertices() > 0);

        if (MathUtil::Abs(scale.x - 1.0f) > MathUtil::epsilonF
            || MathUtil::Abs(scale.y - 1.0f) > MathUtil::epsilonF
            || MathUtil::Abs(scale.z - 1.0f) > MathUtil::epsilonF)
        {
            HYP_LOG(Physics, Warning, "ConvexHull physics shape on a non-unit-scale entity; scale is not applied to convex hull collision shapes");
        }

        return MakeSharedWithAllocator<btConvexHullShape, PhysicsAllocator>(
            shapeCasted->GetVertexData(),
            shapeCasted->NumVertices(),
            sizeof(float) * 3);
    }
    default:
        HYP_UNREACHABLE();
    }
}

static bool s_bulletCustomAllocatorsInit = false;

BulletPhysicsAdapter::BulletPhysicsAdapter()
    : m_broadphase(nullptr),
      m_collisionConfiguration(nullptr),
      m_dispatcher(nullptr),
      m_solver(nullptr),
      m_dynamicsWorld(nullptr)
{
    if (!s_bulletCustomAllocatorsInit)
    {
        s_bulletCustomAllocatorsInit = true;

        // Custom bullet allocators
        btAlignedAllocSetCustomAligned(
            [](size_t size, int alignment) -> void*
            {
                return g_physicsPool->Allocate(size, static_cast<size_t>(alignment));
            },
            [](void* ptr)
            {
                g_physicsPool->Free(ptr);
            });
    }
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

    m_collisionConfiguration = HYP_POOL_NEW(g_physicsPool, btDefaultCollisionConfiguration);
    m_dispatcher = HYP_POOL_NEW(g_physicsPool, btCollisionDispatcher, m_collisionConfiguration);
    m_broadphase = HYP_POOL_NEW(g_physicsPool, btDbvtBroadphase);
    m_solver = HYP_POOL_NEW(g_physicsPool, btSequentialImpulseConstraintSolver);
    m_dynamicsWorld = HYP_POOL_NEW(g_physicsPool, btDiscreteDynamicsWorld,
        m_dispatcher,
        m_broadphase,
        m_solver,
        m_collisionConfiguration);

    m_dynamicsWorld->setGravity(btVector3(
        world->GetGravity().x,
        world->GetGravity().y,
        world->GetGravity().z));

    // Required for btKinematicCharacterController ghost objects
    m_dynamicsWorld->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(HYP_POOL_NEW(g_physicsPool, btGhostPairCallback));
}

void BulletPhysicsAdapter::Teardown(PhysicsWorldBase* world)
{
    PoolDelete(*g_physicsPool, m_dynamicsWorld);
    m_dynamicsWorld = nullptr;

    PoolDelete(*g_physicsPool, m_solver);
    m_solver = nullptr;

    PoolDelete(*g_physicsPool, m_broadphase);
    m_broadphase = nullptr;

    PoolDelete(*g_physicsPool, m_dispatcher);
    m_dispatcher = nullptr;

    PoolDelete(*g_physicsPool, m_collisionConfiguration);
    m_collisionConfiguration = nullptr;
}

void BulletPhysicsAdapter::Tick(PhysicsWorldBase* world, double delta)
{
    Assert(m_dynamicsWorld != nullptr);

    constexpr double fixedTimeStep = 1.0 / 120.0;
    constexpr int maxSubSteps = 8;
    constexpr double maxDelta = maxSubSteps * fixedTimeStep;

    const double clampedDelta = delta > maxDelta ? maxDelta : delta;

    m_dynamicsWorld->stepSimulation(clampedDelta, maxSubSteps, fixedTimeStep);

    for (Handle<RigidBody>& rigidBody : world->GetRigidBodies())
    {
        RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());

        const bool isSleeping = internalData->rigidBody->getActivationState() == ISLAND_SLEEPING;

        if (isSleeping != rigidBody->isSleeping)
        {
            if (!isSleeping)
            {
                // @TODO wake
            }
            
            rigidBody->isSleeping = isSleeping;
        }
        else if (isSleeping)
        {
            continue;
        }

        btTransform btTransform;
        internalData->motionState->getWorldTransform(btTransform);

        Transform rigidBodyTransform = rigidBody->GetTransform();
        rigidBodyTransform.GetTranslation() = FromBtVector(btTransform.getOrigin());
        rigidBodyTransform.GetRotation() = FromBtQuaternion(btTransform.getRotation()).Inverse();

        rigidBody->SetTransform(rigidBodyTransform);
        
        // Sync cached states
        rigidBody->SetVelocity(FromBtVector(internalData->rigidBody->getLinearVelocity()));
        rigidBody->SetAngularVelocity(FromBtVector(internalData->rigidBody->getAngularVelocity()));
    }

    // Reset transient memory
    g_physicsArena->Reset();
}

void BulletPhysicsAdapter::OnRigidBodyAdded(const Handle<RigidBody>& rigidBody)
{
    Assert(m_dynamicsWorld != nullptr);

    Assert(rigidBody.IsValid());
    Assert(rigidBody->shape != nullptr, "No PhysicsShape on RigidBody!");

    SharedPtr<RigidBodyInternalData> internalData = MakeSharedWithAllocator<RigidBodyInternalData, PhysicsAllocator>();
    internalData->collisionShape = CreatePhysicsShapeHandle(rigidBody->shape, rigidBody->GetTransform().GetScale());

    // Kinematic bodies are driven externally. Set mass 0 so the
    // solver never integrates them, but they still push the dynamic bodies they collide with.
    const bool isKinematic = rigidBody->IsKinematic();
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->GetMass();

    btVector3 localInertia(0, 0, 0);

    if (!isKinematic && mass > 0.0f)
    {
        internalData->collisionShape->calculateLocalInertia(mass, localInertia);
    }

    btTransform btTransform;
    btTransform.setIdentity();
    btTransform.setOrigin(ToBtVector(rigidBody->GetTransform().GetTranslation()));
    btTransform.setRotation(ToBtQuaternion(rigidBody->GetTransform().GetRotation().Inverse()));
    internalData->motionState = MakeSharedWithAllocator<btDefaultMotionState, PhysicsAllocator>(btTransform);

    btRigidBody::btRigidBodyConstructionInfo constructionInfo(
        mass,
        internalData->motionState.Get(),
        internalData->collisionShape.Get(),
        localInertia);

    internalData->rigidBody = MakeSharedWithAllocator<btRigidBody, PhysicsAllocator>(constructionInfo);
    internalData->rigidBody->setWorldTransform(btTransform);

    if (isKinematic)
    {
        internalData->rigidBody->setCollisionFlags(internalData->rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        internalData->rigidBody->setActivationState(DISABLE_DEACTIVATION);
    }
    else
    {
        internalData->rigidBody->setAngularVelocity(ToBtVector(rigidBody->GetAngularVelocity()));
        internalData->rigidBody->setLinearVelocity(ToBtVector(rigidBody->GetVelocity()));
    }

    m_dynamicsWorld->addRigidBody(internalData->rigidBody.Get());

    rigidBody->SetInternalData(std::move(internalData));
}

void BulletPhysicsAdapter::SetRigidBodyTransform(const Handle<RigidBody>& rigidBody, const Transform& transform)
{
    Assert(m_dynamicsWorld != nullptr);

    if (!rigidBody.IsValid())
    {
        return;
    }

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());

    if (!internalData || !internalData->rigidBody)
    {
        return;
    }

    btTransform bulletTransform;
    bulletTransform.setIdentity();
    bulletTransform.setOrigin(ToBtVector(transform.GetTranslation()));
    bulletTransform.setRotation(ToBtQuaternion(transform.GetRotation().Inverse()));

    internalData->rigidBody->setWorldTransform(bulletTransform);

    if (internalData->motionState)
    {
        internalData->motionState->setWorldTransform(bulletTransform);
    }

    m_dynamicsWorld->updateSingleAabb(internalData->rigidBody.Get());

    rigidBody->SetTransform(transform);
}

void BulletPhysicsAdapter::SetRigidBodyKinematic(const Handle<RigidBody>& rigidBody, bool isKinematic)
{
    Assert(m_dynamicsWorld != nullptr);

    if (!rigidBody.IsValid())
    {
        return;
    }

    RigidBodyInternalData* internalData = static_cast<RigidBodyInternalData*>(rigidBody->GetInternalData());

    if (!internalData || !internalData->rigidBody)
    {
        return;
    }

    btRigidBody* body = internalData->rigidBody.Get();

    // Bullet caches mass/inertia and collision flags at insertion time, so re-register the body
    // after switching between kinematic and dynamic.
    m_dynamicsWorld->removeRigidBody(body);

    const int collisionFlags = body->getCollisionFlags();

    if (isKinematic)
    {
        body->setMassProps(0.0f, btVector3(0, 0, 0));
        body->setCollisionFlags(collisionFlags | btCollisionObject::CF_KINEMATIC_OBJECT);
        body->setActivationState(DISABLE_DEACTIVATION);
        body->setLinearVelocity(btVector3(0, 0, 0));
        body->setAngularVelocity(btVector3(0, 0, 0));
    }
    else
    {
        const float mass = rigidBody->physicsMaterial->GetMass();

        btVector3 localInertia(0, 0, 0);

        if (mass > 0.0f)
        {
            internalData->collisionShape->calculateLocalInertia(mass, localInertia);
        }

        body->setCollisionFlags(collisionFlags & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        body->setMassProps(mass, localInertia);

        // Kinematic bodies don't integrate velocity, so restore whatever the game had
        // cached for this body instead of leaving it at the zero velocity forced above.
        body->setLinearVelocity(ToBtVector(rigidBody->GetVelocity()));
        body->setAngularVelocity(ToBtVector(rigidBody->GetAngularVelocity()));

        body->setActivationState(ACTIVE_TAG);
        body->activate(true);
    }

    m_dynamicsWorld->addRigidBody(body);

    rigidBody->SetIsKinematic(isKinematic);
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

    SharedPtr<btCollisionShape> newShape = CreatePhysicsShapeHandle(rigidBody->shape, rigidBody->GetTransform().GetScale());

    m_dynamicsWorld->removeRigidBody(internalData->rigidBody.Get());
    internalData->rigidBody->setCollisionShape(newShape.Get());
    internalData->collisionShape = std::move(newShape);

    const bool isKinematic = rigidBody->IsKinematic();
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->GetMass();

    btVector3 localInertia(0, 0, 0);

    if (!isKinematic && mass >= 0.00001f)
    {
        internalData->collisionShape->calculateLocalInertia(mass, localInertia);
    }

    internalData->rigidBody->setMassProps(mass, localInertia);

    m_dynamicsWorld->addRigidBody(internalData->rigidBody.Get());
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

    const bool isKinematic = rigidBody->IsKinematic();
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->GetMass();

    btVector3 localInertia(0, 0, 0);

    if (!isKinematic && mass > 0.0f)
    {
        internalData->collisionShape->calculateLocalInertia(mass, localInertia);
    }

    internalData->rigidBody->setMassProps(mass, localInertia);
    internalData->rigidBody->activate(true);
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

    SharedPtr<CharacterControllerInternalData> internalData = MakeSharedWithAllocator<CharacterControllerInternalData, PhysicsAllocator>();
    internalData->capsuleShape = MakeSharedWithAllocator<btCapsuleShape, PhysicsAllocator>(capsuleShape->GetRadius(), capsuleShape->GetHeight());
    internalData->ghostObject = MakeSharedWithAllocator<btPairCachingGhostObject, PhysicsAllocator>();

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(ToBtVector(config.startTranslation));
    internalData->ghostObject->setWorldTransform(startTransform);
    internalData->ghostObject->setCollisionShape(internalData->capsuleShape.Get());
    internalData->ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    internalData->kcc = MakeSharedWithAllocator<btKinematicCharacterController, PhysicsAllocator>(
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

    internalData->pushSpeedScale = config.pushSpeedScale;
    internalData->maxPushSpeed = config.maxPushSpeed;
    internalData->pushMassLimit = config.pushMassLimit;

    btRigidBody::btRigidBodyConstructionInfo shadowConstructionInfo(
        0.0f,
        nullptr,
        internalData->capsuleShape.Get(),
        btVector3(0.0f, 0.0f, 0.0f));
    shadowConstructionInfo.m_friction = 0.0f;
    shadowConstructionInfo.m_restitution = 0.0f;

    internalData->shadowBody = MakeSharedWithAllocator<btRigidBody, PhysicsAllocator>(shadowConstructionInfo);
    internalData->shadowBody->setWorldTransform(startTransform);
    internalData->shadowBody->setCollisionFlags(internalData->shadowBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    internalData->shadowBody->setActivationState(DISABLE_DEACTIVATION);
    internalData->shadowBody->setContactProcessingThreshold(btScalar(-0.02));

    m_dynamicsWorld->addRigidBody(
        internalData->shadowBody.Get(),
        btBroadphaseProxy::StaticFilter,
        btBroadphaseProxy::DefaultFilter);

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

    m_dynamicsWorld->removeCollisionObject(internalData->ghostObject.Get());

    if (internalData->shadowBody)
    {
        m_dynamicsWorld->removeRigidBody(internalData->shadowBody.Get());
    }

    physicsHandle.Reset();
}

void BulletPhysicsAdapter::SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    internalData->walkVelocity = velocity;
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

void BulletPhysicsAdapter::StepCharacterController(const SharedPtr<void>& physicsHandle, float deltaTime)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData || deltaTime <= 0.0f)
    {
        return;
    }

    constexpr float maxSubstepDelta = 1.0f / 60.0f;
    constexpr int maxSubsteps = 4;

    const float totalDelta = MathUtil::Min(deltaTime, maxSubstepDelta * float(maxSubsteps));
    const int numSubsteps = MathUtil::Min(int(MathUtil::Ceil(totalDelta / maxSubstepDelta)), maxSubsteps);
    const float substepDelta = totalDelta / float(numSubsteps);

    for (int i = 0; i < numSubsteps; ++i)
    {
        internalData->kcc->setWalkDirection(ToBtVector(internalData->walkVelocity) * substepDelta);
        internalData->kcc->updateAction(m_dynamicsWorld, substepDelta);

        ApplyCharacterPush(*internalData, m_dynamicsWorld, substepDelta);
    }

    if (internalData->shadowBody)
    {
        const btTransform newTransform = internalData->ghostObject->getWorldTransform();

        internalData->shadowBody->setWorldTransform(newTransform);

        m_dynamicsWorld->updateSingleAabb(internalData->shadowBody.Get());
    }
}

void BulletPhysicsAdapter::SetCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    btTransform transform = internalData->ghostObject->getWorldTransform();
    transform.setOrigin(ToBtVector(translation));
    internalData->ghostObject->setWorldTransform(transform);

    if (internalData->shadowBody)
    {
        internalData->shadowBody->setWorldTransform(transform);
        internalData->shadowBody->setInterpolationWorldTransform(transform);
        internalData->shadowBody->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));

        m_dynamicsWorld->updateSingleAabb(internalData->shadowBody.Get());
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

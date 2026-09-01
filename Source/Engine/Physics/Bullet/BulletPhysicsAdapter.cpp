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
    float shadowMaxSpeed = 60.0f;
    float shadowTeleportDistance = 0.5f;

    float pushMassLimit = 350.0f;
    float maxPushSpeed = 1.5f;
    float pushSpeedScale = 1.0f;

    Vec3f pushVelocity;

    //-- servo
    btVector3 ghostTravel { 0.0f, 0.0f, 0.0f };
    btScalar characterTime = 0.0f;
    int idleCharacterTicks = 0;
    btVector3 targetVelocity { 0.0f, 0.0f, 0.0f };
    btVector3 shadowVelocity { 0.0f, 0.0f, 0.0f };
    //--

    SharedPtr<btRigidBody> shadowBody;
    SharedPtr<btMotionState> shadowMotionState;

    Vec3f GetPushVelocity() const
    {
        Vec3f velocity = { walkVelocity.x, 0.0f, walkVelocity.z };

        const float speed = velocity.Length();
        const float pushSpeed = MathUtil::Min(speed * pushSpeedScale, maxPushSpeed);

        if (speed > pushSpeed && speed > MathUtil::epsilonF)
        {
            velocity *= pushSpeed / speed;
        }

        return velocity;
    }
};

static bool FindPushableBodies(CharacterControllerInternalData* character)
{
    Vec3f horizontalWalk = { character->walkVelocity.x, 0.0f, character->walkVelocity.z };

    if (horizontalWalk.LengthSquared() < MathUtil::epsilonF)
    {
        return false;
    }

    const btVector3 travelDirection = ToBtVector(horizontalWalk.Normalize());
    const btVector3 characterOrigin = character->ghostObject->getWorldTransform().getOrigin();

    bool foundPushable = false;

    for (int i = 0; i < character->ghostObject->getNumOverlappingObjects(); ++i)
    {
        btRigidBody* body = btRigidBody::upcast(character->ghostObject->getOverlappingObject(i));

        if (!body || body->isStaticOrKinematicObject() || body->getInvMass() <= 0.0f)
        {
            continue;
        }

        if (btScalar(1.0) / body->getInvMass() > character->pushMassLimit)
        {
            continue;
        }

        btVector3 toBody = body->getWorldTransform().getOrigin() - characterOrigin;
        toBody.setY(0.0f);

        if (toBody.length2() < SIMD_EPSILON || toBody.normalized().dot(travelDirection) < btScalar(0.5))
        {
            continue;
        }

        body->activate(true);

        foundPushable = true;
    }

    return foundPushable;
}

struct CharacterGhostOverlapFilter final : btOverlapFilterCallback
{
    const Set<btRigidBody*, PhysicsAllocator>* ghostNonCollidableBodies = nullptr;
    const Array<SharedPtr<void>, PhysicsAllocator>* characterControllers = nullptr;

    CharacterControllerInternalData* FindCharacterData(const btCollisionObject* collisionObject) const
    {
        if (characterControllers == nullptr)
        {
            return nullptr;
        }

        void* userPointer = collisionObject->getUserPointer();

        if (userPointer == nullptr)
        {
            return nullptr;
        }

        for (const SharedPtr<void>& physicsHandle : *characterControllers)
        {
            if (physicsHandle.GetVoid() == userPointer)
            {
                return static_cast<CharacterControllerInternalData*>(userPointer);
            }
        }

        return nullptr;
    }

    bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override
    {
        bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
        collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) != 0;

        if (!collides)
        {
            return false;
        }

        btCollisionObject* obj0 = static_cast<btCollisionObject*>(proxy0->m_clientObject);
        btCollisionObject* obj1 = static_cast<btCollisionObject*>(proxy1->m_clientObject);

        const bool obj0IsGhost = obj0->getInternalType() == btCollisionObject::CO_GHOST_OBJECT;
        const bool obj1IsGhost = obj1->getInternalType() == btCollisionObject::CO_GHOST_OBJECT;

        CharacterControllerInternalData* character = FindCharacterData(obj0);
        btCollisionObject* other = obj1;
        bool characterIsGhost = obj0IsGhost;

        if (character == nullptr)
        {
            character = FindCharacterData(obj1);
            other = obj0;
            characterIsGhost = obj1IsGhost;
        }

        if (character == nullptr)
        {
            return true;
        }

        btRigidBody* otherRigidBody = btRigidBody::upcast(other);

        if (!characterIsGhost && otherRigidBody && !otherRigidBody->isStaticOrKinematicObject())
        {
            const btScalar invMass = otherRigidBody->getInvMass();

            if (invMass <= 0.0f || btScalar(1.0) / invMass > character->pushMassLimit)
            {
                return false;
            }
        }

        if (characterIsGhost
            && otherRigidBody
            && otherRigidBody->isKinematicObject()
            && ghostNonCollidableBodies != nullptr
            && ghostNonCollidableBodies->Contains(otherRigidBody))
        {
            return false;
        }

        return true;
    }
};

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
      m_dynamicsWorld(nullptr),
      m_characterOverlapFilter(nullptr)
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

    m_characterOverlapFilter = HYP_POOL_NEW(g_physicsPool, CharacterGhostOverlapFilter);
    static_cast<CharacterGhostOverlapFilter*>(m_characterOverlapFilter)->ghostNonCollidableBodies = &m_ghostNonCollidableBodies;
    static_cast<CharacterGhostOverlapFilter*>(m_characterOverlapFilter)->characterControllers = &m_characterControllers;
    m_dynamicsWorld->getBroadphase()->getOverlappingPairCache()->setOverlapFilterCallback(m_characterOverlapFilter);

    // Character shadow bodies advance per substep (see AdvanceCharacterShadowBodies) so
    // that they present a smoothly moving surface to the contact solver.
    m_dynamicsWorld->setInternalTickCallback(
        [](btDynamicsWorld* world, btScalar timeStep)
        {
            static_cast<BulletPhysicsAdapter*>(world->getWorldUserInfo())->AdvanceCharacterShadowBodies(float(timeStep));
        },
        /* worldUserInfo */ this,
        /* isPreTick */ true);
}

void BulletPhysicsAdapter::Teardown(PhysicsWorldBase* world)
{
    PoolDelete(*g_physicsPool, m_characterOverlapFilter);
    m_characterOverlapFilter = nullptr;

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

    UpdateCharacterShadowBodies(clampedDelta);

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
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->mass;

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

    constructionInfo.m_friction = MathUtil::Max(rigidBody->physicsMaterial->friction, 0.0f);
    constructionInfo.m_restitution = MathUtil::Clamp(rigidBody->physicsMaterial->restitution, 0.0f, 1.0f);

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

void BulletPhysicsAdapter::MoveRigidBodyKinematic(const Handle<RigidBody>& rigidBody, const Transform& transform, float deltaTime)
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

    // Give bodies in contact with this one (e.g. a character standing on/pushing it) a velocity to
    // solve against instead of a teleport every time a network update arrives.
    if (deltaTime > 0.0f && internalData->rigidBody->isKinematicObject())
    {
        const btTransform oldTransform = internalData->rigidBody->getWorldTransform();

        const btVector3 deltaPosition = ToBtVector(transform.GetTranslation()) - oldTransform.getOrigin();
        internalData->rigidBody->setLinearVelocity(deltaPosition / deltaTime);

        const btQuaternion deltaRotation = ToBtQuaternion(transform.GetRotation().Inverse()) * oldTransform.getRotation().inverse();
        const btScalar angle = deltaRotation.getAngle();

        internalData->rigidBody->setAngularVelocity(
            angle > SIMD_EPSILON ? deltaRotation.getAxis() * (angle / deltaTime) : btVector3(0.0f, 0.0f, 0.0f));
    }

    SetRigidBodyTransform(rigidBody, transform);
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
        const float mass = rigidBody->physicsMaterial->mass;

        btVector3 localInertia(0, 0, 0);

        if (mass > 0.0f)
        {
            internalData->collisionShape->calculateLocalInertia(mass, localInertia);
        }

        body->setCollisionFlags(collisionFlags & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        body->setMassProps(mass, localInertia);
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

    m_ghostNonCollidableBodies.Erase(internalData->rigidBody.Get());

    m_dynamicsWorld->removeRigidBody(internalData->rigidBody.Get());
}

void BulletPhysicsAdapter::SetRigidBodyCharacterGhostCollidable(const Handle<RigidBody>& rigidBody, bool collidable)
{
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

    if (collidable)
    {
        m_ghostNonCollidableBodies.Erase(body);
    }
    else
    {
        m_ghostNonCollidableBodies.Insert(body);
    }
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
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->mass;

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
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->mass;

    btVector3 localInertia(0, 0, 0);

    if (!isKinematic && mass > 0.0f)
    {
        internalData->collisionShape->calculateLocalInertia(mass, localInertia);
    }

    internalData->rigidBody->setMassProps(mass, localInertia);
    internalData->rigidBody->setFriction(MathUtil::Max(rigidBody->physicsMaterial->friction, 0.0f));
    internalData->rigidBody->setRestitution(MathUtil::Clamp(rigidBody->physicsMaterial->restitution, 0.0f, 1.0f));
    internalData->rigidBody->activate(true);

    //internalData->rigidBody->setCollisionShape(internalData->collisionShape.Get());
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
    internalData->ghostObject->setUserPointer(internalData.Get());

    internalData->kcc = MakeSharedWithAllocator<btKinematicCharacterController, PhysicsAllocator>(
        internalData->ghostObject.Get(),
        internalData->capsuleShape.Get(),
        config.stepHeight);

    internalData->kcc->setMaxSlope(btRadians(config.maxSlopeAngle));
    internalData->kcc->setJumpSpeed(config.jumpSpeed);
    internalData->kcc->setFallSpeed(config.fallSpeed);
    internalData->kcc->setGravity(btVector3(0.0f, -btFabs(m_dynamicsWorld->getGravity().y()), 0.0f));
    internalData->kcc->setUseGhostSweepTest(false);

    m_dynamicsWorld->addCollisionObject(
        internalData->ghostObject.Get(),
        btBroadphaseProxy::CharacterFilter,
        btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);

    internalData->shadowMaxSpeed = config.shadowMaxSpeed;
    internalData->shadowTeleportDistance = config.shadowTeleportDistance;
    internalData->pushMassLimit = config.pushMassLimit;
    internalData->maxPushSpeed = config.maxPushSpeed;
    internalData->pushSpeedScale = config.pushSpeedScale;

    internalData->shadowMotionState = MakeSharedWithAllocator<btDefaultMotionState, PhysicsAllocator>(startTransform);

    btRigidBody::btRigidBodyConstructionInfo shadowConstructionInfo(
        0.0f,
        internalData->shadowMotionState.Get(),
        internalData->capsuleShape.Get(),
        btVector3(0.0f, 0.0f, 0.0f));

    internalData->shadowBody = MakeSharedWithAllocator<btRigidBody, PhysicsAllocator>(shadowConstructionInfo);
    internalData->shadowBody->setWorldTransform(startTransform);
    internalData->shadowBody->setCollisionFlags(internalData->shadowBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    internalData->shadowBody->setActivationState(DISABLE_DEACTIVATION);
    internalData->shadowBody->setUserPointer(internalData.Get());

    m_dynamicsWorld->addRigidBody(
        internalData->shadowBody.Get(),
        btBroadphaseProxy::StaticFilter,
        btBroadphaseProxy::DefaultFilter);

    outPhysicsHandle = internalData;
    m_characterControllers.PushBack(outPhysicsHandle);
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

    for (size_t i = 0; i < m_characterControllers.Size();)
    {
        if (m_characterControllers[i].GetVoid() == internalData)
        {
            m_characterControllers.EraseAt(i);

            continue;
        }

        ++i;
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

void BulletPhysicsAdapter::UpdateCharacterShadowBodies(double simDelta)
{
    if (simDelta <= 0.0)
    {
        return;
    }

    constexpr btScalar errorCorrectionRate = 40.0f;  // 1/s, fraction of position error closed per second
    constexpr btScalar velocitySmoothing = 0.5f;

    const btScalar correctionRate = MathUtil::Min(errorCorrectionRate, btScalar(1.0 / simDelta));

    for (SharedPtr<void>& physicsHandle : m_characterControllers)
    {
        CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

        if (!internalData || !internalData->shadowBody)
        {
            continue;
        }

        if (internalData->characterTime > 0.0f)
        {
            const btVector3 measuredVelocity = internalData->ghostTravel / internalData->characterTime;

            internalData->targetVelocity = internalData->targetVelocity.lerp(measuredVelocity, velocitySmoothing);
            internalData->idleCharacterTicks = 0;
        }
        else if (++internalData->idleCharacterTicks >= 2)
        {
            internalData->targetVelocity *= 0.85f;
        }

        internalData->ghostTravel.setValue(0.0f, 0.0f, 0.0f);
        internalData->characterTime = 0.0f;

        const btTransform targetTransform = internalData->ghostObject->getWorldTransform();
        const btTransform previousTransform = internalData->shadowBody->getWorldTransform();

        const btVector3 positionError = targetTransform.getOrigin() - previousTransform.getOrigin();
        const btScalar errorLength = positionError.length();

        if (errorLength > btScalar(internalData->shadowTeleportDistance))
        {
            internalData->shadowBody->setWorldTransform(targetTransform);
            internalData->shadowMotionState->setWorldTransform(targetTransform);
            internalData->shadowBody->setInterpolationWorldTransform(targetTransform);
            internalData->shadowBody->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));

            // Discontinuity -- drop the stale velocity estimate along with the error.
            internalData->targetVelocity.setValue(0.0f, 0.0f, 0.0f);
            internalData->shadowVelocity.setValue(0.0f, 0.0f, 0.0f);

            continue;
        }

        btVector3 chaseVelocity = internalData->targetVelocity + positionError * correctionRate;
        const btScalar chaseSpeed = chaseVelocity.length();

        if (chaseSpeed > btScalar(internalData->shadowMaxSpeed))
        {
            chaseVelocity *= btScalar(internalData->shadowMaxSpeed) / chaseSpeed;
        }

        internalData->shadowVelocity = chaseVelocity;
    }
}

void BulletPhysicsAdapter::AdvanceCharacterShadowBodies(float substepDelta)
{
    if (substepDelta <= 0.0f || m_characterControllers.Empty())
    {
        return;
    }

    for (SharedPtr<void>& physicsHandle : m_characterControllers)
    {
        CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

        if (!internalData || !internalData->shadowBody)
        {
            continue;
        }

        btTransform advancedTransform = internalData->shadowBody->getWorldTransform();
        advancedTransform.setOrigin(advancedTransform.getOrigin() + internalData->shadowVelocity * btScalar(substepDelta));

        internalData->shadowBody->setWorldTransform(advancedTransform);
        internalData->shadowMotionState->setWorldTransform(advancedTransform);
        internalData->shadowBody->setLinearVelocity(internalData->shadowVelocity);
    }
}

void BulletPhysicsAdapter::StepCharacterController(const SharedPtr<void>& physicsHandle, float deltaTime)
{
    CharacterControllerInternalData* internalData = static_cast<CharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData || deltaTime <= 0.0f)
    {
        return;
    }

    // Step in fixed substeps
    constexpr float maxSubstepDelta = 1.0f / 60.0f;
    constexpr int maxSubsteps = 3;

    const float totalDelta = MathUtil::Min(deltaTime, maxSubstepDelta * float(maxSubsteps));
    const int numSubsteps = MathUtil::Min(int(MathUtil::Ceil(totalDelta / maxSubstepDelta)), maxSubsteps);
    const float substepDelta = totalDelta / float(numSubsteps);

    internalData->pushVelocity = internalData->GetPushVelocity();

    Vec3f walkVelocity = internalData->walkVelocity;

    if (FindPushableBodies(internalData))
    {
        // Don't let the character outrun the object it's pushing
        walkVelocity = internalData->pushVelocity + Vec3f(0.0f, walkVelocity.y, 0.0f);
    }

    const btVector3 ghostPositionBeforeStep = internalData->ghostObject->getWorldTransform().getOrigin();

    for (int i = 0; i < numSubsteps; ++i)
    {
        internalData->kcc->setWalkDirection(ToBtVector(walkVelocity) * substepDelta);
        internalData->kcc->updateAction(m_dynamicsWorld, substepDelta);
    }

    // accum ghost travel
    internalData->ghostTravel += internalData->ghostObject->getWorldTransform().getOrigin() - ghostPositionBeforeStep;
    internalData->characterTime += btScalar(totalDelta);

    // The shadow body deliberately is NOT moved here (chases during tick)
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

    // Reset states
    internalData->pushVelocity = Vec3f::Zero();
    internalData->ghostTravel.setValue(0.0f, 0.0f, 0.0f);
    internalData->characterTime = 0.0f;
    internalData->idleCharacterTicks = 0;
    internalData->targetVelocity.setValue(0.0f, 0.0f, 0.0f);
    internalData->shadowVelocity.setValue(0.0f, 0.0f, 0.0f);

    if (internalData->shadowBody)
    {
        internalData->shadowBody->setWorldTransform(transform);
        internalData->shadowMotionState->setWorldTransform(transform);

        internalData->shadowBody->setInterpolationWorldTransform(transform);
        internalData->shadowBody->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
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

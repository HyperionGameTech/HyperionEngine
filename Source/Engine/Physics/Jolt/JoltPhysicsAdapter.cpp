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

#if defined(HYP_JOLT) && HYP_JOLT
#include <Physics/Jolt/JoltPhysicsAdapter.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstring>
#include <thread>

namespace Hyperion {

static inline JPH::Vec3 ToJPHVec(const Vec3f& vec)
{
    return JPH::Vec3(vec.x, vec.y, vec.z);
}

static inline Vec3f FromJPHVec(const JPH::Vec3& vec)
{
    return Vec3f(vec.GetX(), vec.GetY(), vec.GetZ());
}

static inline JPH::Quat ToJPHQuat(const Quat4f& quat)
{
    return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
}

static inline Quat4f FromJPHQuat(const JPH::Quat& quat)
{
    return Quat4f(quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW());
}

namespace JoltLayers {

static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;

}

static constexpr JPH::BroadPhaseLayer JoltBroadPhaseNON_MOVING(0);
static constexpr JPH::BroadPhaseLayer JoltBroadPhaseMOVING(1);

class JoltBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
    virtual JPH::uint GetNumBroadPhaseLayers() const override
    {
        return 2;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        JPH_ASSERT(layer < JoltLayers::NUM_LAYERS);

        return layer == JoltLayers::MOVING ? JoltBroadPhaseMOVING : JoltBroadPhaseNON_MOVING;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        switch (static_cast<JPH::BroadPhaseLayer::Type>(layer))
        {
        case static_cast<JPH::BroadPhaseLayer::Type>(JoltBroadPhaseNON_MOVING): return "NON_MOVING";
        case static_cast<JPH::BroadPhaseLayer::Type>(JoltBroadPhaseMOVING): return "MOVING";
        default: return "INVALID";
        }
    }
#endif
};

class JoltObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override
    {
        switch (object1)
        {
        case JoltLayers::NON_MOVING:
            return object2 == JoltLayers::MOVING;
        case JoltLayers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

class JoltObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
    {
        switch (layer)
        {
        case JoltLayers::NON_MOVING:
            return broadPhaseLayer == JoltBroadPhaseMOVING;
        case JoltLayers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

class JoltCharacterBodyFilter final : public JPH::BodyFilter
{
public:
    const Set<uint32, PhysicsAllocator>* ghostNonCollidableBodyIds = nullptr;

    virtual bool ShouldCollide(const JPH::BodyID& bodyID) const override
    {
        return !ghostNonCollidableBodyIds->Contains(bodyID.GetIndexAndSequenceNumber());
    }
};

struct JoltRigidBodyInternalData
{
    JPH::BodyID bodyID;
    JPH::RefConst<JPH::Shape> shape;
    bool isDynamic = false;
};

struct JoltCharacterControllerInternalData
{
    JPH::Ref<JPH::CharacterVirtual> character;
    Vec3f walkVelocity;
    float capsuleCenterOffset = 0.0f;
    float stepHeight = 0.35f;
    float jumpSpeed = 7.0f;
    float fallSpeed = 55.0f;

    float coyoteTime = 0.15f;
    float jumpBufferTime = 0.15f;

    float coyoteTimeRemaining = 0.0f;
    float jumpBufferTimeRemaining = 0.0f;
};

static JPH::MassProperties CreateMassProperties(const JPH::Shape* shape, float mass)
{
    JPH::MassProperties massProperties = shape->GetMassProperties();

    massProperties.ScaleToMass(MathUtil::Max(mass, MathUtil::epsilonF));

    return massProperties;
}

static JPH::RefConst<JPH::Shape> CreatePhysicsShapeHandle(PhysicsShape* physicsShape, const Vec3f& scale)
{
    Assert(physicsShape != nullptr);

    switch (physicsShape->GetType())
    {
    case PhysicsShapeType::Box:
    {
        const BoundingBox& aabb = static_cast<BoxPhysicsShape*>(physicsShape)->GetAABB();

        const Vec3f halfExtent = aabb.GetExtent() * 0.5f * scale;

        JPH::Ref<JPH::BoxShape> boxShape = new JPH::BoxShape(ToJPHVec(halfExtent));

        const Vec3f center = aabb.GetCenter() * scale;

        if (center.LengthSquared() < MathUtil::epsilonF)
        {
            return boxShape.GetPtr();
        }

        return new JPH::RotatedTranslatedShape(ToJPHVec(center), JPH::Quat::sIdentity(), boxShape.GetPtr());
    }
    case PhysicsShapeType::Sphere:
    {
        const BoundingSphere& sphere = static_cast<SpherePhysicsShape*>(physicsShape)->GetSphere();

        const float radiusScale = MathUtil::Max(MathUtil::Abs(scale.x), MathUtil::Abs(scale.y), MathUtil::Abs(scale.z));

        JPH::Ref<JPH::SphereShape> sphereShape = new JPH::SphereShape(sphere.GetRadius() * MathUtil::Max(radiusScale, MathUtil::epsilonF));

        const Vec3f center = sphere.GetCenter() * scale;

        if (center.LengthSquared() < MathUtil::epsilonF)
        {
            return sphereShape.GetPtr();
        }

        return new JPH::RotatedTranslatedShape(ToJPHVec(center), JPH::Quat::sIdentity(), sphereShape.GetPtr());
    }
    case PhysicsShapeType::Plane:
        return new JPH::PlaneShape(JPH::Plane(
            ToJPHVec(static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().GetXYZ()),
            static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().w));
    case PhysicsShapeType::ConvexHull:
    {
        ConvexHullPhysicsShape* shapeCasted = static_cast<ConvexHullPhysicsShape*>(physicsShape);

        TSharedResLock lock(*shapeCasted);

        AssertDebug(shapeCasted->NumVertices() > 0);

        if (MathUtil::Abs(scale.x - 1.0f) > MathUtil::epsilonF
            || MathUtil::Abs(scale.y - 1.0f) > MathUtil::epsilonF
            || MathUtil::Abs(scale.z - 1.0f) > MathUtil::epsilonF)
        {
            HYP_LOG(Physics, Warning, "ConvexHull physics shape on a non-unit-scale entity; scale is not applied to convex hull collision shapes");
        }

        JPH::Array<JPH::Vec3> points;
        points.reserve(shapeCasted->NumVertices());

        const float* vertexData = shapeCasted->GetVertexData();

        for (size_t i = 0; i < shapeCasted->NumVertices(); ++i)
        {
            points.push_back(JPH::Vec3(vertexData[i * 3], vertexData[i * 3 + 1], vertexData[i * 3 + 2]));
        }

        JPH::ConvexHullShapeSettings settings(points.data(), int(points.size()));

        JPH::ShapeSettings::ShapeResult result = settings.Create();

        if (!result.IsValid())
        {
            HYP_LOG(Physics, Error, "Failed to create ConvexHull physics shape: {}", result.GetError().c_str());

            return new JPH::SphereShape(0.05f);
        }

        return result.Get();
    }
    default:
        HYP_UNREACHABLE();
    }
}

static uint32 s_joltReferenceCount = 0;

JoltPhysicsAdapter::JoltPhysicsAdapter()
    : m_physicsSystem(nullptr),
      m_jobSystem(nullptr),
      m_tempAllocator(nullptr),
      m_broadPhaseLayerInterface(nullptr),
      m_objectLayerPairFilter(nullptr),
      m_objectVsBroadPhaseLayerFilter(nullptr),
      m_characterBodyFilter(nullptr),
      m_characterVsCharacterCollision(nullptr),
      m_needsBroadphaseOptimize(false)
{
}

JoltPhysicsAdapter::~JoltPhysicsAdapter()
{
    Assert(m_physicsSystem == nullptr);
    Assert(m_jobSystem == nullptr);
    Assert(m_tempAllocator == nullptr);
}

void JoltPhysicsAdapter::Init(PhysicsWorldBase* world)
{
    Assert(m_physicsSystem == nullptr);

    if (s_joltReferenceCount++ == 0)
    {
        Assert(JPH::Factory::sInstance == nullptr);

        JPH::Allocate = [](size_t inSize) -> void*
        {
            return g_physicsPool->Allocate(inSize);
        };

        JPH::Free = [](void* inBlock)
        {
            g_physicsPool->Free(inBlock);
        };

        JPH::AlignedAllocate = [](size_t inSize, size_t inAlignment) -> void*
        {
            return g_physicsPool->Allocate(inSize, inAlignment);
        };

        JPH::AlignedFree = [](void* inBlock)
        {
            g_physicsPool->Free(inBlock);
        };

        JPH::Reallocate = [](void* inBlock, size_t inOldSize, size_t inNewSize) -> void*
        {
            void* newBlock = g_physicsPool->Allocate(inNewSize);

            if (inBlock)
            {
                std::memcpy(newBlock, inBlock, MathUtil::Min(inOldSize, inNewSize));
                g_physicsPool->Free(inBlock);
            }

            return newBlock;
        };

        JPH::Factory::sInstance = new JPH::Factory();

        JPH::RegisterTypes();
    }

    constexpr uint32 cMaxPhysicsJobs = 2048;
    constexpr uint32 cMaxPhysicsBarriers = 8;

    const int numThreads = MathUtil::Max(int(std::thread::hardware_concurrency()) - 1, 1);

    // Give some slack for bookkeeping
    m_tempAllocator = new JPH::TempAllocatorImpl(g_physicsPool->GetBlockSize() - (1 * 1024 * 1024));
    m_jobSystem = new JPH::JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, numThreads);
    m_broadPhaseLayerInterface = new JoltBroadPhaseLayerInterface();
    m_objectLayerPairFilter = new JoltObjectLayerPairFilter();
    m_objectVsBroadPhaseLayerFilter = new JoltObjectVsBroadPhaseLayerFilter();
    m_characterBodyFilter = new JoltCharacterBodyFilter();
    static_cast<JoltCharacterBodyFilter*>(m_characterBodyFilter)->ghostNonCollidableBodyIds = &m_ghostNonCollidableBodyIds;
    m_characterVsCharacterCollision = new JPH::CharacterVsCharacterCollisionSimple();

    m_physicsSystem = new JPH::PhysicsSystem();
    m_physicsSystem->Init(
        65536,
        0,
        65536,
        10240,
        *static_cast<JoltBroadPhaseLayerInterface*>(m_broadPhaseLayerInterface),
        *static_cast<JoltObjectVsBroadPhaseLayerFilter*>(m_objectVsBroadPhaseLayerFilter),
        *static_cast<JoltObjectLayerPairFilter*>(m_objectLayerPairFilter));

    m_physicsSystem->SetGravity(ToJPHVec(world->GetGravity()));
}

void JoltPhysicsAdapter::Teardown(PhysicsWorldBase* world)
{
    Assert(m_physicsSystem != nullptr);

    delete m_physicsSystem;
    m_physicsSystem = nullptr;

    delete m_characterVsCharacterCollision;
    m_characterVsCharacterCollision = nullptr;

    delete m_characterBodyFilter;
    m_characterBodyFilter = nullptr;

    delete m_objectVsBroadPhaseLayerFilter;
    m_objectVsBroadPhaseLayerFilter = nullptr;

    delete m_objectLayerPairFilter;
    m_objectLayerPairFilter = nullptr;

    delete m_broadPhaseLayerInterface;
    m_broadPhaseLayerInterface = nullptr;

    delete m_jobSystem;
    m_jobSystem = nullptr;

    delete m_tempAllocator;
    m_tempAllocator = nullptr;

    if (--s_joltReferenceCount == 0)
    {
        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

void JoltPhysicsAdapter::Tick(PhysicsWorldBase* world, double delta)
{
    Assert(m_physicsSystem != nullptr);

    constexpr double fixedTimeStep = 1.0 / 120.0;
    constexpr int maxSubSteps = 8;
    constexpr double maxDelta = maxSubSteps * fixedTimeStep;

    const double clampedDelta = delta > maxDelta ? maxDelta : delta;

    if (m_needsBroadphaseOptimize)
    {
        m_needsBroadphaseOptimize = false;

        m_physicsSystem->OptimizeBroadPhase();
    }

    const int numCollisionSteps = MathUtil::Clamp(int(MathUtil::Ceil(clampedDelta / fixedTimeStep)), 1, maxSubSteps);

    m_physicsSystem->Update(float(clampedDelta), numCollisionSteps, m_tempAllocator, m_jobSystem);

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    for (Handle<RigidBody>& rigidBody : world->GetRigidBodies())
    {
        JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());

        if (!internalData || internalData->bodyID.IsInvalid())
        {
            continue;
        }

        bool isSleeping = true;

        {
            JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), internalData->bodyID);

            if (lock.SucceededAndIsInBroadPhase())
            {
                isSleeping = !lock.GetBody().IsActive();
            }
        }

        if (isSleeping != rigidBody->isSleeping)
        {
            rigidBody->isSleeping = isSleeping;
        }
        else if (isSleeping)
        {
            continue;
        }

        JPH::RVec3 position;
        JPH::Quat rotation;

        bodyInterface.GetPositionAndRotation(internalData->bodyID, position, rotation);

        Transform rigidBodyTransform = rigidBody->GetTransform();
        rigidBodyTransform.GetTranslation() = FromJPHVec(position);
        rigidBodyTransform.GetRotation() = FromJPHQuat(rotation).Inverse();

        rigidBody->SetTransform(rigidBodyTransform);

        JPH::Vec3 linearVelocity;
        JPH::Vec3 angularVelocity;

        bodyInterface.GetLinearAndAngularVelocity(internalData->bodyID, linearVelocity, angularVelocity);

        rigidBody->SetVelocity(FromJPHVec(linearVelocity));
        rigidBody->SetAngularVelocity(FromJPHVec(angularVelocity));
    }

    g_physicsArena->Reset();
}

void JoltPhysicsAdapter::OnRigidBodyAdded(const Handle<RigidBody>& rigidBody)
{
    Assert(m_physicsSystem != nullptr);

    Assert(rigidBody.IsValid());
    Assert(rigidBody->shape != nullptr, "No PhysicsShape on RigidBody!");

    SharedPtr<JoltRigidBodyInternalData> internalData = MakeSharedWithAllocator<JoltRigidBodyInternalData, PhysicsAllocator>();
    internalData->shape = CreatePhysicsShapeHandle(rigidBody->shape, rigidBody->GetTransform().GetScale());

    const bool isKinematic = rigidBody->IsKinematic();
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->mass;

    JPH::EMotionType motionType = isKinematic ? JPH::EMotionType::Kinematic
        : (mass > MathUtil::epsilonF ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static);

    if (rigidBody->shape->GetType() == PhysicsShapeType::Plane)
    {
        motionType = JPH::EMotionType::Static;
    }

    const JPH::ObjectLayer objectLayer = motionType == JPH::EMotionType::Static ? JoltLayers::NON_MOVING : JoltLayers::MOVING;

    JPH::BodyCreationSettings creationSettings(
        internalData->shape.GetPtr(),
        ToJPHVec(rigidBody->GetTransform().GetTranslation()),
        ToJPHQuat(rigidBody->GetTransform().GetRotation().Inverse()),
        motionType,
        objectLayer);

    creationSettings.mFriction = MathUtil::Max(rigidBody->physicsMaterial->friction, 0.0f);
    creationSettings.mRestitution = MathUtil::Clamp(rigidBody->physicsMaterial->restitution, 0.0f, 1.0f);
    creationSettings.mAllowDynamicOrKinematic = true;

    internalData->isDynamic = motionType == JPH::EMotionType::Dynamic;

    if (internalData->isDynamic)
    {
        creationSettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
        creationSettings.mMassPropertiesOverride = CreateMassProperties(internalData->shape.GetPtr(), mass);
        creationSettings.mLinearVelocity = ToJPHVec(rigidBody->GetVelocity());
        creationSettings.mAngularVelocity = ToJPHVec(rigidBody->GetAngularVelocity());
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    JPH::Body* body = bodyInterface.CreateBody(creationSettings);
    Assert(body != nullptr);

    internalData->bodyID = body->GetID();

    m_bodyIdToRigidBody.Set(internalData->bodyID.GetIndexAndSequenceNumber(), rigidBody);

    bodyInterface.AddBody(
        internalData->bodyID,
        internalData->isDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

    m_needsBroadphaseOptimize = true;

    rigidBody->SetInternalData(std::move(internalData));
}

void JoltPhysicsAdapter::SetRigidBodyTransform(const Handle<RigidBody>& rigidBody, const Transform& transform)
{
    Assert(m_physicsSystem != nullptr);

    if (!rigidBody.IsValid())
    {
        return;
    }

    JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());

    if (!internalData || internalData->bodyID.IsInvalid())
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    bodyInterface.SetPositionAndRotation(
        internalData->bodyID,
        ToJPHVec(transform.GetTranslation()),
        ToJPHQuat(transform.GetRotation().Inverse()),
        JPH::EActivation::DontActivate);

    rigidBody->SetTransform(transform);
}

void JoltPhysicsAdapter::MoveRigidBodyKinematic(const Handle<RigidBody>& rigidBody, const Transform& transform, float deltaTime)
{
    Assert(m_physicsSystem != nullptr);

    if (!rigidBody.IsValid())
    {
        return;
    }

    JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());

    if (!internalData || internalData->bodyID.IsInvalid())
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    // MoveKinematic derives the velocity needed to reach the target over deltaTime, so bodies in
    // contact with this one (e.g. a character standing on/pushing it) get a continuously-moving
    // surface to solve against instead of a teleport every time a network update arrives.
    if (deltaTime > 0.0f && bodyInterface.GetMotionType(internalData->bodyID) == JPH::EMotionType::Kinematic)
    {
        bodyInterface.MoveKinematic(
            internalData->bodyID,
            ToJPHVec(transform.GetTranslation()),
            ToJPHQuat(transform.GetRotation().Inverse()),
            deltaTime);

        rigidBody->SetTransform(transform);

        return;
    }

    SetRigidBodyTransform(rigidBody, transform);
}

void JoltPhysicsAdapter::SetRigidBodyKinematic(const Handle<RigidBody>& rigidBody, bool isKinematic)
{
    Assert(m_physicsSystem != nullptr);

    if (!rigidBody.IsValid())
    {
        return;
    }

    JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());

    if (!internalData || internalData->bodyID.IsInvalid())
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    if (isKinematic)
    {
        bodyInterface.SetLinearAndAngularVelocity(internalData->bodyID, JPH::Vec3::sZero(), JPH::Vec3::sZero());

        bodyInterface.SetMotionType(internalData->bodyID, JPH::EMotionType::Kinematic, JPH::EActivation::DontActivate);
    }
    else
    {
        const float mass = rigidBody->physicsMaterial->mass;

        {
            JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), internalData->bodyID);

            if (lock.SucceededAndIsInBroadPhase() && lock.GetBody().GetMotionPropertiesUnchecked() != nullptr)
            {
                JPH::Body& body = lock.GetBody();

                body.GetMotionPropertiesUnchecked()->SetMassProperties(
                    JPH::EAllowedDOFs::All,
                    CreateMassProperties(body.GetShape(), mass));
            }
        }

        bodyInterface.SetMotionType(internalData->bodyID, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
    }

    internalData->isDynamic = !isKinematic && rigidBody->physicsMaterial->mass > MathUtil::epsilonF;

    rigidBody->SetIsKinematic(isKinematic);
}

void JoltPhysicsAdapter::OnRigidBodyRemoved(const Handle<RigidBody>& rigidBody)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_physicsSystem != nullptr);

    JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());
    Assert(internalData != nullptr);

    m_ghostNonCollidableBodyIds.Erase(internalData->bodyID.GetIndexAndSequenceNumber());
    m_bodyIdToRigidBody.Erase(internalData->bodyID.GetIndexAndSequenceNumber());

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    bodyInterface.RemoveBody(internalData->bodyID);
    bodyInterface.DestroyBody(internalData->bodyID);
}

void JoltPhysicsAdapter::SetRigidBodyCharacterGhostCollidable(const Handle<RigidBody>& rigidBody, bool collidable)
{
    // no-op
}

void JoltPhysicsAdapter::OnChangePhysicsShape(RigidBody* rigidBody)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_physicsSystem != nullptr);

    JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());
    Assert(internalData != nullptr);

    JPH::RefConst<JPH::Shape> newShape = CreatePhysicsShapeHandle(rigidBody->shape, rigidBody->GetTransform().GetScale());

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    bodyInterface.SetShape(
        internalData->bodyID,
        newShape.GetPtr(),
        false,
        internalData->isDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

    internalData->shape = newShape;

    const bool isKinematic = rigidBody->IsKinematic();
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->mass;

    internalData->isDynamic = !isKinematic && mass > MathUtil::epsilonF;

    if (internalData->isDynamic)
    {
        JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), internalData->bodyID);

        if (lock.SucceededAndIsInBroadPhase() && lock.GetBody().GetMotionPropertiesUnchecked() != nullptr)
        {
            JPH::Body& body = lock.GetBody();

            body.GetMotionPropertiesUnchecked()->SetMassProperties(
                JPH::EAllowedDOFs::All,
                CreateMassProperties(body.GetShape(), mass));
        }
    }
}

void JoltPhysicsAdapter::OnChangePhysicsMaterial(RigidBody* rigidBody)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_physicsSystem != nullptr);

    JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());
    Assert(internalData != nullptr);

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    const bool isKinematic = rigidBody->IsKinematic();
    const float mass = isKinematic ? 0.0f : rigidBody->physicsMaterial->mass;

    internalData->isDynamic = !isKinematic && mass > MathUtil::epsilonF;

    if (internalData->isDynamic)
    {
        JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), internalData->bodyID);

        if (lock.SucceededAndIsInBroadPhase() && lock.GetBody().GetMotionPropertiesUnchecked() != nullptr)
        {
            JPH::Body& body = lock.GetBody();

            body.GetMotionPropertiesUnchecked()->SetMassProperties(
                JPH::EAllowedDOFs::All,
                CreateMassProperties(body.GetShape(), mass));
        }
    }

    bodyInterface.SetFriction(internalData->bodyID, MathUtil::Max(rigidBody->physicsMaterial->friction, 0.0f));
    bodyInterface.SetRestitution(internalData->bodyID, MathUtil::Clamp(rigidBody->physicsMaterial->restitution, 0.0f, 1.0f));

    if (internalData->isDynamic)
    {
        bodyInterface.ActivateBody(internalData->bodyID);
    }
}

void JoltPhysicsAdapter::ApplyForceToBody(const RigidBody* rigidBody, const Vec3f& force)
{
    if (!rigidBody)
    {
        return;
    }

    Assert(m_physicsSystem != nullptr);

    JoltRigidBodyInternalData* internalData = static_cast<JoltRigidBodyInternalData*>(rigidBody->GetInternalData());

    if (!internalData || !internalData->isDynamic || internalData->bodyID.IsInvalid())
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    bodyInterface.AddForce(internalData->bodyID, ToJPHVec(force), JPH::EActivation::Activate);
}

void JoltPhysicsAdapter::OnCharacterControllerAdded(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle)
{
    Assert(m_physicsSystem != nullptr);

    if (!config.shape || !config.shape->IsA<CapsulePhysicsShape>())
    {
        HYP_LOG(Physics, Error, "CharacterController requires a valid CapsulePhysicsShape");
        return;
    }

    CapsulePhysicsShape* capsuleShape = StaticCast<CapsulePhysicsShape>(config.shape);

    const float radius = capsuleShape->GetRadius();
    const float cylinderHeight = MathUtil::Max(capsuleShape->GetHeight(), MathUtil::epsilonF);
    const float totalHeight = cylinderHeight + 2.0f * radius;

    JPH::Ref<JPH::CapsuleShape> capsule = new JPH::CapsuleShape(cylinderHeight * 0.5f, radius);

    JPH::Ref<JPH::RotatedTranslatedShape> shape = new JPH::RotatedTranslatedShape(
        JPH::Vec3(0.0f, totalHeight * 0.5f, 0.0f),
        JPH::Quat::sIdentity(),
        capsule.GetPtr());

    JPH::CharacterVirtualSettings settings;

    settings.mMaxSlopeAngle = JPH::DegreesToRadians(config.maxSlopeAngle);
    settings.mShape = shape.GetPtr();
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);

    const Vec3f startFeetPosition = config.startTranslation - Vec3f(0.0f, totalHeight * 0.5f, 0.0f);

    SharedPtr<JoltCharacterControllerInternalData> internalData = MakeSharedWithAllocator<JoltCharacterControllerInternalData, PhysicsAllocator>();
    internalData->character = new JPH::CharacterVirtual(
        &settings,
        ToJPHVec(startFeetPosition),
        JPH::Quat::sIdentity(),
        0,
        m_physicsSystem);
    internalData->capsuleCenterOffset = totalHeight * 0.5f;
    internalData->stepHeight = config.stepHeight;
    internalData->jumpSpeed = config.jumpSpeed;
    internalData->fallSpeed = config.fallSpeed;
    internalData->coyoteTime = config.coyoteTime;
    internalData->jumpBufferTime = config.jumpBufferTime;

    m_characterVsCharacterCollision->Add(internalData->character.GetPtr());

    outPhysicsHandle = internalData;
    m_characterControllers.PushBack(outPhysicsHandle);
}

void JoltPhysicsAdapter::OnCharacterControllerRemoved(SharedPtr<void>& physicsHandle)
{
    Assert(m_physicsSystem != nullptr);

    JoltCharacterControllerInternalData* internalData = static_cast<JoltCharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    m_characterVsCharacterCollision->Remove(internalData->character.GetPtr());

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

void JoltPhysicsAdapter::SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity)
{
    JoltCharacterControllerInternalData* internalData = static_cast<JoltCharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    internalData->walkVelocity = velocity;
}

void JoltPhysicsAdapter::ApplyCharacterJump(const SharedPtr<void>& physicsHandle)
{
    JoltCharacterControllerInternalData* internalData = static_cast<JoltCharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    internalData->jumpBufferTimeRemaining = internalData->jumpBufferTime;
}

void JoltPhysicsAdapter::StepCharacterController(const SharedPtr<void>& physicsHandle, float deltaTime)
{
    JoltCharacterControllerInternalData* internalData = static_cast<JoltCharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData || deltaTime <= 0.0f)
    {
        return;
    }

    JPH::CharacterVirtual* character = internalData->character.GetPtr();

    character->UpdateGroundVelocity();

    const JPH::Vec3 gravity = m_physicsSystem->GetGravity();

    const JPH::Vec3 walkVelocity = ToJPHVec(internalData->walkVelocity);
    const JPH::Vec3 horizontalInputVelocity = JPH::Vec3(walkVelocity.GetX(), 0.0f, walkVelocity.GetZ());

    constexpr float maxSubstepDelta = 1.0f / 60.0f;
    constexpr int maxSubsteps = 3;

    const float totalDelta = MathUtil::Min(deltaTime, maxSubstepDelta * float(maxSubsteps));
    const int numSubsteps = MathUtil::Min(int(MathUtil::Ceil(totalDelta / maxSubstepDelta)), maxSubsteps);
    const float substepDelta = totalDelta / float(numSubsteps);

    for (int i = 0; i < numSubsteps; ++i)
    {
        const JPH::Vec3 currentVelocity = character->GetLinearVelocity();
        const JPH::Vec3 currentVerticalVelocity = JPH::Vec3(0.0f, currentVelocity.GetY(), 0.0f);

        const JPH::Vec3 groundVelocity = character->GetGroundVelocity();

        const bool movingTowardsGround = (currentVerticalVelocity.GetY() - groundVelocity.GetY()) < 0.1f;

        const bool isGrounded = character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround
            && !character->IsSlopeTooSteep(character->GetGroundNormal());

        if (isGrounded)
        {
            internalData->coyoteTimeRemaining = internalData->coyoteTime;
        }
        else
        {
            internalData->coyoteTimeRemaining = MathUtil::Max(0.0f, internalData->coyoteTimeRemaining - substepDelta);
        }

        JPH::Vec3 newVelocity;

        if (isGrounded)
        {
            newVelocity = groundVelocity;
        }
        else
        {
            newVelocity = currentVerticalVelocity;
        }

        const bool canJump = internalData->jumpBufferTimeRemaining > 0.0f && (isGrounded || internalData->coyoteTimeRemaining > 0.0f);

        if (canJump && movingTowardsGround)
        {
            newVelocity += JPH::Vec3(0.0f, internalData->jumpSpeed, 0.0f);

            internalData->jumpBufferTimeRemaining = 0.0f;
            internalData->coyoteTimeRemaining = 0.0f;
        }
        else
        {
            internalData->jumpBufferTimeRemaining = MathUtil::Max(0.0f, internalData->jumpBufferTimeRemaining - substepDelta);
        }

        newVelocity += gravity * substepDelta;
        newVelocity += horizontalInputVelocity;

        if (newVelocity.GetY() < -internalData->fallSpeed)
        {
            newVelocity.SetY(-internalData->fallSpeed);
        }

        character->SetLinearVelocity(newVelocity);

        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;

        updateSettings.mWalkStairsStepUp = JPH::Vec3(0.0f, internalData->stepHeight, 0.0f);

        character->ExtendedUpdate(
            substepDelta,
            gravity,
            updateSettings,
            m_physicsSystem->GetDefaultBroadPhaseLayerFilter(JoltLayers::MOVING),
            m_physicsSystem->GetDefaultLayerFilter(JoltLayers::MOVING),
            *m_characterBodyFilter,
            JPH::ShapeFilter(),
            *m_tempAllocator);
    }
}

void JoltPhysicsAdapter::SetCharacterTranslation(const SharedPtr<void>& physicsHandle, const Vec3f& translation)
{
    JoltCharacterControllerInternalData* internalData = static_cast<JoltCharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    JPH::CharacterVirtual* character = internalData->character.GetPtr();

    const Vec3f feetPosition = translation - Vec3f(0.0f, internalData->capsuleCenterOffset, 0.0f);

    character->SetPosition(ToJPHVec(feetPosition));
    character->SetLinearVelocity(JPH::Vec3::sZero());

    internalData->jumpBufferTimeRemaining = 0.0f;
    internalData->coyoteTimeRemaining = 0.0f;
    internalData->walkVelocity = Vec3f::Zero();

    character->RefreshContacts(
        m_physicsSystem->GetDefaultBroadPhaseLayerFilter(JoltLayers::MOVING),
        m_physicsSystem->GetDefaultLayerFilter(JoltLayers::MOVING),
        *m_characterBodyFilter,
        JPH::ShapeFilter(),
        *m_tempAllocator);
}

void JoltPhysicsAdapter::GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround)
{
    JoltCharacterControllerInternalData* internalData = static_cast<JoltCharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    JPH::CharacterVirtual* character = internalData->character.GetPtr();

    outTranslation = FromJPHVec(character->GetPosition()) + Vec3f(0.0f, internalData->capsuleCenterOffset, 0.0f);
    outIsOnGround = character->IsSupported();
}

void JoltPhysicsAdapter::GetCharacterTouchedRigidBodies(const SharedPtr<void>& physicsHandle, Array<Handle<RigidBody>, PhysicsAllocator>& out)
{
    JoltCharacterControllerInternalData* internalData = static_cast<JoltCharacterControllerInternalData*>(physicsHandle.GetVoid());

    if (!internalData)
    {
        return;
    }

    JPH::CharacterVirtual* character = internalData->character.GetPtr();

    for (const JPH::CharacterContact& contact : character->GetActiveContacts())
    {
        if (!contact.mHadCollision || contact.mBodyB.IsInvalid())
        {
            continue;
        }

        auto it = m_bodyIdToRigidBody.Find(contact.mBodyB.GetIndexAndSequenceNumber());

        if (it == m_bodyIdToRigidBody.End())
        {
            continue;
        }

        const Handle<RigidBody>& rigidBody = it->second;

        if (!rigidBody)
        {
            continue;
        }

        if (!rigidBody->IsKinematic() && !rigidBody->IsLocallyPredicted())
        {
            continue;
        }

        if (rigidBody->physicsMaterial == nullptr || rigidBody->physicsMaterial->mass <= MathUtil::epsilonF)
        {
            continue;
        }

        out.PushBack(rigidBody);
    }
}

} // namespace Hyperion

#endif // HYP_JOLT_PHYSICS

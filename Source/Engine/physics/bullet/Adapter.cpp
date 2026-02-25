/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <physics/bullet/Adapter.hpp>
#include <physics/PhysicsWorld.hpp>
#include <physics/RigidBody.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/math/Vector3.hpp>
#include <Core/math/Quaternion.hpp>

#ifdef HYP_BULLET_PHYSICS
#include "btBulletDynamicsCommon.h"

namespace Hyperion {

static inline btVector3 ToBtVector(const Vec3f& vec)
{
    return btVector3(vec.x, vec.y, vec.z);
}

static inline Vec3f FromBtVector(const btVector3& vec)
{
    return Vec3f(vec.x(), vec.y(), vec.z());
}

static inline btQuaternion ToBtQuaternion(const Quaternion& quat)
{
    return btQuaternion(quat.x, quat.y, quat.z, quat.w);
}

static inline Quaternion FromBtQuaternion(const btQuaternion& quat)
{
    return Quaternion(quat.x(), quat.y(), quat.z(), quat.w());
}

struct RigidBodyInternalData
{
    UniquePtr<btRigidBody> rigidBody;
    UniquePtr<btMotionState> motionState;
};

static UniquePtr<btCollisionShape> CreatePhysicsShapeHandle(PhysicsShape* physicsShape)
{
    switch (physicsShape->GetType())
    {
    case PhysicsShapeType::BOX:
        return MakeUnique<btBoxShape>(
            ToBtVector(static_cast<BoxPhysicsShape*>(physicsShape)->GetAABB().GetExtent() * 0.5f));
    case PhysicsShapeType::SPHERE:
        return MakeUnique<btSphereShape>(
            static_cast<SpherePhysicsShape*>(physicsShape)->GetSphere().GetRadius());
    case PhysicsShapeType::PLANE:
        return MakeUnique<btStaticPlaneShape>(
            ToBtVector(static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().GetXYZ()),
            static_cast<PlanePhysicsShape*>(physicsShape)->GetPlane().w);
    case PhysicsShapeType::CONVEX_HULL:
        static_assert(sizeof(btScalar) == sizeof(float), "sizeof(btScalar) must be sizeof(float) for reinterpret_cast to be safe");

        return MakeUnique<btConvexHullShape>(
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
    Assert(rigidBody->GetShape() != nullptr, "No PhysicsShape on RigidBody!");

    if (!rigidBody->GetShape()->GetHandle())
    {
        rigidBody->GetShape()->SetHandle(CreatePhysicsShapeHandle(rigidBody->GetShape().Get()));
    }

    btVector3 localInertia(0, 0, 0);

    if (rigidBody->IsKinematic() && rigidBody->GetPhysicsMaterial().GetMass() != 0.0f)
    {
        static_cast<btCollisionShape*>(rigidBody->GetShape()->GetHandle())
            ->calculateLocalInertia(rigidBody->GetPhysicsMaterial().GetMass(), localInertia);
    }

    UniquePtr<RigidBodyInternalData> internalData = MakeUnique<RigidBodyInternalData>();

    btTransform btTransform;
    btTransform.setIdentity();
    btTransform.setOrigin(ToBtVector(rigidBody->GetTransform().GetTranslation()));
    btTransform.setRotation(ToBtQuaternion(rigidBody->GetTransform().GetRotation()));
    internalData->motionState = MakeUnique<btDefaultMotionState>(btTransform);

    btRigidBody::btRigidBodyConstructionInfo constructionInfo(
        rigidBody->GetPhysicsMaterial().GetMass(),
        internalData->motionState.Get(),
        static_cast<btCollisionShape*>(rigidBody->GetShape()->GetHandle()),
        localInertia);

    internalData->rigidBody = MakeUnique<btRigidBody>(constructionInfo);
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

    if (rigidBody->GetShape() != nullptr && rigidBody->GetShape()->GetHandle() != nullptr)
    {
        if (rigidBody->IsKinematic() && rigidBody->GetPhysicsMaterial().GetMass() >= 0.00001f)
        {
            static_cast<btCollisionShape*>(rigidBody->GetShape()->GetHandle())
                ->calculateLocalInertia(rigidBody->GetPhysicsMaterial().GetMass(), localInertia);
        }
    }

    internalData->rigidBody->setMassProps(
        rigidBody->GetPhysicsMaterial().GetMass(),
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

    if (!rigidBody->GetShape()->GetHandle())
    {
        rigidBody->GetShape()->SetHandle(CreatePhysicsShapeHandle(rigidBody->GetShape().Get()));
    }

    internalData->rigidBody->setCollisionShape(static_cast<btCollisionShape*>(rigidBody->GetShape()->GetHandle()));
}

void BulletPhysicsAdapter::ApplyForceToBody(const RigidBody* rigidBody, const Vector3& force)
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

} // namespace Hyperion

#endif

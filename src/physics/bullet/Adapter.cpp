/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <physics/bullet/Adapter.hpp>
#include <physics/PhysicsWorld.hpp>
#include <physics/RigidBody.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Quaternion.hpp>

#ifdef HYP_BULLET_PHYSICS
    #include "btBulletDynamicsCommon.h"

namespace hyperion::physics {

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
    UniquePtr<btRigidBody> rigid_body;
    UniquePtr<btMotionState> motion_state;
};

static UniquePtr<btCollisionShape> CreatePhysicsShapeHandle(PhysicsShape* physics_shape)
{
    switch (physics_shape->GetType())
    {
    case PhysicsShapeType::BOX:
        return MakeUnique<btBoxShape>(
            ToBtVector(static_cast<BoxPhysicsShape*>(physics_shape)->GetAABB().GetExtent() * 0.5f));
    case PhysicsShapeType::SPHERE:
        return MakeUnique<btSphereShape>(
            static_cast<SpherePhysicsShape*>(physics_shape)->GetSphere().GetRadius());
    case PhysicsShapeType::PLANE:
        return MakeUnique<btStaticPlaneShape>(
            ToBtVector(static_cast<PlanePhysicsShape*>(physics_shape)->GetPlane().GetXYZ()),
            static_cast<PlanePhysicsShape*>(physics_shape)->GetPlane().w);
    case PhysicsShapeType::CONVEX_HULL:
        static_assert(sizeof(btScalar) == sizeof(float), "sizeof(btScalar) must be sizeof(float) for reinterpret_cast to be safe");

        return MakeUnique<btConvexHullShape>(
            reinterpret_cast<const btScalar*>(static_cast<ConvexHullPhysicsShape*>(physics_shape)->GetVertexData()),
            static_cast<ConvexHullPhysicsShape*>(physics_shape)->NumVertices(),
            sizeof(float) * 3);
    default:
        AssertThrowMsg(false, "Unknown PhysicsShapeType!");
    }
}

BulletPhysicsAdapter::BulletPhysicsAdapter()
    : m_broadphase(nullptr),
      m_collision_configuration(nullptr),
      m_dispatcher(nullptr),
      m_solver(nullptr),
      m_dynamics_world(nullptr)
{
}

BulletPhysicsAdapter::~BulletPhysicsAdapter()
{
    AssertThrow(m_collision_configuration == nullptr);
    AssertThrow(m_dynamics_world == nullptr);
    AssertThrow(m_dispatcher == nullptr);
    AssertThrow(m_broadphase == nullptr);
    AssertThrow(m_solver == nullptr);
}

void BulletPhysicsAdapter::Init(PhysicsWorldBase* world)
{
    AssertThrow(m_collision_configuration == nullptr);
    AssertThrow(m_dynamics_world == nullptr);
    AssertThrow(m_dispatcher == nullptr);
    AssertThrow(m_broadphase == nullptr);
    AssertThrow(m_solver == nullptr);

    m_collision_configuration = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collision_configuration);
    m_broadphase = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver();
    m_dynamics_world = new btDiscreteDynamicsWorld(
        m_dispatcher,
        m_broadphase,
        m_solver,
        m_collision_configuration);

    m_dynamics_world->setGravity(btVector3(
        world->GetGravity().x,
        world->GetGravity().y,
        world->GetGravity().z));
}

void BulletPhysicsAdapter::Teardown(PhysicsWorldBase* world)
{
    delete m_dynamics_world;
    m_dynamics_world = nullptr;

    delete m_solver;
    m_solver = nullptr;

    delete m_broadphase;
    m_broadphase = nullptr;

    delete m_dispatcher;
    m_dispatcher = nullptr;

    delete m_collision_configuration;
    m_collision_configuration = nullptr;
}

void BulletPhysicsAdapter::Tick(PhysicsWorldBase* world, GameCounter::TickUnitHighPrec delta)
{
    AssertThrow(m_dynamics_world != nullptr);

    m_dynamics_world->stepSimulation(delta);

    for (Handle<RigidBody>& rigid_body : world->GetRigidBodies())
    {
        RigidBodyInternalData* internal_data = static_cast<RigidBodyInternalData*>(rigid_body->GetHandle());

        btTransform bt_transform;
        internal_data->motion_state->getWorldTransform(bt_transform);

        Transform rigid_body_transform = rigid_body->GetTransform();
        rigid_body_transform.GetTranslation() = FromBtVector(bt_transform.getOrigin());
        rigid_body_transform.GetRotation() = FromBtQuaternion(bt_transform.getRotation()).Invert();
        rigid_body_transform.UpdateMatrix();

        rigid_body->SetTransform(rigid_body_transform);
    }
}

void BulletPhysicsAdapter::OnRigidBodyAdded(const Handle<RigidBody>& rigid_body)
{
    AssertThrow(m_dynamics_world != nullptr);

    AssertThrow(rigid_body.IsValid());
    AssertThrowMsg(rigid_body->GetShape() != nullptr, "No PhysicsShape on RigidBody!");

    if (!rigid_body->GetShape()->GetHandle())
    {
        rigid_body->GetShape()->SetHandle(CreatePhysicsShapeHandle(rigid_body->GetShape().Get()));
    }

    btVector3 local_inertia(0, 0, 0);

    if (rigid_body->IsKinematic() && rigid_body->GetPhysicsMaterial().GetMass() != 0.0f)
    {
        static_cast<btCollisionShape*>(rigid_body->GetShape()->GetHandle())
            ->calculateLocalInertia(rigid_body->GetPhysicsMaterial().GetMass(), local_inertia);
    }

    UniquePtr<RigidBodyInternalData> internal_data = MakeUnique<RigidBodyInternalData>();

    btTransform bt_transform;
    bt_transform.setIdentity();
    bt_transform.setOrigin(ToBtVector(rigid_body->GetTransform().GetTranslation()));
    bt_transform.setRotation(ToBtQuaternion(rigid_body->GetTransform().GetRotation()));
    internal_data->motion_state = MakeUnique<btDefaultMotionState>(bt_transform);

    btRigidBody::btRigidBodyConstructionInfo construction_info(
        rigid_body->GetPhysicsMaterial().GetMass(),
        internal_data->motion_state.Get(),
        static_cast<btCollisionShape*>(rigid_body->GetShape()->GetHandle()),
        local_inertia);

    internal_data->rigid_body = MakeUnique<btRigidBody>(construction_info);
    internal_data->rigid_body->setActivationState(DISABLE_DEACTIVATION); // TEMP
    internal_data->rigid_body->setWorldTransform(bt_transform);

    m_dynamics_world->addRigidBody(internal_data->rigid_body.Get());

    rigid_body->SetHandle(std::move(internal_data));
}

void BulletPhysicsAdapter::OnRigidBodyRemoved(const Handle<RigidBody>& rigid_body)
{
    if (!rigid_body)
    {
        return;
    }

    AssertThrow(m_dynamics_world != nullptr);

    RigidBodyInternalData* internal_data = static_cast<RigidBodyInternalData*>(rigid_body->GetHandle());
    AssertThrow(internal_data != nullptr);

    m_dynamics_world->removeRigidBody(internal_data->rigid_body.Get());
}

void BulletPhysicsAdapter::OnChangePhysicsShape(RigidBody* rigid_body)
{
    if (!rigid_body)
    {
        return;
    }

    AssertThrow(m_dynamics_world != nullptr);

    RigidBodyInternalData* internal_data = static_cast<RigidBodyInternalData*>(rigid_body->GetHandle());
    AssertThrow(internal_data != nullptr);

    AssertThrow(internal_data->rigid_body != nullptr);

    btVector3 local_inertia = internal_data->rigid_body->getLocalInertia();

    if (rigid_body->GetShape() != nullptr && rigid_body->GetShape()->GetHandle() != nullptr)
    {
        if (rigid_body->IsKinematic() && rigid_body->GetPhysicsMaterial().GetMass() >= 0.00001f)
        {
            static_cast<btCollisionShape*>(rigid_body->GetShape()->GetHandle())
                ->calculateLocalInertia(rigid_body->GetPhysicsMaterial().GetMass(), local_inertia);
        }
    }

    internal_data->rigid_body->setMassProps(
        rigid_body->GetPhysicsMaterial().GetMass(),
        local_inertia);
}

void BulletPhysicsAdapter::OnChangePhysicsMaterial(RigidBody* rigid_body)
{
    if (!rigid_body)
    {
        return;
    }

    AssertThrow(m_dynamics_world != nullptr);

    RigidBodyInternalData* internal_data = static_cast<RigidBodyInternalData*>(rigid_body->GetHandle());
    AssertThrow(internal_data != nullptr);

    AssertThrow(internal_data->rigid_body != nullptr);

    if (!rigid_body->GetShape()->GetHandle())
    {
        rigid_body->GetShape()->SetHandle(CreatePhysicsShapeHandle(rigid_body->GetShape().Get()));
    }

    internal_data->rigid_body->setCollisionShape(static_cast<btCollisionShape*>(rigid_body->GetShape()->GetHandle()));
}

void BulletPhysicsAdapter::ApplyForceToBody(const RigidBody* rigid_body, const Vector3& force)
{
    if (!rigid_body)
    {
        return;
    }

    AssertThrow(m_dynamics_world != nullptr);

    RigidBodyInternalData* internal_data = static_cast<RigidBodyInternalData*>(rigid_body->GetHandle());
    AssertThrow(internal_data != nullptr);

    AssertThrow(internal_data->rigid_body != nullptr);

    internal_data->rigid_body->activate();
    internal_data->rigid_body->applyCentralForce(ToBtVector(force));
}

} // namespace hyperion::physics

#endif
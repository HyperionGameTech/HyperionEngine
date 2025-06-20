/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PHYSICS_BULLET_ADAPTER_HPP
#define HYPERION_PHYSICS_BULLET_ADAPTER_HPP

#include <physics/Adapter.hpp>

struct btDbvtBroadphase;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;

namespace hyperion::physics {

class HYP_API BulletPhysicsAdapter : public PhysicsAdapter<BulletPhysicsAdapter>
{
public:
    BulletPhysicsAdapter();
    ~BulletPhysicsAdapter();

    void Init(PhysicsWorldBase* world);
    void Teardown(PhysicsWorldBase* world);
    void Tick(PhysicsWorldBase* world, double delta);

    void OnRigidBodyAdded(const Handle<RigidBody>& rigid_body);
    void OnRigidBodyRemoved(const Handle<RigidBody>& rigid_body);

    void OnChangePhysicsShape(RigidBody* rigid_body);
    void OnChangePhysicsMaterial(RigidBody* rigid_body);

    void ApplyForceToBody(const RigidBody* rigid_body, const Vector3& force);

private:
    btDbvtBroadphase* m_broadphase;
    btDefaultCollisionConfiguration* m_collision_configuration;
    btCollisionDispatcher* m_dispatcher;
    btSequentialImpulseConstraintSolver* m_solver;
    btDiscreteDynamicsWorld* m_dynamics_world;
};

} // namespace hyperion::physics

#endif
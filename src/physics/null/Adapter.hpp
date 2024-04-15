/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_PHYSICS_NULL_ADAPTER_HPP
#define HYPERION_V2_PHYSICS_NULL_ADAPTER_HPP

#include <physics/Adapter.hpp>

namespace hyperion::v2::physics {

class HYP_API NullPhysicsAdapter : public PhysicsAdapter<NullPhysicsAdapter>
{
public:
    NullPhysicsAdapter();
    ~NullPhysicsAdapter();

    void Init(PhysicsWorldBase *world);
    void Teardown(PhysicsWorldBase *world);
    void Tick(PhysicsWorldBase *world, GameCounter::TickUnitHighPrec delta);

    void OnRigidBodyAdded(const Handle<RigidBody> &rigid_body);
    void OnRigidBodyRemoved(const Handle<RigidBody> &rigid_body);

    void OnChangePhysicsShape(RigidBody *rigid_body);
    void OnChangePhysicsMaterial(RigidBody *rigid_body);

    void ApplyForceToBody(const RigidBody *rigid_body, const Vector3 &force);
};

} // namespace hyperion::v2::physics

#endif
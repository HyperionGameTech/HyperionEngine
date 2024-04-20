/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PHYSICS_ADAPTER_HPP
#define HYPERION_PHYSICS_ADAPTER_HPP

#include <core/Handle.hpp>
#include <GameCounter.hpp>

namespace hyperion::physics {

class PhysicsWorldBase;
class RigidBody;

template <class DerivedAdapter>
class PhysicsAdapter
{
public:
    DerivedAdapter *GetDerivedAdapter()
        { return static_cast<DerivedAdapter *>(this); }

    const DerivedAdapter *GetDerivedAdapter() const
        { return static_cast<const DerivedAdapter *>(this); }

    void Init(PhysicsWorldBase *world)
        { GetDerivedAdapter()->DerivedAdapter::Init(world); }

    void Teardown(PhysicsWorldBase *world)
        { GetDerivedAdapter()->DerivedAdapter::Teardown(world); }

    void Tick(PhysicsWorldBase *world, GameCounter::TickUnitHighPrec delta)
        { GetDerivedAdapter()->DerivedAdapter::Tick(world, delta); }

    void OnRigidBodyAdded(const Handle<RigidBody> &rigid_body)
        { GetDerivedAdapter()->DerivedAdapter::OnRigidBodyAdded(rigid_body); }

    void OnRigidBodyRemoved(const Handle<RigidBody> &rigid_body)
        { GetDerivedAdapter()->DerivedAdapter::OnRigidBodyRemoved(rigid_body); }
    
    void OnChangePhysicsShape(RigidBody *rigid_body)
        { GetDerivedAdapter()->DerivedAdapter::OnChangePhysicsShape(rigid_body); }
    
    void OnChangePhysicsMaterial(RigidBody *rigid_body)
        { GetDerivedAdapter()->DerivedAdapter::OnChangePhysicsMaterial(rigid_body); }

    void ApplyForceToBody(const RigidBody *rigid_body, const Vector3 &force)
        { GetDerivedAdapter()->DerivedAdapter::ApplyForceToBody(rigid_body, force); }
};

} // namespace hyperion::physics

#endif
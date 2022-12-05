#ifndef HYPERION_V2_PHYSICS_ADAPTER_HPP
#define HYPERION_V2_PHYSICS_ADAPTER_HPP

#include <physics/RigidBody.hpp>
#include <core/ID.hpp>
#include <GameCounter.hpp>

#include <type_traits>

namespace hyperion::v2::physics {

class PhysicsWorldBase;

template <class DerivedAdapter>
class PhysicsAdapter
{
public:
    DerivedAdapter *GetDerivedAdapter()
        { return static_cast<DerivedAdapter *>(this); }

    const DerivedAdapter *GetDerivedAdapter() const
        { return static_cast<DerivedAdapter *>(this); }

    void Init(PhysicsWorldBase *world)
        { GetDerivedAdapter()->DerivedAdapter::Init(world); }

    void Teardown(PhysicsWorldBase *world)
        { GetDerivedAdapter()->DerivedAdapter::Teardown(world); }

    void Tick(PhysicsWorldBase *world, GameCounter::TickUnitHighPrec delta)
        { GetDerivedAdapter()->DerivedAdapter::Tick(world, delta); }

    void OnRigidBodyAdded(Handle<RigidBody> &rigid_body)
        { GetDerivedAdapter()->DerivedAdapter::OnRigidBodyAdded(rigid_body); }

    void OnRigidBodyRemoved(const Handle<RigidBody> &rigid_body)
        { GetDerivedAdapter()->DerivedAdapter::OnRigidBodRemoved(rigid_body); }
};

} // namespace hyperion::v2::physics

#endif
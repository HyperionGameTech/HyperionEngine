#ifndef HYPERION_V2_PHYSICS_WORLD_HPP
#define HYPERION_V2_PHYSICS_WORLD_HPP

#include <physics/Adapter.hpp>
#include <physics/RigidBody.hpp>
#include <core/Handle.hpp>
#include <core/Containers.hpp>
#include <math/Vector3.hpp>

#include <type_traits>

namespace hyperion::v2::physics {

class PhysicsWorldBase
{
public:
    static const Vector3 earth_gravity;

    PhysicsWorldBase() = default;
    PhysicsWorldBase(const PhysicsWorldBase &other) = delete;
    PhysicsWorldBase &operator=(const PhysicsWorldBase &other) = delete;
    ~PhysicsWorldBase() = default;

    const Vector3 &GetGravity() const
        { return m_gravity; }

    void SetGravity(const Vector3 &gravity)
        { m_gravity = gravity; }

    Array<Handle<RigidBody>> &GetRigidBodies()
        { return m_rigid_bodies; }
    
    const Array<Handle<RigidBody>> &GetRigidBodies() const
        { return m_rigid_bodies; }

protected:
    Vector3 m_gravity = earth_gravity;

    Array<Handle<RigidBody>> m_rigid_bodies;
};

template <class Adapter>
class PhysicsWorld : public PhysicsWorldBase
{
public:
    PhysicsWorld()
        : PhysicsWorldBase()
    {
    }

    ~PhysicsWorld()
    {
    }

    Adapter &GetAdapter()
        { return m_adapter; }

    const Adapter &GetAdapter() const
        { return m_adapter; }

    void AddRigidBody(Handle<RigidBody> &&rigid_body)
    {
        if (!rigid_body) {
            return;
        }

        m_adapter.OnRigidBodyAdded(rigid_body);
        m_rigid_bodies.PushBack(std::move(rigid_body));
    }

    void RemoveRigidBody(const Handle<RigidBody> &rigid_body)
    {
        if (!rigid_body) {
            return;
        }

        m_adapter.OnRigidBodyRemoved(rigid_body);
        m_rigid_bodies.Erase(rigid_body);
    }

    void Init()
        { m_adapter.Init(this); }

    void Teardown()
        { m_adapter.Teardown(this); }

    void Tick(GameCounter::TickUnitHighPrec delta)
        { m_adapter.Tick(this, delta); }

private:
    Adapter m_adapter;
};

} // namespace hyperion::v2::physics

#ifdef HYP_BULLET_PHYSICS

#include <physics/bullet/Adapter.hpp>

namespace hyperion::v2 {
using PhysicsWorld = physics::PhysicsWorld<physics::BulletPhysicsAdapter>;
} // namespace hyperion::v2

#endif

#endif
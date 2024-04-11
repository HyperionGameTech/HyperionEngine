#ifndef HYPERION_V2_PHYSICS_WORLD_HPP
#define HYPERION_V2_PHYSICS_WORLD_HPP

#include <physics/Adapter.hpp>
#include <physics/RigidBody.hpp>
#include <core/ID.hpp>
#include <core/Containers.hpp>
#include <math/Vector3.hpp>

#include <type_traits>

namespace hyperion::v2::physics {

class HYP_API PhysicsWorldBase
{
public:
    static constexpr Vec3f earth_gravity = Vec3f { 0.0f, -9.81f, 0.0f };

    PhysicsWorldBase()                                          = default;
    PhysicsWorldBase(const PhysicsWorldBase &other)             = delete;
    PhysicsWorldBase &operator=(const PhysicsWorldBase &other)  = delete;
    ~PhysicsWorldBase()                                         = default;

    const Vec3f &GetGravity() const
        { return m_gravity; }

    void SetGravity(const Vec3f &gravity)
        { m_gravity = gravity; }

    Array<Handle<RigidBody>> &GetRigidBodies()
        { return m_rigid_bodies; }
    
    const Array<Handle<RigidBody>> &GetRigidBodies() const
        { return m_rigid_bodies; }

protected:
    Vec3f                       m_gravity = earth_gravity;

    FlatSet<Handle<RigidBody>>  m_rigid_bodies;
};

template <class Adapter>
class HYP_API PhysicsWorld : public PhysicsWorldBase
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

    void AddRigidBody(const Handle<RigidBody> &rigid_body)
    {
        if (!rigid_body) {
            return;
        }

        const auto insert_result = m_rigid_bodies.Insert(rigid_body);

        if (insert_result.second) {
            m_adapter.OnRigidBodyAdded(rigid_body);
        }
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

#else

#include <physics/null/Adapter.hpp>

namespace hyperion::v2 {
using PhysicsWorld = physics::PhysicsWorld<physics::NullPhysicsAdapter>;
} // na

#endif

#endif
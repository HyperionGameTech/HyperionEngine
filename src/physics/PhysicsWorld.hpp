/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PHYSICS_WORLD_HPP
#define HYPERION_PHYSICS_WORLD_HPP

#include <physics/Adapter.hpp>
#include <physics/RigidBody.hpp>

#include <core/math/Vector3.hpp>

namespace hyperion {
namespace physics {

class HYP_API PhysicsWorldBase
{
public:
    static constexpr Vec3f earth_gravity = Vec3f { 0.0f, -9.81f, 0.0f };

    PhysicsWorldBase() = default;
    PhysicsWorldBase(const PhysicsWorldBase& other) = delete;
    PhysicsWorldBase& operator=(const PhysicsWorldBase& other) = delete;
    ~PhysicsWorldBase() = default;

    HYP_FORCE_INLINE const Vec3f& GetGravity() const
    {
        return m_gravity;
    }

    HYP_FORCE_INLINE void SetGravity(const Vec3f& gravity)
    {
        m_gravity = gravity;
    }

    HYP_FORCE_INLINE Array<Handle<RigidBody>>& GetRigidBodies()
    {
        return m_rigid_bodies;
    }

    HYP_FORCE_INLINE const Array<Handle<RigidBody>>& GetRigidBodies() const
    {
        return m_rigid_bodies;
    }

protected:
    Vec3f m_gravity = earth_gravity;

    FlatSet<Handle<RigidBody>> m_rigid_bodies;
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

    HYP_FORCE_INLINE Adapter& GetAdapter()
    {
        return m_adapter;
    }

    HYP_FORCE_INLINE const Adapter& GetAdapter() const
    {
        return m_adapter;
    }

    void AddRigidBody(const Handle<RigidBody>& rigid_body)
    {
        if (!rigid_body)
        {
            return;
        }

        const auto insert_result = m_rigid_bodies.Insert(rigid_body);

        if (insert_result.second)
        {
            m_adapter.OnRigidBodyAdded(rigid_body);
        }
    }

    void RemoveRigidBody(const Handle<RigidBody>& rigid_body)
    {
        if (!rigid_body)
        {
            return;
        }

        m_adapter.OnRigidBodyRemoved(rigid_body);
        m_rigid_bodies.Erase(rigid_body);
    }

    void Init()
    {
        m_adapter.Init(this);
    }

    void Teardown()
    {
        m_adapter.Teardown(this);
    }

    void Tick(GameCounter::TickUnitHighPrec delta)
    {
        m_adapter.Tick(this, delta);
    }

private:
    Adapter m_adapter;
};

} // namespace physics
} // namespace hyperion

#ifdef HYP_BULLET_PHYSICS

    #include <physics/bullet/Adapter.hpp>

namespace hyperion {
using PhysicsWorld = physics::PhysicsWorld<physics::BulletPhysicsAdapter>;
} // namespace hyperion

#else

    #include <physics/null/Adapter.hpp>

namespace hyperion {
using PhysicsWorld = physics::PhysicsWorld<physics::NullPhysicsAdapter>;
} // namespace hyperion

#endif

#endif
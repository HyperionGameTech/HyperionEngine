/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <physics/Adapter.hpp>
#include <physics/RigidBody.hpp>

#include <Core/math/Vector3.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class HYP_API PhysicsWorldBase : public ObjectBase
{
    HYP_OBJECT_BODY(PhysicsWorldBase);

public:
    static constexpr Vec3f earthGravity = Vec3f { 0.0f, -9.81f, 0.0f };

    PhysicsWorldBase() = default;
    PhysicsWorldBase(const PhysicsWorldBase& other) = delete;
    PhysicsWorldBase& operator=(const PhysicsWorldBase& other) = delete;
    virtual ~PhysicsWorldBase() override = default;

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
        return m_rigidBodies;
    }

    HYP_FORCE_INLINE const Array<Handle<RigidBody>>& GetRigidBodies() const
    {
        return m_rigidBodies;
    }

    virtual void Init() override
    {
        ObjectBase::Init();
    }

    virtual void Teardown() = 0;
    virtual void Tick(double delta) = 0;

    virtual void AddRigidBody(const Handle<RigidBody>& rigidBody) = 0;
    virtual void RemoveRigidBody(const Handle<RigidBody>& rigidBody) = 0;

protected:
    Vec3f m_gravity = earthGravity;

    FlatSet<Handle<RigidBody>> m_rigidBodies;
};

template <class Adapter>
class HYP_API TPhysicsWorld : public PhysicsWorldBase
{
public:
    TPhysicsWorld()
        : PhysicsWorldBase()
    {
    }

    ~TPhysicsWorld() override = default;

    HYP_FORCE_INLINE Adapter& GetAdapter()
    {
        return m_adapter;
    }

    HYP_FORCE_INLINE const Adapter& GetAdapter() const
    {
        return m_adapter;
    }

    void AddRigidBody(const Handle<RigidBody>& rigidBody) override
    {
        if (!rigidBody)
        {
            return;
        }

        const auto insertResult = m_rigidBodies.Insert(rigidBody);

        if (insertResult.second)
        {
            m_adapter.OnRigidBodyAdded(rigidBody);
        }
    }

    void RemoveRigidBody(const Handle<RigidBody>& rigidBody) override
    {
        if (!rigidBody)
        {
            return;
        }

        m_adapter.OnRigidBodyRemoved(rigidBody);
        m_rigidBodies.Erase(rigidBody);
    }

    void Init() override
    {
        m_adapter.Init(this);
    }

    void Teardown() override
    {
        m_adapter.Teardown(this);
    }

    void Tick(double delta) override
    {
        m_adapter.Tick(this, delta);
    }

private:
    Adapter m_adapter;
};

} // namespace Hyperion

#ifdef HYP_BULLET_PHYSICS

#include <physics/bullet/Adapter.hpp>

namespace Hyperion {
using PhysicsWorld = TPhysicsWorld<BulletPhysicsAdapter>;
} // namespace Hyperion

#else

#include <physics/null/Adapter.hpp>

namespace Hyperion {
using PhysicsWorld = TPhysicsWorld<NullPhysicsAdapter>;
} // namespace Hyperion

#endif

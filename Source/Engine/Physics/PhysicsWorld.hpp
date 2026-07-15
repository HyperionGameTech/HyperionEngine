/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Physics/Adapter.hpp>
#include <Physics/RigidBody.hpp>

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class ENGINE_API PhysicsWorldBase : public ObjectBase
{
    HYP_OBJECT_BODY(PhysicsWorldBase);

public:
    static constexpr Vec3f EarthGravity = Vec3f { 0.0f, -9.81f, 0.0f };

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

    virtual void Initialize() = 0;
    virtual void Teardown() = 0;

    virtual void Tick(double delta) = 0;

    virtual void AddRigidBody(const Handle<RigidBody>& rigidBody) = 0;
    virtual void RemoveRigidBody(const Handle<RigidBody>& rigidBody) = 0;

    virtual void AddCharacterController(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle) = 0;
    virtual void RemoveCharacterController(SharedPtr<void>& physicsHandle) = 0;
    virtual void SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity) = 0;
    virtual void ApplyCharacterJump(const SharedPtr<void>& physicsHandle) = 0;
    virtual void GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround) = 0;

protected:
    Vec3f m_gravity = EarthGravity;

    Array<Handle<RigidBody>> m_rigidBodies;
};

template <class Adapter>
class ENGINE_API TPhysicsWorld : public PhysicsWorldBase
{
public:
    TPhysicsWorld()
        : PhysicsWorldBase(),
          m_adapter(),
          m_isInitialized(false)
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

        if (m_rigidBodies.Contains(rigidBody))
        {
            return;
        }

        m_rigidBodies.PushBack(rigidBody);
        m_adapter.OnRigidBodyAdded(rigidBody);
    }

    void RemoveRigidBody(const Handle<RigidBody>& rigidBody) override
    {
        if (!rigidBody)
        {
            return;
        }

        auto it = m_rigidBodies.Find(rigidBody);

        if (it == m_rigidBodies.End())
        {
            return;
        }

        m_adapter.OnRigidBodyRemoved(rigidBody);
        
        m_rigidBodies.Erase(it);
    }

    void AddCharacterController(const CharacterControllerConfig& config, SharedPtr<void>& outPhysicsHandle) override
    {
        m_adapter.OnCharacterControllerAdded(config, outPhysicsHandle);
    }

    void RemoveCharacterController(SharedPtr<void>& physicsHandle) override
    {
        m_adapter.OnCharacterControllerRemoved(physicsHandle);
    }

    void SetCharacterWalkDirection(const SharedPtr<void>& physicsHandle, const Vec3f& velocity) override
    {
        m_adapter.SetCharacterWalkDirection(physicsHandle, velocity);
    }

    void ApplyCharacterJump(const SharedPtr<void>& physicsHandle) override
    {
        m_adapter.ApplyCharacterJump(physicsHandle);
    }

    void GetCharacterState(const SharedPtr<void>& physicsHandle, Vec3f& outTranslation, bool& outIsOnGround) override
    {
        m_adapter.GetCharacterState(physicsHandle, outTranslation, outIsOnGround);
    }

    void Initialize() override
    {
        if (m_isInitialized)
        {
            return;
        }

        m_adapter.Init(this);

        m_isInitialized = true;
    }

    void Teardown() override
    {
        if (!m_isInitialized)
        {
            return;
        }

        m_adapter.Teardown(this);

        m_isInitialized = false;
    }

    void Tick(double delta) override
    {
        m_adapter.Tick(this, delta);
    }

private:
    Adapter m_adapter;
    bool m_isInitialized;
};

} // namespace Hyperion

#if defined(HYP_BULLET) && HYP_BULLET

#include <Physics/Bullet/Adapter.hpp>

namespace Hyperion {
using PhysicsWorld = TPhysicsWorld<BulletPhysicsAdapter>;
} // namespace Hyperion

#else // !HYP_BULLET_PHYSICS

#include <Physics/Null/Adapter.hpp>

namespace Hyperion {
using PhysicsWorld = TPhysicsWorld<NullPhysicsAdapter>;
} // namespace Hyperion

#endif // HYP_BULLET_PHYSICS

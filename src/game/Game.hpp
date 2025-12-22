/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class UISubsystem;
class World;
class Scene;
class Event;

HYP_CLASS()
class HYP_API Game : public ObjectBase
{
    friend class SimThread;
    friend class EngineDriver;

    HYP_OBJECT_BODY(Game);

public:
    Game();
    virtual ~Game();

    HYP_METHOD(Property = "World")
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD(Scriptable)
    virtual void OnLaunch() final;

    HYP_METHOD(Scriptable)
    virtual void OnUpdate(float delta) final;

    void HandleEvent(Event&& event);

    HYP_METHOD(Property = "IsLaunched", Transient)
    bool IsLaunched() const
    {
        return m_isLaunched.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FIELD()
    ScriptableDelegate<void> OnLaunched;

protected:
    void Init() override final;

    virtual void Logic(float delta)
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void OnInputEvent(const Event& event);

    HYP_METHOD()
    virtual void OnLaunch_Impl()
    {
    }

    HYP_METHOD()
    virtual void OnUpdate_Impl(float delta)
    {
    }

    const Handle<UISubsystem>& GetUISubsystem() const
    {
        return m_uiSubsystem;
    }

    HYP_FIELD(Property = "World", Serialize)
    Handle<World> m_world;

    Handle<UISubsystem> m_uiSubsystem;

    AtomicVar<bool> m_isLaunched;
};

} // namespace hyperion

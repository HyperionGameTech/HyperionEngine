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

namespace sys {
class SystemEvent;
} // namespace sys

using sys::SystemEvent;

HYP_CLASS()
class HYP_API Game : public ObjectBase
{
    friend class GameThread;
    friend class EngineDriver;

    HYP_OBJECT_BODY(Game);

public:
    Game();
    virtual ~Game();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD(Scriptable)
    virtual void OnLaunch() final;

    HYP_METHOD(Scriptable)
    virtual void OnUpdate(float delta) final;

    HYP_METHOD()
    bool IsLaunched() const
    {
        return m_isLaunched.Get(MemoryOrder::ACQUIRE);
    }

    void HandleEvent(SystemEvent&& event);
    
    HYP_FIELD()
    ScriptableDelegate<void> OnLaunched;

protected:
    void Init() override final;

    virtual void Logic(float delta)
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void OnInputEvent(const SystemEvent& event);

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

    Handle<UISubsystem> m_uiSubsystem;
    Handle<World> m_world;
    AtomicVar<bool> m_isLaunched;
};

} // namespace hyperion

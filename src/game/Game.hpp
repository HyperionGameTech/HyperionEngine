/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include "core/debug/Debug.hpp"
#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class UISubsystem;
class World;
class Scene;

namespace sys {
class SystemEvent;
} // namespace sys

using sys::SystemEvent;

HYP_CLASS(Abstract)
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

protected:
    void Init() override final;

    virtual void Logic(float delta)
    {
        HYP_PURE_VIRTUAL();
    }

    virtual void OnInputEvent(const SystemEvent& event);

    virtual void OnLaunch_Impl()
    {
        // must be implemented by derived class
        HYP_PURE_VIRTUAL();
    }

    virtual void OnUpdate_Impl(float delta)
    {
        // must be implemented by derived class
        HYP_PURE_VIRTUAL();
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

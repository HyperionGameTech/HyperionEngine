/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <engine/GameState.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

class UISubsystem;
class World;
class Scene;
class Event;

HYP_CLASS()
class HYP_API Game : public ObjectBase
{
    friend class SimThread;
    friend class EngineDriver;
    friend struct LaunchGameAsync;

    HYP_OBJECT_BODY(Game);

public:
    Game();
    virtual ~Game();

    HYP_METHOD(Property = "World")
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD(Property = "GameState")
    const GameState& GetGameState() const
    {
        return m_gameState;
    }

    HYP_METHOD(Scriptable)
    virtual void OnLaunch() final;

    HYP_METHOD(Scriptable)
    virtual void OnUpdate(float delta) final;

    HYP_METHOD()
    void StartSimulating();

    HYP_METHOD()
    void StopSimulating();

    HYP_METHOD()
    void PauseSimulation();

#if HYP_EDITOR
    HYP_METHOD(EditorOnly)
    void SetToEditMode();
#endif

    void HandleEvent(Event&& event);

    HYP_METHOD(Property = "IsLaunched", Transient)
    bool IsLaunched() const
    {
        return m_isLaunched.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FIELD()
    ScriptableDelegate<void> OnLaunched;

    HYP_FIELD()
    ScriptableDelegate<void, Game*, GameStateMode, GameStateMode> OnGameStateChange;

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

    HYP_FIELD(Property = "GameState", Transient)
    GameState m_gameState;

    Handle<UISubsystem> m_uiSubsystem;

    AtomicVar<bool> m_isLaunched;
};

} // namespace Hyperion

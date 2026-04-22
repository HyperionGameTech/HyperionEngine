/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <engine/GameState.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/Defines.hpp>
#include <Core/Util.hpp>

namespace Hyperion {

class AssetRegistry;
class UISubsystem;
class World;
class Scene;
class Event;

HYP_CLASS()
class HYP_API Game : public ObjectBase
{
    friend class SimThread;
    friend class EngineDriver;
    friend class EditorProject;
    friend struct LaunchGameAsync;

    HYP_OBJECT_BODY(Game);

public:
    Game();
    virtual ~Game();

    HYP_METHOD(Property = "AssetRegistry", Transient)
    const Handle<AssetRegistry>& GetAssetRegistry() const
    {
        return m_assetRegistry;
    }

    HYP_METHOD(Property = "AssetRegistry", Transient)
    void SetAssetRegistry(const Handle<AssetRegistry>& assetRegistry);

    HYP_METHOD(Property = "World")
    const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD(Property = "World")
    void SetWorld(const Handle<World>& world);

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
    void Initialize();

    HYP_METHOD()
    void Shutdown();

#if HYP_EDITOR
    HYP_METHOD(EditorOnly)
    void SetToEditMode();
#else
    void SetToEditMode() { }
#endif

    void HandleEvent(Event&& event);

    HYP_METHOD(Property = "IsLaunched", Transient)
    bool IsLaunched() const
    {
        return m_isLaunched.Get(MemoryOrder::ACQUIRE);
    }

    HYP_METHOD()
    void StartSimulating();

    HYP_METHOD()
    void StopSimulating();

    HYP_METHOD()
    void PauseSimulation();

    HYP_FIELD()
    ScriptableDelegate<void> OnLaunched;

    HYP_FIELD()
    ScriptableDelegate<void, Game*, GameStateMode, GameStateMode> OnGameStateChange;

protected:
    virtual void Logic(float delta)
    {
        HYP_PURE_VIRTUAL();
    }

    virtual bool OnInputEvent(const Event& event);

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

    Handle<AssetRegistry> m_assetRegistry;
    Handle<UISubsystem> m_uiSubsystem;

    bool m_assetRegistryActive;
    bool m_isInitialized;

    AtomicVar<bool> m_isLaunched;
};

} // namespace Hyperion

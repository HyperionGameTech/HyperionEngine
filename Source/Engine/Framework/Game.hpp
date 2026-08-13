/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Framework/GameState.hpp>
#include <Framework/Content/ContentSync.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/Task.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Defines.hpp>
#include <Core/Util.hpp>

#include <Scripting/ScriptableDelegate.hpp>

namespace Hyperion {

class AssetRegistry;
class UISubsystem;
class World;
class Scene;
class Event;
class InputHandlerBase;

HYP_CLASS()
class ENGINE_API Game : public ObjectBase
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

    HYP_METHOD()
    void Initialize();

    HYP_METHOD()
    void Shutdown(bool shutdownWorld = true);

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

    HYP_METHOD()
    void RegisterInputHandler(const Handle<InputHandlerBase>& inputHandler);

    HYP_METHOD()
    void UnregisterInputHandler(InputHandlerBase* inputHandler);

    HYP_METHOD()
    virtual Handle<World> LoadWorld(Name worldName);

    /// -

    HYP_FIELD()
    static ScriptableDelegate<void> OnLaunched;

    HYP_FIELD()
    static ScriptableDelegate<void, Game*, GameStateMode, GameStateMode> OnGameStateChange;

protected:
    const Handle<UISubsystem>& GetUISubsystem() const
    {
        return m_uiSubsystem;
    }

    bool IsSyncingOrPreparingContent() const
    {
        return m_syncState.IsInProgress();
    }
    
    void SyncContentAndLaunch();
    
    virtual void BeforeContentLoaded();
    virtual void AfterContentLoaded();

    virtual void OnSyncProgress(uint64 current, uint64 total);

    virtual void OnLaunch();
    virtual void OnUpdate(float delta);

    virtual void BeforeShutdown();

    virtual bool OnInputEvent(const Event& event);

    HYP_FIELD(Property = "World", Transient)
    Handle<World> m_world;

    HYP_FIELD(Property = "GameState", Transient)
    GameState m_gameState;

    Handle<AssetRegistry> m_assetRegistry;
    Handle<UISubsystem> m_uiSubsystem;

    Array<Handle<InputHandlerBase>> m_inputHandlers;

    ContentSyncState m_syncState;

    bool m_assetRegistryActive;
    bool m_isInitialized;

    AtomicVar<bool> m_isLaunched;

private:
    void Launch();
};

} // namespace Hyperion

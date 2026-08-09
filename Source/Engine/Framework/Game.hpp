/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Framework/GameState.hpp>

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

    HYP_METHOD(Property = "PackageName")
    Name GetPackageName() const
    {
        return m_packageName;
    }

    HYP_METHOD(Property = "PackageName")
    void SetPackageName(Name packageName)
    {
        m_packageName = packageName;
    }

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
    void OnUpdate(float delta);

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

    /// Danger zone: Only for internal usage

    HYP_METHOD(Scriptable)
    void OnLaunch();

    HYP_METHOD(Scriptable)
    void BeforeShutdown();

    /// -

    HYP_FIELD()
    static ScriptableDelegate<void> OnLaunched;

    HYP_FIELD()
    static ScriptableDelegate<void, Game*, GameStateMode, GameStateMode> OnGameStateChange;

protected:
    bool IsSyncingContent() const
    {
        return m_syncContentTask.IsValid();
    }

    void AfterContentLoaded();

    virtual void InitializeWorld();

    virtual bool OnInputEvent(const Event& event);

    HYP_METHOD()
    virtual void OnLaunch_Impl()
    {
    }

    HYP_METHOD()
    virtual void BeforeShutdown_Impl()
    {
    }

    HYP_METHOD()
    virtual void OnUpdate_Impl(float delta);

    const Handle<UISubsystem>& GetUISubsystem() const
    {
        return m_uiSubsystem;
    }

    HYP_FIELD(Property = "PackageName", Serialize)
    Name m_packageName;

    HYP_FIELD(Property = "World", Serialize)
    Handle<World> m_world;

    HYP_FIELD(Property = "GameState", Transient)
    GameState m_gameState;

    Handle<AssetRegistry> m_assetRegistry;
    Handle<UISubsystem> m_uiSubsystem;

    Array<Handle<InputHandlerBase>> m_inputHandlers;

    Task<Result> m_syncContentTask;

    bool m_assetRegistryActive;
    bool m_isInitialized;

    AtomicVar<bool> m_isLaunched;
};

} // namespace Hyperion

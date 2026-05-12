/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <engine/Game.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <rendering/DebugDrawer.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/debug/Debug.hpp>

#include <scene/World.hpp>

#include <scene/camera/Camera.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <dotnet/DotNETHost.hpp>
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/Assembly.hpp>

#include <scripting/ScriptingService.hpp>

#include <input/Event.hpp>

#include <scene/input/TouchControlsSubsystem.hpp>

#include <Game.generated.inl>

namespace Hyperion {

static const Name s_nameMainWorld = NAME("World");

HYP_API extern const FilePath& GetLibraryDirectory();

Game::Game()
    : m_isInitialized(false),
      m_assetRegistryActive(false),
      m_isLaunched(false)
{
}

Game::~Game()
{
    if (m_assetRegistryActive)
    {
        PopAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = false;
    }

    if (m_assetRegistry)
    {
        m_assetRegistry->Shutdown();
    }
}

void Game::Initialize()
{
    AssertOnThread(g_simThread);

    if (m_isInitialized)
    {
        return;
    }

    if (!m_assetRegistry)
    {
        m_assetRegistry = MakeHandle<AssetRegistry>(
            AssetRegistryId::Game,
            GetLibraryDirectory() / *InstanceClass()->GetName());
    }

    if (!m_assetRegistryActive)
    {
        m_assetRegistry->Initialize();

        PushAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = true;
    }

    Assert(m_world != nullptr);

    AssertDebug(m_world->m_gameInstance == nullptr || m_world->m_gameInstance == this);
    m_world->m_gameInstance = this;
    InitObject(m_world);

    m_assetRegistry->PutAssetsDeep(m_world);

    if (!m_uiSubsystem)
    {
        m_uiSubsystem = m_world->AddSubsystem(MakeHandle<UISubsystem>());
    }

    m_isInitialized = true;
}

void Game::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

    if (m_world)
    {
        m_world->m_gameInstance = nullptr;

        g_engineDriver->RemoveWorld(m_world);
    }

    if (m_assetRegistry && m_assetRegistryActive)
    {
        PopAssetRegistry(m_assetRegistry);

        m_assetRegistryActive = false;
        m_assetRegistry->Shutdown();
    }

    m_isInitialized = false;
}

void Game::SetAssetRegistry(const Handle<AssetRegistry>& assetRegistry)
{
    if (m_assetRegistry == assetRegistry)
    {
        return;
    }

    if (m_assetRegistryActive)
    {
        PopAssetRegistry(m_assetRegistry);

        m_assetRegistryActive = false;
    }

    if (m_assetRegistry)
    {
        m_assetRegistry->Shutdown();
    }

    m_assetRegistry = assetRegistry;

    if (m_assetRegistry && m_isInitialized)
    {
        m_assetRegistry->Initialize();

        PushAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = true;
    }
}

void Game::SetWorld(const Handle<World>& world)
{
    if (m_world == world)
    {
        return;
    }

    const bool isLaunched = IsLaunched();

    if (m_world)
    {
        m_world->m_gameInstance = nullptr;

        if (isLaunched)
        {
            g_engineDriver->RemoveWorld(m_world);
        }
    }

    m_world = world;

    if (m_world && isLaunched)
    {
        AssertDebug(m_world->m_gameInstance == nullptr || m_world->m_gameInstance == this);
        m_world->m_gameInstance = this;

        g_engineDriver->AddWorld(m_world);
    }
}

void Game::HandleEvent(Event&& event)
{
    AssertOnThread(g_simThread);

    // Pass touch events to TouchControlsSubsystem if available
    if (event.GetType() == EventType::TOUCH_DOWN ||
        event.GetType() == EventType::TOUCH_UP ||
        event.GetType() == EventType::TOUCH_MOVE)
    {
        if (m_world != nullptr)
        {
            TouchControlsSubsystem* touchControls = m_world->GetSubsystem<TouchControlsSubsystem>();
            if (touchControls != nullptr)
            {
                TouchEvent touchEvent = event.ToTouchEvent();
                touchControls->ProcessTouchEvent(touchEvent);
            }
        }
    }

    OnInputEvent(event);
}

bool Game::OnInputEvent(const Event& event)
{
    AssertOnThread(g_simThread);

    if (m_uiSubsystem.IsValid())
    {
        if (m_uiSubsystem->GetUIStage()->OnInputEvent(event))
        {
            return true;
        }
    }

    return false;
}

void Game::StartSimulating()
{
    if (m_gameState.mode == GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    if (previousGameStateMode != GameStateMode::PAUSED)
    {
        m_gameState.gameTime = 0.0f;
        m_gameState.deltaTime = 0.0f;
    }

    m_gameState.mode = GameStateMode::SIMULATING;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::SIMULATING);
}

void Game::StopSimulating()
{
    const GameStateMode previousGameStateMode = m_gameState.mode;

    if (previousGameStateMode == GameStateMode::STOPPED)
    {
        return;
    }

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::STOPPED;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::STOPPED);
}

void Game::PauseSimulation()
{
    if (m_gameState.mode != GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.mode = GameStateMode::PAUSED;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::PAUSED);
}

#if HYP_EDITOR

void Game::SetToEditMode()
{
    if (m_gameState.mode == GameStateMode::EDIT_MODE)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::EDIT_MODE;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::EDIT_MODE);

    HYP_LOG(Engine, Verbose, "Game set to Edit Mode");
}

#endif

} // namespace Hyperion

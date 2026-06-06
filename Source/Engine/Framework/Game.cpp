/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>
#include <Rendering/DebugDrawer.hpp>
#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Debug/Debug.hpp>

#include <Scene/World.hpp>

#include <Scene/Camera/Camera.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIStage.hpp>

#include <DotNET/DotNETHost.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedObject.hpp>
#include <DotNET/Assembly.hpp>

#include <Scripting/ScriptingService.hpp>

#include <Input/Event.hpp>

#include <Scene/Input/TouchControlsSubsystem.hpp>

#include <Game.generated.inl>

namespace Hyperion {

static const Name s_nameMainWorld = NAME("World");

ENGINE_API extern const FilePath& GetLibraryDirectory();

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

    m_world->Initialize();

    m_assetRegistry->PutAssetsDeep(m_world);

    if (!m_uiSubsystem)
    {
        m_uiSubsystem = m_world->AddSubsystem(MakeHandle<UISubsystem>());
    }

    m_isInitialized = true;
}

void Game::Shutdown(bool shutdownWorld)
{
    if (!m_isInitialized)
    {
        return;
    }

    if (m_world)
    {
        if (m_uiSubsystem.IsValid())
        {
            m_world->RemoveSubsystem(m_uiSubsystem);
            m_uiSubsystem.Reset();
        }

        m_world->m_gameInstance = nullptr;

        g_engineDriver->RemoveWorld(m_world);
        
        if (shutdownWorld)
        {
            m_world->Shutdown();
        }
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

        if (m_isInitialized)
        {
            if (isLaunched)
            {
                g_engineDriver->RemoveWorld(m_world);
            }
            
            m_world->Shutdown();
        }
    }

    m_world = world;

    if (m_world)
    {
        AssertDebug(m_world->m_gameInstance == nullptr || m_world->m_gameInstance == this);
        m_world->m_gameInstance = this;

        if (m_isInitialized)
        {
            m_world->Initialize();

            if (isLaunched)
            {
                g_engineDriver->AddWorld(m_world);
            }
        }
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

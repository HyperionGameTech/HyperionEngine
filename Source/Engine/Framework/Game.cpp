/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/CacheClient.hpp>

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

#include <System/MessageBox.hpp>

#include <Game.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DEFINE_LOG_CHANNEL(Game);

ScriptableDelegate<void> Game::OnLaunched;
ScriptableDelegate<void, Game*, GameStateMode, GameStateMode> Game::OnGameStateChange;

static const Name s_nameMainWorld = NAME("MainWorld");
static const Name s_nameTempUIWorld = NAME("TempUIWorld");

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

        if (m_syncContentTask.IsValid())
        {
            m_syncContentTask.Await();
            m_syncContentTask = {};
        }
    }

    OnLaunched.RemoveAllForTarget(this);
    OnGameStateChange.RemoveAllForTarget(this);
}

void Game::Initialize()
{
    AssertOnThread(g_simThread);

    if (m_isInitialized || IsSyncingContent())
    {
        return;
    }

    if (!m_assetRegistry)
    {
        m_assetRegistry = MakeHandle<AssetRegistry>(
            AssetRegistryId::Game,
            EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Game")>());
    }

    if (!m_assetRegistryActive)
    {
        SyncContentAndLaunch();
    }
    else
    {
        Launch();
    }
}

Handle<World> Game::LoadWorld_Impl(Name worldName)
{
    return m_assetRegistry->GetAsset<World>(AssetBuckets::Worlds, worldName);
}

void Game::Shutdown(bool shutdownWorld)
{
    if (m_syncContentTask.IsValid())
    {
        m_syncContentTask.Await();
        m_syncContentTask = {};
    }

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

        g_engineDriver->RemoveWorld(m_world);

        if (shutdownWorld)
        {
            m_world->Shutdown();
        }

        m_world->m_gameInstance = nullptr;
    }

    if (m_assetRegistry && m_assetRegistryActive)
    {
        PopAssetRegistry(m_assetRegistry);

        m_assetRegistryActive = false;
        m_assetRegistry->Shutdown();
    }

    m_isInitialized = false;
    m_isLaunched.Set(false, MemoryOrder::RELEASE);
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

        if (m_syncContentTask.IsValid())
        {
            m_syncContentTask.Await();
            m_syncContentTask = {};
        }
    }

    m_assetRegistry = assetRegistry;

    if (m_assetRegistry && m_isInitialized)
    {
        m_assetRegistry->Initialize(&m_syncContentTask);

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
    
    if (m_syncContentTask.IsValid())
    {
        m_syncContentTask.Await();
        m_syncContentTask = {};
    }

    const bool isLaunched = IsLaunched();

    if (m_world)
    {
        if (m_uiSubsystem.IsValid())
        {
            m_world->RemoveSubsystem(m_uiSubsystem);
            m_uiSubsystem.Reset();
        }

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
        
        m_uiSubsystem = m_world->AddSubsystem(MakeHandle<UISubsystem>());

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

    OnInputEvent(event);
}

bool Game::OnInputEvent(const Event& event)
{
    AssertOnThread(g_simThread);

    if (m_uiSubsystem.IsValid() && m_uiSubsystem->GetUIStage().IsValid())
    {
        if (m_uiSubsystem->GetUIStage()->OnInputEvent(event) == UIEventHandlerResult::STOP_BUBBLING)
        {
            return true;
        }
    }

    if (event.GetType() == EventType::TOUCH_DOWN
        || event.GetType() == EventType::TOUCH_UP
        || event.GetType() == EventType::TOUCH_MOVE)
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

    if (m_inputHandlers.Any())
    {
        switch (event.GetType())
        {
        case EventType::KEYUP:
        {
            KeyboardEvent kbe = event.ToKeyboardEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnKeyUp(kbe))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::KEYDOWN:
        {
            KeyboardEvent kbe = event.ToKeyboardEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnKeyDown(kbe))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::MOUSEBUTTON_DOWN:
        {
            MouseEvent me = event.ToMouseEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnMouseDown(me))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::MOUSEBUTTON_UP:
        {
            MouseEvent me = event.ToMouseEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnMouseUp(me))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::MOUSEMOTION:
        {
            MouseEvent me = event.ToMouseEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnMouseMove(me))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::TOUCH_DOWN:
        {
            TouchEvent te = event.ToTouchEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnTouchDown(te))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::TOUCH_UP:
        {
            TouchEvent te = event.ToTouchEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnTouchUp(te))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::TOUCH_MOVE:
        {
            TouchEvent te = event.ToTouchEvent();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnTouchMove(te))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::CONTROLLER_BUTTON_DOWN:
        {
            ControllerButton btn = event.GetControllerButton();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnControllerButtonDown(btn))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::CONTROLLER_BUTTON_UP:
        {
            ControllerButton btn = event.GetControllerButton();

            for (size_t i = m_inputHandlers.Size(); i != 0; i--)
            {
                InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                if (inputHandler->OnControllerButtonUp(btn))
                {
                    return true;
                }
            }
            
            break;
        }
        case EventType::CONTROLLER_ANALOG_MOVE:
        {
            const ControllerAnalogData* analogData = event.GetControllerAnalogData();

            if (analogData)
            {
                for (size_t i = m_inputHandlers.Size(); i != 0; i--)
                {
                    InputHandlerBase* inputHandler = m_inputHandlers[i - 1];

                    if (inputHandler->OnControllerAnalogMove(*analogData))
                    {
                        return true;
                    }
                }
            }

            break;
        }
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

    OnGameStateChange.Fire(this, this, previousGameStateMode, GameStateMode::SIMULATING);
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

    OnGameStateChange.Fire(this, this, previousGameStateMode, GameStateMode::STOPPED);
}

void Game::PauseSimulation()
{
    if (m_gameState.mode != GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.mode = GameStateMode::PAUSED;

    OnGameStateChange.Fire(this, this, previousGameStateMode, GameStateMode::PAUSED);
}

//#ifdef HYP_EDITOR
//
//void Game::SetToEditMode()
//{
//    if (m_gameState.mode == GameStateMode::EDIT_MODE)
//    {
//        return;
//    }
//
//    const GameStateMode previousGameStateMode = m_gameState.mode;
//
//    m_gameState.gameTime = 0.0f;
//    m_gameState.deltaTime = 0.0f;
//    m_gameState.mode = GameStateMode::EDIT_MODE;
//
//    OnGameStateChange.Fire(this, this, previousGameStateMode, GameStateMode::EDIT_MODE);
//
//    HYP_LOG(Engine, Verbose, "Game set to Edit Mode");
//}
//
//#endif

void Game::RegisterInputHandler(const Handle<InputHandlerBase>& inputHandler)
{
    AssertOnThread(g_simThread);

    if (!inputHandler.IsValid())
    {
        return;
    }

    auto it = m_inputHandlers.Find(inputHandler);

    if (it != m_inputHandlers.End())
    {
        return;
    }
    
    m_inputHandlers.PushBack(inputHandler);
}

void Game::UnregisterInputHandler(InputHandlerBase* inputHandler)
{
    AssertOnThread(g_simThread);

    if (!inputHandler)
    {
        return;
    }
    
    auto it = m_inputHandlers.FindAs(inputHandler);

    if (it == m_inputHandlers.End())
    {
        return;
    }

    m_inputHandlers.Erase(it);
}

void Game::OnUpdate_Impl(float delta)
{
    AssertOnThread(g_simThread);

    if (m_syncContentTask.IsValid())
    {
        if (m_syncContentTask.IsCompleted())
        {
            Result res = m_syncContentTask.Await();
            m_syncContentTask = {};

            if (res.HasError())
            {
                bool clickedRetry = false;
                bool clickedExit = false;

                CacheClient::SyncFailed(res.GetError(), clickedRetry, clickedExit);

                if (clickedExit)
                {
                    std::terminate();
                    return;
                }

                if (clickedRetry)
                {
                    m_assetRegistry->Initialize(&m_syncContentTask);

                    if (!m_syncContentTask.IsValid())
                    {
                        AfterContentLoaded();
                    }
                }
            }
            else
            {
                AfterContentLoaded();
            }
        }
    }
}

void Game::BeforeContentLoaded()
{
    AssertOnThread(g_simThread);

    Handle<World> tempWorld = MakeHandle<World>();
    tempWorld->SetName(s_nameTempUIWorld);
    tempWorld->SetIsTransient(true);

    SetWorld(tempWorld);
}

void Game::AfterContentLoaded()
{
    AssertOnThread(g_simThread);

    Assert(m_isLaunched.Get(MemoryOrder::ACQUIRE) == false);

    SetWorld(Handle<World>::Null());
    
    if (Handle<World> world = LoadWorld(s_nameMainWorld); world.IsValid())
    {
        SetWorld(world);
    }
    else
    {
        auto setToDummyWorld = [&]
        {
            SetWorld(MakeHandle<World>());
        };

        bool shouldReturn = false;

        // clang-format off
        SystemMessageBox(MessageBoxType::CRITICAL)
            .Title("Error")
            .Text("Failed to load game content! The world could not be initialized.\n\n"
                "Please make sure the game is up to date, or try reinstalling it.\n"
                "Please file a bug report! Apologies for the inconvenience.")
            .Button("Retry", [this, &shouldReturn] { shouldReturn = true; SyncContentAndLaunch(); })
            .Button("Launch Anyway", &setToDummyWorld)
            .Button("Exit", &std::terminate)
            .Show();
        // clang-format on

        if (shouldReturn)
        {
            return;
        }
    }

    Launch();
}

void Game::SyncContentAndLaunch()
{
    AssertOnThread(g_simThread);

    // This check exists mainly for initializing newly created editor projects that
    // have a World set on them and should not destroy that world to attempt to load one from disk.
    const bool canSyncContent = m_assetRegistry->GetRootPath().Exists();

    if (canSyncContent)
    {
        BeforeContentLoaded();

        m_assetRegistry->Initialize(&m_syncContentTask);

        PushAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = true;

        if (!m_syncContentTask.IsValid())
        {
            AfterContentLoaded();
        }
    }
    else
    {
        m_assetRegistry->Initialize(nullptr);

        PushAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = true;

        Launch();
    }
}

void Game::Launch()
{
    AssertOnThread(g_simThread);

    Assert(m_isLaunched.Get(MemoryOrder::ACQUIRE) == false);
    Assert(m_world.IsValid());

    // Add to global worlds
    g_engineDriver->AddWorld(m_world);
    
    OnLaunch();
    m_isLaunched.Set(true, MemoryOrder::RELEASE);
        
    // Invoke callbacks
    Game::OnLaunched.Fire(this);

    m_isInitialized = true;
    m_syncContentTask = {};
}

} // namespace Hyperion

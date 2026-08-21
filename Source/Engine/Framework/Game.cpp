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

#include <Framework/Client/GameClient.hpp>

#include <Rendering/DebugDrawer.hpp>
#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/TaskSystem.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Core.hpp>

#include <Scene/World.hpp>
#include <Scene/View.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Systems/PlayerSystem.hpp>
#include <Scene/Systems/CharacterControllerSystem.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/Input/TouchControlsSubsystem.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIStage.hpp>

#include <DotNET/DotNETHost.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedObject.hpp>
#include <DotNET/Assembly.hpp>

#include <Scripting/ScriptingService.hpp>

#include <Input/Event.hpp>

#include <System/MessageBox.hpp>

///--- For shader preload ---
#include <Rendering/RenderInterface.hpp>
#include <Rendering/ShaderManager.hpp>
///--------------------------

#include <Game.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DEFINE_LOG_CHANNEL(Game);

ScriptableDelegate<void> Game::OnLaunched;
ScriptableDelegate<void, Game*, GameStateMode, GameStateMode> Game::OnGameStateChange;

const Name Game::s_nameMainWorld = NAME("MainWorld");
const Name Game::s_nameTempUIWorld = NAME("TempUIWorld");

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

        m_syncState.Wait();
    }

    OnLaunched.RemoveAllForTarget(this);
    OnGameStateChange.RemoveAllForTarget(this);
}

void Game::Initialize()
{
    AssertOnThread(g_simThread);

    if (m_isInitialized || IsSyncingOrPreparingContent() || m_connectionState.state == ServerConnectionState::Connecting)
    {
        return;
    }

    if (!m_assetRegistry)
    {
        m_assetRegistry = MakeHandle<AssetRegistry>(
            AssetRegistryId::Game,
            EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Game")>());
    }

    if (g_gameClient != nullptr && !EngineGlobals::IsEditor() && m_connectionState.state != ServerConnectionState::Connected)
    {
        BeforeConnectingToServer();

        if (CoreApi::GetCommandLineArguments()["autoconnect"].ToBool())
        {
            if (const char* hostAddress = EngineGlobals::GetHostAddress(); hostAddress != nullptr && *hostAddress != '\0')
            {
                ConnectToServer(hostAddress);
            }
        }

        return;
    }

    SyncContentAndLaunch();
}

void Game::ConnectToServer(const ANSIString& hostAddress)
{
    AssertOnThread(g_simThread);

    if (g_gameClient == nullptr || m_connectionState.state == ServerConnectionState::Connecting)
    {
        return;
    }

    m_connectionState.lastResult = {};
    m_connectionState.SetState(ServerConnectionState::Connecting);

    if (Result connectResult = g_gameClient->Connect(hostAddress, EngineGlobals::GetGameServerPort()); connectResult.HasError())
    {
        m_connectionState.lastResult = connectResult;
        m_connectionState.SetState(ServerConnectionState::Failed);
    }
}

Handle<World> Game::LoadWorld(Name worldName)
{
    return m_assetRegistry->GetAsset<World>(AssetBuckets::Worlds, worldName);
}

void Game::Shutdown(bool shutdownWorld)
{
    m_syncState.Wait();

    if (!m_isInitialized)
    {
        return;
    }

    BeforeShutdown();

    if (m_world)
    {
        if (m_uiSubsystem.IsValid())
        {
            m_world->RemoveSubsystem(m_uiSubsystem);
            m_uiSubsystem.Reset();
        }

        if (shutdownWorld)
        {
            m_world->Shutdown();
        }

        m_world->SetGame(nullptr);

        g_engineDriver->RemoveWorld(m_world, shutdownWorld);
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

        m_syncState.Wait();
    }

    m_assetRegistry = assetRegistry;

    if (m_assetRegistry && m_isInitialized)
    {
        m_syncState.currentTask = {};
        m_syncState.progress.Set(0, MemoryOrder::RELAXED);
        m_syncState.SetState(ContentSyncState::NotStarted);

        m_assetRegistry->Initialize(
            &m_syncState.currentTask,
            ProcRef<void(uint64, uint64)>(*this, ValueWrapper<&Game::OnSyncProgress>{}));

        PushAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = true;

        if (m_syncState.IsInProgress())
        {
            m_syncState.SetState(ContentSyncState::InProgress);
        }
        else
        {
            m_syncState.SetState(ContentSyncState::Finished);
        }
    }
}

void Game::SetWorld(const Handle<World>& world)
{
    if (m_world == world)
    {
        return;
    }
    
    m_syncState.Wait();

    const bool isLaunched = IsLaunched();

    if (m_world)
    {
        if (m_uiSubsystem.IsValid())
        {
            m_world->RemoveSubsystem(m_uiSubsystem);
            m_uiSubsystem.Reset();
        }

        m_world->Shutdown();

        m_world->SetGame(nullptr);

        g_engineDriver->RemoveWorld(m_world);
    }

    m_world = world;

    if (m_world)
    {
        AssertDebug(m_world->GetGame() == nullptr || m_world->GetGame() == this);
        m_world->SetGame(this);
        
        if (!EngineGlobals::IsHeadless())
        {
            m_uiSubsystem = m_world->AddSubsystem(MakeHandle<UISubsystem>());
        }

        m_world->Initialize();

        g_engineDriver->AddWorld(m_world);
    }
}

void Game::Launch()
{
    AssertOnThread(g_simThread);

    Assert(m_isLaunched.Get(MemoryOrder::ACQUIRE) == false);
    Assert(m_world.IsValid());

    Assert(!IsSyncingOrPreparingContent());
    
    OnLaunch();
    m_isLaunched.Set(true, MemoryOrder::RELEASE);
        
    // Invoke callbacks
    Game::OnLaunched.Fire(this);

    m_isInitialized = true;
}

void Game::SyncContentAndLaunch()
{
    AssertOnThread(g_simThread);
    
    Assert(!IsSyncingOrPreparingContent());
    Assert(m_assetRegistry.IsValid());

    const bool shouldSyncContent =
        // Don't want to skip syncing just because we have the loader UI up (temp world)
        (!m_world.IsValid() || m_world->IsTransient())
        // For editor; don't want to try to sync a brand new, unsaved project.
        && m_assetRegistry->GetRootPath().Exists();

    if (shouldSyncContent)
    {
        BeforeContentLoaded();
        
        m_syncState.currentTask = {};
        m_syncState.progress.Set(0, MemoryOrder::RELAXED);
        m_syncState.SetState(ContentSyncState::NotStarted);

        m_assetRegistry->Initialize(
            &m_syncState.currentTask,
            ProcRef<void(uint64, uint64)>(*this, ValueWrapper<&Game::OnSyncProgress>{}));

        PushAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = true;

        if (m_syncState.IsInProgress())
        {
            m_syncState.SetState(ContentSyncState::InProgress);
        }
        else
        {
            AfterContentLoaded();
        }
    }
    else
    {
        m_syncState.currentTask = {};
        m_syncState.progress.Set(0, MemoryOrder::RELAXED);
        m_syncState.SetState(ContentSyncState::Finished);

        m_assetRegistry->Initialize(nullptr);

        PushAssetRegistry(m_assetRegistry);
        m_assetRegistryActive = true;

        // Re-register assets that were removed from the AssetRegistry's cache from Shutdown(shutdownWorld = false) call.
        if (m_world.IsValid() && !m_world->IsTransient())
        {
            m_assetRegistry->PutAssetsDeep(m_world);
        }

        Launch();
    }
}

void Game::BeforeContentLoaded()
{
    AssertOnThread(g_simThread);

    Handle<World> tempWorld = MakeHandle<World>();
    tempWorld->SetName(s_nameTempUIWorld);
    tempWorld->SetWorldFlags(tempWorld->GetWorldFlags() & ~WorldFlags::IsReplicated);
    tempWorld->SetIsTransient(true);

    SetWorld(tempWorld);
}

void Game::AfterContentLoaded()
{
    AssertOnThread(g_simThread);

    Assert(m_isLaunched.Get(MemoryOrder::ACQUIRE) == false);
    
    m_syncState.currentTask = {};

    if (Handle<World> world = LoadWorld(s_nameMainWorld); world.IsValid())
    {
        m_syncState.SetState(ContentSyncState::Finished);

        // Systems
        if (!world->HasSystem<CharacterControllerSystem>())
        {
            world->AddSystem(MakeHandle<CharacterControllerSystem>());
        }

        if (!world->HasSystem<PlayerSystem>())
        {
            world->AddSystem(MakeHandle<PlayerSystem>());
        }

        SetWorld(world);
    }
    else
    {
        m_syncState.SetState(ContentSyncState::Failed);
        m_syncState.progress.Set(0, MemoryOrder::RELAXED);

        return;
    }

    Launch();
}

void Game::BeforeConnectingToServer()
{
    AssertOnThread(g_simThread);

    Handle<World> tempWorld = MakeHandle<World>();
    tempWorld->SetWorldFlags(tempWorld->GetWorldFlags() & ~WorldFlags::IsReplicated);
    tempWorld->SetName(s_nameTempUIWorld);
    tempWorld->SetIsTransient(true);

    SetWorld(tempWorld);
}

void Game::AfterConnectedToServer()
{
    AssertOnThread(g_simThread);

    // no-op - BeforeContentLoaded() (called next, via SyncContentAndLaunch()) sets up its own
    // temp world for the loading screen.
}

void Game::OnSyncProgress(uint64 current, uint64 total)
{
    m_syncState.progress.Set(
        total > 0
            ? uint32(10000.0f * (float(current) / float(total)))
            : 0,
        MemoryOrder::RELAXED);
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
    
    m_syncState.Wait();

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
    
    m_syncState.Wait();

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
    
    m_syncState.Wait();

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.mode = GameStateMode::PAUSED;

    OnGameStateChange.Fire(this, this, previousGameStateMode, GameStateMode::PAUSED);
}

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

void Game::OnLaunch()
{
    World* world = GetWorld();
    Assert(world != nullptr);

    if (!world)
    {
        return;
    }
    
    // Add a View to the World to capture from the camera's perspective
    Camera* camera = SceneHelpers::FindMainCamera(*world);

    if (camera != nullptr)
    {
        m_camera = MakeStrongRef(camera);
        m_camera->SetCameraFlags(m_camera->GetCameraFlags() | CameraFlags::MatchWindowSize);

        ViewDesc viewDesc;
        viewDesc.flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS;
        viewDesc.framebufferDesc = { Vec2u(m_camera->GetDimensions()) };
        viewDesc.camera = m_camera;

        Handle<View> view = MakeHandle<View>(viewDesc);
        view->SetName(NAME_FMT("{}_View", m_camera->GetName()));
        
        world->AddView(view);

        m_views.PushBack(view);
    }
}

void Game::OnUpdate(float delta)
{
    AssertOnThread(g_simThread);

    switch (m_connectionState.state)
    {
    case ServerConnectionState::Connecting:
    {
        Assert(g_gameClient != nullptr);

        const NetClientConnectionState clientState = g_gameClient->GetConnectionState();

        if (clientState == NetClientConnectionState::Connected)
        {
            m_connectionState.SetState(ServerConnectionState::Connecting_StartingNextTask);

            // Call next frame.
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [weakThis = MakeWeakRef(this)]()
                {
                    Handle<Game> strongThis = weakThis.Lock();
                    if (!strongThis.IsValid())
                    {
                        return;
                    }
                    
                    strongThis->m_connectionState.SetState(ServerConnectionState::Connected);

                    strongThis->AfterConnectedToServer();
                    strongThis->SyncContentAndLaunch();
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            return;
        }
        
        if (clientState == NetClientConnectionState::Disconnected)
        {
            // Connect() attempt ended (timed out or errored) while we were waiting on it
            m_connectionState.lastResult = g_gameClient->GetLastError();
            m_connectionState.SetState(ServerConnectionState::Failed);
        }

        return;
    }
    case ServerConnectionState::Connected:
        break;
    case ServerConnectionState::Failed: // fallthrough
    case ServerConnectionState::Connecting_StartingNextTask:
        if (m_syncState.IsInProgress())
        {
            if (m_syncState.currentTask.Cancel())
            {
                m_syncState.currentTask = {};
                m_syncState.progress.Set(0, MemoryOrder::RELAXED);
                m_syncState.SetState(ContentSyncState::NotStarted);
            }
        }

        return;
    }

    if (m_syncState.IsInProgress())
    {
        auto callAfterContentLoadedNextFrame = [this]
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [this, weakThis = MakeWeakRef(this)]()
                {
                    Handle<Game> strongThis = weakThis.Lock();
                    if (!strongThis.IsValid())
                    {
                        return;
                    }

                    AfterContentLoaded();
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        };

        auto beginPreparingContent = [this]
        {
            HYP_LOG(Game, Info, "Setting up preparation task...");

            // Set the prepare task
            m_syncState.currentTask = TaskSystem::GetInstance().Enqueue(
                [this]() -> Result
                {
                    // Keeping check in here in case we add preparations that need to be done even in headless

                    if (!EngineGlobals::IsHeadless())
                    {
                        g_shaderManager->PreloadShadersFromCacheFile(
                            /* blockingWait */ true,
                            ProcRef<void(uint64, uint64)>(*this, ValueWrapper<&Game::OnSyncProgress> {}));
                    }

                    return {};
                },
                TaskThreadPoolName::THREAD_POOL_BACKGROUND);

            m_syncState.progress.Set(0, MemoryOrder::RELAXED);
            m_syncState.SetState(ContentSyncState::Downloaded_Preparing);
        };

        if (m_syncState.currentTask.IsCompleted())
        {
            Result res = m_syncState.Wait();

            if (res.HasError())
            {
                m_syncState.currentTask = {};
                m_syncState.progress.Set(0, MemoryOrder::RELAXED);
                m_syncState.SetState(ContentSyncState::Failed);

                return;
            }
            
            if (m_syncState.state == ContentSyncState::InProgress)
            {
                beginPreparingContent();

                return;
            }
            
            if (m_syncState.state == ContentSyncState::Downloaded_Preparing)
            {
                // Done preparing
                callAfterContentLoadedNextFrame();

                return;
            }
        }
    }
}

void Game::BeforeShutdown()
{
    World* world = GetWorld();

    if (world != nullptr)
    {
        for (const Handle<View>& view : m_views)
        {
            world->RemoveView(view.Get());
        }
    }

    m_views.Clear();
}

Handle<Game> Game::CreateGame(StringHash classNameHash)
{
    if (!classNameHash)
    {
        return nullptr;
    }

    const Class* gameClass = GetClass(classNameHash);

    if (!gameClass || !gameClass->IsDerivedFrom(Game::StaticClass()))
    {
        return nullptr;
    }

    BoxedValue boxed;
    if (!gameClass->CreateInstance(boxed))
    {
        return nullptr;
    }

    return boxed.Get<Handle<Game>>();
}

} // namespace Hyperion

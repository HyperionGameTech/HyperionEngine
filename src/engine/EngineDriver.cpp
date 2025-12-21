/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineMemory.hpp>
#include <engine/DebugDrawer.hpp>

#include <engine/threads/GameThread.hpp>
#include <engine/threads/MainThread.hpp>
#include <engine/threads/RenderThread.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Device.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Texture.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Subsystem.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/debug/StackDump.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/reflection/Enum.hpp> // For EnumValue()

#include <core/cli/CommandLine.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <asset/Assets.hpp>

#include <streaming/StreamingManager.hpp>

#include <util/MeshBuilder.hpp>

#include <input/Event.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <scripting/ScriptingService.hpp>

#include <game/Game.hpp>

#include <HyperionEngine.hpp>

#define HYP_PROCESS_VIEWS_ASYNC 1
#define HYP_PROCESS_SUBSYSTEMS_ASYNC 1

#include <EngineDriver.generated.inl>

namespace hyperion {

void HandleSignal(int signum);

extern const GlobalConfig& CoreApi_GetGlobalConfig();
extern FilePath CoreApi_GetExecutablePath();
extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

EngineStatTimer g_renderThreadUpdateTimer("Frame/RenderThreadUpdate");

#pragma region MainThread
#pragma endregion MainThread

#pragma region RenderThread
#pragma endregion RenderThread

void HandleSignal(int signum)
{
#ifdef HYP_WINDOWS
    Win32_CleanupWindowClasses();
#endif

    exit(signum);
}

#pragma region EngineDriver

const Handle<EngineDriver>& EngineDriver::GetInstance()
{
    return g_engineDriver;
}

EngineDriver::EngineDriver()
    : m_currentWorld(nullptr),
      m_viewCollectionBatch(nullptr),
      m_isShuttingDown(0)
{
}

EngineDriver::~EngineDriver()
{
}

HYP_API void EngineDriver::Init()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    // #ifdef HYP_EDITOR
    //     // Create script compilation service
    //     m_scriptingService = MakeUnique<ScriptingService>(
    //         GetResourceDirectory() / "scripts" / "src",
    //         GetResourceDirectory() / "scripts" / "projects",
    //         CoreApi_GetExecutablePath()); // copy script binaries into executable path

    //     m_scriptingService->Start();
    // #endif

    RC<NetRequestThread> netRequestThread = MakeRefCountedPtr<NetRequestThread>();
    SetGlobalNetRequestThread(netRequestThread);
    netRequestThread->Start();

    // must start after net request thread
    if (CoreApi_GetCommandLineArguments()["Profile"])
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpointUrl */ CoreApi_GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    m_debugDrawer = CreateObject<DebugDrawer>();

    m_viewCollectionBatch = new TaskBatch();
    m_viewCollectionBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_GENERIC);

    SetReady(true);
}

World* EngineDriver::GetCurrentWorld() const
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    return m_currentWorld;
}

void EngineDriver::SetCurrentWorld(World* world)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (world == m_currentWorld)
    {
        return;
    }

    if (world)
    {
        AssertDebug(!(world->GetWorldFlags() & WorldFlags::EDITOR_WORLD), "Cannot set an editor world as the current world!");
        AssertDebug(m_worlds.FindAs(world) != m_worlds.End(), "World must be added to the engine before it can be set as the current world!");
    }

    m_currentWorld = world;

    OnCurrentWorldChanged(m_currentWorld);
}

void EngineDriver::SetDefaultWorld(const Handle<World>& defaultWorld)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    m_defaultWorld = defaultWorld;

    if (IsInitCalled())
    {
        InitObject(m_defaultWorld);
    }
}

void EngineDriver::EnqueueWorldRender(World* world)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    AssertDebug(world != nullptr && world->IsReady());

    const uint32 slot = RenderApi::GetRingIndex();

    auto& worldsToRender = m_worldsToRenderPerFrame[slot];

    if (!worldsToRender.Contains(world))
    {
        if ((world->GetWorldFlags() & WorldFlags::EDITOR_WORLD))
        {
            // editor world gets rendered first
            worldsToRender.PushFront(world);
            return;
        }

        worldsToRender.PushBack(world);
    }
}

void EngineDriver::AddWorld(const Handle<World>& world)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!world)
    {
        return;
    }

    InitObject(world);

    if (!m_worlds.Contains(world))
    {
        m_worlds.PushBack(world);
    }
}

void EngineDriver::RemoveWorld(const World* world)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    if (!world)
    {
        return;
    }

    auto it = m_worlds.FindIf([world](const Handle<World>& other)
        {
            return other.Get() == world;
        });

    if (it != m_worlds.End())
    {
        if (m_currentWorld == world)
        {
            SetCurrentWorld(nullptr);
        }

        SafeDelete(std::move(*it));
        m_worlds.Erase(it);
    }
}

bool EngineDriver::IsRenderLoopActive() const
{
    return g_renderThreadInstance != nullptr
        && g_renderThreadInstance->IsRunning();
}

void EngineDriver::SetGameInstance(Game* gameInstance)
{
    AssertOnThread(g_mainThread);

    Assert(gameInstance != nullptr);
    Assert(g_gameThreadInstance->IsRunning());

    Handle<Game> gameInstanceStrong = MakeStrongRef(gameInstance);

    g_gameInstance = gameInstanceStrong;
    g_gameThreadInstance->SetGame(gameInstanceStrong);
}

Game* EngineDriver::GetGameInstance() const
{
    AssertOnThread(g_mainThread);

    return g_gameInstance;
}

void EngineDriver::StartThreads()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);
    AssertReady();

    Assert(g_renderThreadInstance != nullptr
        && g_gameThreadInstance != nullptr
        && g_mainThreadInstance != nullptr);

    Assert(!g_renderThreadInstance->IsRunning(), "Render thread is already running!");
    Assert(!g_gameThreadInstance->IsRunning(), "Game thread is already running!");

    Assert(g_renderThreadInstance->Start(), "Failed to start render thread!");
    Assert(g_gameThreadInstance->Start(), "Failed to start game thread!");
    Assert(g_mainThreadInstance->Start(), "Failed to start main thread!");
}

void EngineDriver::RequestStop()
{
    if (int32 shutdownCounter = AtomicIncrement(&m_isShuttingDown); shutdownCounter == 1)
    {
        if (g_renderThreadInstance != nullptr && g_renderThreadInstance->IsRunning())
        {
            g_renderThreadInstance->Stop();
        }

        if (g_gameThreadInstance != nullptr && g_gameThreadInstance->IsRunning())
        {
            g_gameThreadInstance->Stop();
        }
    }
}

void EngineDriver::FinalizeStop()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Assert(AtomicAdd(&m_isShuttingDown, 0) >= 1);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    m_delegates.OnShutdown();

    if (m_scriptingService)
    {
        m_scriptingService->Stop();
        m_scriptingService.Reset();
    }

    if (m_viewCollectionBatch)
    {
        AssertDebug(m_viewCollectionBatch->IsCompleted());

        delete m_viewCollectionBatch;
        m_viewCollectionBatch = nullptr;
    }

    // must stop before net request thread
    StopProfilerConnectionThread();

    if (RC<NetRequestThread> netRequestThread = GetGlobalNetRequestThread())
    {
        if (netRequestThread->IsRunning())
        {
            netRequestThread->Stop();
        }

        if (netRequestThread->CanJoin())
        {
            netRequestThread->Join();
        }

        SetGlobalNetRequestThread(nullptr);
    }

    SafeDelete(std::move(m_worlds));

    m_debugDrawer.Reset();

    // delete remaining enqueued deletions.
    // loop until all deletions are done

    // clang-format off
    FixedArray<int, RingBufferDepth> counts {};
    
    do
    {
        for (uint32 i = 0; i < RingBufferDepth; i++)
        {
            counts[i] = g_safeDeleter->ForceDeleteAll(i);
        }

        ThreadSleep(1); // give some time for other threads to finish
    }
    while (AnyOf(counts, [](uint32 count) { return count > 0; }));
    // clang-format on

    m_isShuttingDown = 0;
}

void EngineDriver::PreFrameUpdate(Frame* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);
}

void EngineDriver::GameThreadUpdate(float delta)
{
    if (m_scriptingService)
    {
        m_scriptingService->Update();
    }

    g_streamingManager->Update(delta);

    const uint32 slot = RenderApi::GetRingIndex();

    m_worldsToRenderPerFrame[slot].Clear();

    Array<View*, SceneAllocator> viewsToProcess;
    Array<Subsystem*, SceneAllocator> subsystemsToProcess;

    TaskBatch worldUpdateTaskBatch;
    TaskBatch* currBatch = &worldUpdateTaskBatch;

    for (uint32 i = 0; i < uint32(m_worlds.Size()); i++)
    {
        World* world = m_worlds[i];

        world->CollectViews(viewsToProcess);
        world->CollectSubsystems(subsystemsToProcess);

        world->BeginUpdate(*currBatch, delta);

        if (i != uint32(m_worlds.Size() - 1))
        {
            // get the tail to pass to the next world's BeginUpdate()
            while (currBatch->nextBatch != nullptr)
            {
                currBatch = currBatch->nextBatch;
            }
        }

        EnqueueWorldRender(world);
    }

    // Remove non-unqiue views
    for (auto it = viewsToProcess.Begin(); it != viewsToProcess.End();)
    {
        const SizeType idx = viewsToProcess.IndexOf(it);
        if (idx != std::distance(viewsToProcess.Begin(), it))
        {
            it = viewsToProcess.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Remove non-unique subsystems
    for (auto it = subsystemsToProcess.Begin(); it != subsystemsToProcess.End();)
    {
        const SizeType idx = subsystemsToProcess.IndexOf(it);
        if (idx != std::distance(subsystemsToProcess.Begin(), it))
        {
            it = subsystemsToProcess.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Update worlds and their systems asynchronously - execution defined by
    // component descriptors on systems.
    TaskSystem::GetInstance().EnqueueBatch(&worldUpdateTaskBatch);
    worldUpdateTaskBatch.AwaitCompletion();

    for (World* world : m_worlds)
    {
        world->EndUpdate();
    }

#if HYP_PROCESS_SUBSYSTEMS_ASYNC
    Array<Task<void>, SceneAllocator> updateSubsystemTasks;

    for (Subsystem* subsystem : subsystemsToProcess)
    {
        if (subsystem->RequiresUpdateOnGameThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);

        updateSubsystemTasks.PushBack(TaskSystem::GetInstance().Enqueue([subsystem, delta]
            {
                HYP_NAMED_SCOPE_FMT("Update subsystem: {}", subsystem->InstanceClass()->GetName());

                subsystem->Update(delta);
            }));
    }

    for (Subsystem* subsystem : subsystemsToProcess)
    {
        if (!subsystem->RequiresUpdateOnGameThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }

    for (Task<void>& task : updateSubsystemTasks)
    {
        task.Await();
    }

    updateSubsystemTasks.Clear();
#else
    for (Subsystem* subsystem : m_subsystemsArray)
    {
        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }
#endif

    for (uint32 index = 0; index < viewsToProcess.Size(); index++)
    {
        HYP_NAMED_SCOPE("Per-view entity collection");

        View* view = viewsToProcess[index];
        Assert(view != nullptr);

        view->UpdateViewport();
        // View must be updated on the game thread as it mutates the scene's octree state
        view->UpdateVisibility();

#if HYP_PROCESS_VIEWS_ASYNC
        view->BeginAsyncCollection(*m_viewCollectionBatch);
#else
        view->CollectSync();
#endif
    }

#if HYP_PROCESS_VIEWS_ASYNC
    TaskSystem::GetInstance().EnqueueBatch(m_viewCollectionBatch);
    m_viewCollectionBatch->AwaitCompletion();

    for (uint32 index = 0; index < viewsToProcess.Size(); index++)
    {
        viewsToProcess[index]->EndAsyncCollection();
    }
#endif

#if HYP_PROCESS_VIEWS_ASYNC
    AssertDebug(m_viewCollectionBatch != nullptr);
    AssertDebug(m_viewCollectionBatch->IsCompleted());

    m_viewCollectionBatch->ResetState();
#endif

    // write buffered render data
    WorldShaderData* bufferData = RenderApi::GetWorldBufferData();
    bufferData->frameCounter = RenderApi::GetFrameCounter();

    if (m_currentWorld)
    {
        bufferData->gameTime = m_currentWorld->GetGameState().gameTime;
    }
}

#pragma endregion EngineDriver

static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <rendering/inl/DescriptorSets.inl>
    }
} s_globalDescriptorSetsDeclarations;

} // namespace hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineMemory.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/GameThread.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderCommand.hpp>

#include <rendering/AsyncCompute.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderSwapchain.hpp>
#include <rendering/RenderConfig.hpp>

#include <engine/DebugDrawer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <asset/Assets.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>

#include <rendering/Texture.hpp>

#include <system/SystemEvent.hpp>

#include <core/debug/StackDump.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/reflection/Enum.hpp> // For EnumValue()

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/cli/CommandLine.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <scripting/ScriptingService.hpp>

#include <HyperionEngine.hpp>

#define HYP_PROCESS_VIEWS_ASYNC 1
#define HYP_PROCESS_SUBSYSTEMS_ASYNC 1

#ifdef HYP_LIBUI
#include <ui.h>
#endif

#include <EngineDriver.generated.inl>

namespace hyperion {

class RenderThread;

void HandleSignal(int signum);

extern const GlobalConfig& CoreApi_GetGlobalConfig();
extern FilePath CoreApi_GetExecutablePath();
extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

EngineStatTimer g_renderThreadUpdateTimer("Frame/RenderThreadUpdate");

#pragma region RenderThread

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread()
        : Thread(g_renderThread, ThreadPriorityValue::HIGHEST)
    {
    }

    bool Start()
    {
        signal(SIGINT, HandleSignal);
        signal(SIGSEGV, HandleSignal);

        // invoke thread operation on main thread.
        if (m_id == g_mainThread)
        {
            Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

            SetCurrentThreadObject(this);

            (*this)();
            return true;
        }

        return Thread::Start();
    }

private:
    virtual void operator()() override
    {
        RenderApi::Init();

        // init window swapchain after rendering api is initialized
        if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
        {
            mainWindow->CreateSwapchain();
        }

        /// HAX !!! We should only upload gpu resources on first use for debug draer
        InitObject(g_engineDriver->GetDebugDrawer());

        SystemEvent event;
        Queue<Scheduler::ScheduledTask> tasks;

#ifdef HYP_LIBUI
        uiInitOptions options = {};
        const char* err = uiInit(&options);
        if (err != nullptr)
        {
            uiFreeInitError(err);

            HYP_FAIL("Failed to initialize libui! Message: {}", err);

            return;
        }
#endif

        while (m_isRunning.Get(MemoryOrder::RELAXED))
        {
            ENGINE_STAT_SCOPE(&g_renderThreadUpdateTimer);

#ifdef HYP_LIBUI
            uiMainSteps();
#endif

            RenderApi::BeginFrame_RenderThread();

            // if we're the main thread, we're responsible for handling input events.
            if (m_id == g_mainThread)
            {
                while (g_appContext->PollEvent(event))
                {
                    g_appContext->GetMainWindow()->GetInputEventSink().Push(std::move(event));
                }
            }

            if (uint32 numEnqueued = m_scheduler.NumEnqueued())
            {
                m_scheduler.AcceptAll(tasks);

                while (tasks.Any())
                {
                    tasks.Pop().Execute();
                }
            }

            g_engineDriver->RenderNextFrame();

            RenderApi::EndFrame_RenderThread();
        }

#ifdef HYP_LIBUI
        uiUninit();
#endif

        RenderApi::Shutdown();
    }
};

#pragma endregion RenderThread

void HandleSignal(int signum)
{
#ifdef HYP_WINDOWS
    sys::Win32_CleanupWindowClasses();
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

    m_renderThread = MakeUnique<RenderThread>();
    m_gameThread = MakeUnique<GameThread>();

#ifdef HYP_EDITOR
    // Create script compilation service
    m_scriptingService = MakeUnique<ScriptingService>(
        GetResourceDirectory() / "scripts" / "src",
        GetResourceDirectory() / "scripts" / "projects",
        CoreApi_GetExecutablePath()); // copy script binaries into executable path

    m_scriptingService->Start();
#endif

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
    return m_renderThread != nullptr
        && m_renderThread->IsRunning();
}

void EngineDriver::StartThreadsForGame(const Handle<Game>& game)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    AssertReady();

    Assert(game != nullptr);

    Assert(m_renderThread && m_gameThread, "EngineDriver threads must be created in Init()!");

    Assert(!m_renderThread->IsRunning(), "Render thread is already running!");
    Assert(!m_gameThread->IsRunning(), "Game thread is already running!");

    m_gameThread->SetGame(game);

    Assert(m_gameThread->Start(), "Failed to start game thread!");
    Assert(m_renderThread->Start(), "Failed to start render thread!");
}

void EngineDriver::RequestStop()
{
    if (int32 shutdownCounter = AtomicIncrement(&m_isShuttingDown); shutdownCounter == 1)
    {
        if (m_renderThread != nullptr && m_renderThread->IsRunning())
        {
            m_renderThread->Stop();
        }

        if (m_gameThread != nullptr && m_gameThread->IsRunning())
        {
            m_gameThread->Stop();
        }
    }
}

void EngineDriver::FinalizeStop()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    Assert(AtomicAdd(&m_isShuttingDown, 0) >= 1);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    if (m_gameThread != nullptr)
    {
        m_gameThread->Join();
        m_gameThread.Reset();
    }

    if (m_renderThread)
    {
        m_renderThread->Stop();
        m_renderThread->Join();

        m_renderThread.Reset();
    }

    // look at me, i'm the render thread now
    g_renderThread = g_mainThread;

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

HYP_API void EngineDriver::RenderNextFrame()
{
    HYP_PROFILE_BEGIN;
    AssertOnThread(g_renderThread);

    Frame* frame = g_renderBackend->PrepareNextFrame();
    Assert(frame != nullptr);

    PreFrameUpdate(frame);

    Swapchain* swapchain = nullptr;

    if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
    {
        swapchain = mainWindow->GetSwapchain();
    }

    auto& worldsToRender = m_worldsToRenderPerFrame[RenderApi::GetRingIndex()];

    if (worldsToRender.Any())
    {
        uint32 numViewsRendered = 0;

        RendererBase* mainRenderer = g_renderGlobalState->globalRenderers[GRT_MAIN][0];
        AssertDebug(mainRenderer != nullptr);

        RenderSetup rs;
        rs.swapchain = swapchain;

        for (World* world : worldsToRender)
        {
            AssertDebug(world != nullptr && world->IsReady());

            rs.world = world;

#if HYP_EDITOR
            // for editor world, render UI as well
            if ((world->GetWorldFlags() & WorldFlags::EDITOR_WORLD))
            {
                if (RendererBase* uiRenderer = g_renderGlobalState->globalRenderers[GRT_UI][0])
                {
                    uiRenderer->RenderFrame(frame, rs);
                }
            }
#endif

            if (world->GetViews().Size() != 0)
            {
                mainRenderer->RenderFrame(frame, rs);
                numViewsRendered += world->GetViews().Size();
            }
        }

        rs.world = nullptr;

        if (!g_renderGlobalState->finalPass)
        {
            g_renderGlobalState->finalPass = PoolNew<FinalPass>(*g_renderPool);
            g_renderGlobalState->finalPass->Create();
        }

        g_renderGlobalState->finalPass->Render(frame, rs);
    }

    g_renderGlobalState->UpdateBuffers(frame);

    g_renderBackend->SubmitCommandBuffers();

    if (swapchain != nullptr)
    {
        g_renderBackend->PresentToSwapchain(swapchain);
    }
}

void EngineDriver::PreFrameUpdate(Frame* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    // Check if any swapchains need to be recreated
    Array<Swapchain*, RenderTempAllocator> swapchains;

    if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
    {
        if (Swapchain* swapchain = mainWindow->GetSwapchain().Get())
        {
            swapchains.PushBack(swapchain);
        }
    }

    for (Swapchain* swapchain : swapchains)
    {
        g_renderBackend->PrepareSwapchain(swapchain);
    }

    g_renderGlobalState->gpuBuffers[GRB_WORLDS]->WriteBufferData(0, RenderApi::GetWorldBufferData(), sizeof(WorldShaderData));
}

void EngineDriver::GameThreadUpdate(float delta)
{
    m_scriptingService->Update();
    g_streamingManager->Update(delta);

    const uint32 slot = RenderApi::GetRingIndex();

    m_worldsToRenderPerFrame[slot].Clear();

    Array<View*, SceneAllocator> viewsToProcess;
    Array<Subsystem*, SceneAllocator> subsystemsToProcess;

    TaskBatch worldUpdateTaskBatch;
    TaskBatch* pCurrBatch = &worldUpdateTaskBatch;

    for (uint32 i = 0; i < uint32(m_worlds.Size()); i++)
    {
        World* world = m_worlds[i];

        world->CollectViews(viewsToProcess);
        world->CollectSubsystems(subsystemsToProcess);

        world->BeginUpdate(*pCurrBatch, delta);

        if (i != uint32(m_worlds.Size() - 1))
        {
            // get the tail to pass to the next world's BeginUpdate()
            while (pCurrBatch->nextBatch != nullptr)
            {
                pCurrBatch = pCurrBatch->nextBatch;
            }
        }

        EnqueueWorldRender(world);
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

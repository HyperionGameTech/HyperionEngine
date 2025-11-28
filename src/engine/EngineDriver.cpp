/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineMemory.hpp>

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

#include <core/debug/StackDump.hpp>
#include <system/SystemEvent.hpp>

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

#include <engine/EngineGlobals.hpp>
#include <engine/EngineStats.hpp>

#include <HyperionEngine.hpp>

#define HYP_PROCESS_VIEWS_ASYNC 1
#define HYP_PROCESS_SUBSYSTEMS_ASYNC 1

#ifdef HYP_LIBUI
#include <ui.h>
#endif

#include <EngineDriver.generated.inl>

namespace hyperion {

class RenderThread;
static RenderThread* g_renderThreadInstance = nullptr;

void HandleSignal(int signum);

extern const GlobalConfig& CoreApi_GetGlobalConfig();
extern FilePath CoreApi_GetExecutablePath();
extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

EngineStatTimer g_renderThreadUpdateTimer("Frame/RenderThreadUpdate");

#pragma region RenderThread

class RenderThread final : public Thread<Scheduler>
{
public:
    explicit RenderThread(bool useSeparateThread)
        : Thread(g_renderThread, ThreadPriorityValue::HIGHEST),
          m_useSeparateThread(useSeparateThread),
          m_isRunning(false)
    {
    }

    bool Start()
    {
        Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        signal(SIGINT, HandleSignal);
        signal(SIGSEGV, HandleSignal);

        g_renderThreadInstance = this;

        if (!m_useSeparateThread)
        {
            SetCurrentThreadObject(this);

            // call on main thread
            (*this)();
            return true;
        }

        return Thread::Start();
    }

    void Stop() override
    {
        m_isRunning.Set(false, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_isRunning.Get(MemoryOrder::ACQUIRE);
    }

private:
    virtual void operator()() override
    {
        RenderApi::Init();

        
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

            while (g_appContext->PollEvent(event))
            {
                g_appContext->GetMainWindow()->GetInputEventSink().Push(std::move(event));
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

        g_renderThreadInstance = nullptr;
    }

    bool m_useSeparateThread;
    AtomicVar<bool> m_isRunning;
};

#pragma endregion RenderThread

void HandleSignal(int signum)
{
    if (!g_renderThreadInstance)
    {
        return;
    }

    //    Time startTime = Time::Now();

    g_renderThreadInstance->Stop();
    //
    //    while (g_renderThreadInstance->IsRunning())
    //    {
    //        ThreadSleep(10);
    //    }
    //
    //    g_renderThreadInstance->Join();

    exit(signum);
}

#pragma region Render commands

struct RecreateSwapchain : RenderCommand
{
    WeakHandle<EngineDriver> engineWeak;

    RecreateSwapchain(const Handle<EngineDriver>& engine)
        : engineWeak(engine)
    {
    }

    virtual ~RecreateSwapchain() override = default;

    virtual RendererResult operator()() override
    {
        Handle<EngineDriver> engine = engineWeak.Lock();

        if (!engine)
        {
            HYP_LOG(Rendering, Warning, "EngineDriver was destroyed before swapchain could be recreated");
            HYPERION_RETURN_OK;
        }

        engine->m_shouldRecreateSwapchain = true;

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region EngineDriver

const Handle<EngineDriver>& EngineDriver::GetInstance()
{
    return g_engineDriver;
}

EngineDriver::EngineDriver()
    : m_currentWorld(nullptr),
      m_isShuttingDown(false),
      m_shouldRecreateSwapchain(false),
      m_viewCollectionBatch(nullptr)
{
}

EngineDriver::~EngineDriver()
{
}

HYP_API void EngineDriver::Init()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    // Set ready to false after render thread stops running.
    HYP_DEFER({ SetReady(false); });

    if (CoreApi_GetCommandLineArguments()["Headless"].ToBool(false))
    {
        // in headless mode, don't block the caller thread; on Hyp_Initialize() call,
        // we need to create a separate thread for rendering.

        g_renderThread = StaticThreadId(NAME("Render"));
        m_renderThread = MakeUnique<RenderThread>(true);
    }
    else
    {
        g_renderThread = g_mainThread;
        m_renderThread = MakeUnique<RenderThread>(false);
    }

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

    // g_streamingManager->Start();

    // must start after net request thread
    if (CoreApi_GetCommandLineArguments()["Profile"])
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpointUrl */ CoreApi_GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    m_debugDrawer = CreateObject<DebugDrawer>();

    m_defaultWorld = CreateObject<World>(NAME("DefaultWorld"), WorldFlags::NONE);
    InitObject(m_defaultWorld);

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

bool EngineDriver::StartRenderLoop()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    if (m_renderThread == nullptr)
    {
        HYP_LOG(Engine, Error, "Render thread is not initialized!");
        return false;
    }

    if (m_renderThread->IsRunning())
    {
        HYP_LOG(Engine, Warning, "Render thread is already running!");
        return true;
    }

    m_renderThread->Start();

    return true;
}

void EngineDriver::RequestStop()
{
    if (m_renderThread != nullptr)
    {
        if (m_renderThread->IsRunning())
        {
            m_renderThread->Stop();
        }
    }
}

void EngineDriver::FinalizeStop()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    m_isShuttingDown.Set(true, MemoryOrder::SEQUENTIAL);

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

    m_renderThread->Join();
    m_renderThread.Reset();
}

HYP_API void EngineDriver::RenderNextFrame()
{
    HYP_PROFILE_BEGIN;
    AssertOnThread(g_renderThread);

    FrameBase* frame = g_renderBackend->PrepareNextFrame();

    PreFrameUpdate(frame);

    auto& worldsToRender = m_worldsToRenderPerFrame[RenderApi::GetRingIndex()];

    if (worldsToRender.Any())
    {
        uint32 numViewsRendered = 0;

        RendererBase* mainRenderer = g_renderGlobalState->globalRenderers[GRT_MAIN][0];
        AssertDebug(mainRenderer != nullptr);

        for (World* world : worldsToRender)
        {
            AssertDebug(world != nullptr && world->IsReady());

            RenderSetup rs { world, nullptr };

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

        g_renderGlobalState->finalPass->Render(frame, RenderSetup());
    }

    g_renderGlobalState->UpdateBuffers(frame);

    g_renderBackend->PresentFrame(frame);
}

void EngineDriver::PreFrameUpdate(FrameBase* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

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

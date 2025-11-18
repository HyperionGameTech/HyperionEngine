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
#include <rendering/Texture.hpp>

#include <core/debug/StackDump.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>

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

#define HYP_LOG_FRAMES_PER_SECOND

// temp, move this
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

#pragma region RenderThread

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread()
        : Thread(g_renderThread, ThreadPriorityValue::HIGHEST),
          m_isRunning(false)
    {
    }

    bool Start()
    {
        Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // Must be current thread
        AssertOnThread(g_renderThread);

        SetCurrentThreadObject(this);
        m_scheduler.SetOwnerThread(Id());

        signal(SIGINT, HandleSignal);
        signal(SIGSEGV, HandleSignal);

        (*this)();

        return true;
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
        SystemEvent event;

        Queue<Scheduler::ScheduledTask> tasks;

        g_renderThreadInstance = this;

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

        g_renderThreadInstance = nullptr;
    }

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
      m_shouldRecreateSwapchain(false)
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

    m_renderThread = MakeUnique<RenderThread>();

    Assert(g_renderBackend != nullptr);

    g_renderBackend->GetOnSwapchainRecreatedDelegate()
        .Bind([this](SwapchainBase* swapchain)
            {
                m_finalPass = MakeUnique<FinalPass>(swapchain->HandleFromThis());
                m_finalPass->Create();
            })
        .Detach();

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

    m_finalPass = MakeUnique<FinalPass>(g_renderBackend->GetSwapchain()->HandleFromThis());
    m_finalPass->Create();

    m_debugDrawer = CreateObject<DebugDrawer>();
    InitObject(m_debugDrawer);

    m_defaultWorld = CreateObject<World>();
    m_defaultWorld->SetName(NAME("DefaultWorld"));

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

    RenderApi::Shutdown();

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
    m_finalPass.Reset();

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

            // @FIXME: will overwrite !!!
            g_renderGlobalState->gpuBuffers[GRB_WORLDS]->WriteBufferData(0, RenderApi::GetWorldBufferData(), sizeof(WorldShaderData));

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

        m_finalPass->Render(frame, RenderSetup());
    }

    g_renderGlobalState->UpdateBuffers(frame);

    g_renderBackend->PresentFrame(frame);
}

void EngineDriver::PreFrameUpdate(FrameBase* frame)
{
    HYP_SCOPE;

    AssertOnThread(g_renderThread);
}

void EngineDriver::GameThreadUpdate(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    m_scriptingService->Update();

    const uint32 slot = RenderApi::GetRingIndex();

    m_worldsToRenderPerFrame[slot].Clear();

    for (World* world : m_worlds)
    {
        world->Update(delta);

        EnqueueWorldRender(world);
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

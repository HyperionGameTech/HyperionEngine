/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/AsyncCompute.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderSwapchain.hpp>
#include <rendering/RenderConfig.hpp>

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

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <scripting/ScriptingService.hpp>

#include <EngineGlobals.hpp>
#include <HyperionEngine.hpp>

#define HYP_LOG_FRAMES_PER_SECOND

namespace hyperion {

static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <rendering/inl/DescriptorSets.inl>
    }
} g_globalDescriptorSetsDeclarations;

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread(const Handle<AppContextBase>& appContext)
        : Thread(g_renderThread, ThreadPriorityValue::HIGHEST),
          m_appContext(appContext),
          m_isRunning(false)
    {
    }

    // Overrides Start() to not create a thread object. Runs the render loop on the main thread.
    bool Start()
    {
        Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // Must be current thread
        Threads::AssertOnThread(g_renderThread);

        SetCurrentThreadObject(this);
        m_scheduler.SetOwnerThread(Id());

        (*this)();

        return true;
    }

    void Stop()
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
        Assert(m_appContext != nullptr);

        SystemEvent event;

        Queue<Scheduler::ScheduledTask> tasks;

        while (m_isRunning.Get(MemoryOrder::RELAXED))
        {
            // input manager stuff
            while (m_appContext->PollEvent(event))
            {
                m_appContext->GetMainWindow()->GetInputEventSink().Push(std::move(event));
            }

            RenderApi_BeginFrame_RenderThread();

            if (uint32 numEnqueued = m_scheduler.NumEnqueued())
            {
                m_scheduler.AcceptAll(tasks);

                while (tasks.Any())
                {
                    tasks.Pop().Execute();
                }
            }

            g_engine->RenderNextFrame();

            RenderApi_EndFrame_RenderThread();
        }
    }

    Handle<AppContextBase> m_appContext;
    AtomicVar<bool> m_isRunning;
};

#pragma region Render commands

struct RENDER_COMMAND(RecreateSwapchain)
    : RenderCommand
{
    WeakHandle<Engine> engineWeak;

    RENDER_COMMAND(RecreateSwapchain)(const Handle<Engine>& engine)
        : engineWeak(engine)
    {
    }

    virtual ~RENDER_COMMAND(RecreateSwapchain)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<Engine> engine = engineWeak.Lock();

        if (!engine)
        {
            HYP_LOG(Rendering, Warning, "Engine was destroyed before swapchain could be recreated");
            HYPERION_RETURN_OK;
        }

        engine->m_shouldRecreateSwapchain = true;

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region Engine

const Handle<Engine>& Engine::GetInstance()
{
    return g_engine;
}

Engine::Engine()
    : m_isShuttingDown(false),
      m_shouldRecreateSwapchain(false)
{
}

Engine::~Engine()
{
}

HYP_API void Engine::Init()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    // Set ready to false after render thread stops running.
    HYP_DEFER({ SetReady(false); });

    Assert(m_appContext != nullptr, "App context must be set before initializing the engine!");

    m_renderThread = MakeUnique<RenderThread>(m_appContext);
    RenderApi_Init();

    Assert(m_appContext->GetMainWindow() != nullptr);

    // m_appContext->GetMainWindow()->OnWindowSizeChanged.Bind(
    //                                                        [this](Vec2i newWindowSize)
    //                                                        {
    //                                                            HYP_LOG(Engine, Info, "Resize window to {}", newWindowSize);

    //                                                            // m_finalPass->Resize(Vec2u(newWindowSize));
    //                                                        })
    //     .Detach();

    Assert(g_renderBackend != nullptr);

    g_renderBackend->GetOnSwapchainRecreatedDelegate()
        .Bind([this](SwapchainBase* swapchain)
            {
                m_finalPass = MakeUnique<FinalPass>(swapchain->HandleFromThis());
                m_finalPass->Create();
            })
        .Detach();

    // Update app configuration to reflect device, after instance is created (e.g RT is not supported)
    m_appContext->UpdateConfigurationOverrides();

    m_configuration.SetToDefaultConfiguration();
    m_configuration.LoadFromDefinitionsFile();

#ifdef HYP_EDITOR
    // Create script compilation service
    m_scriptingService = MakeUnique<ScriptingService>(
        GetResourceDirectory() / "scripts" / "src",
        GetResourceDirectory() / "scripts" / "projects",
        GetExecutablePath()); // copy script binaries into executable path

    m_scriptingService->Start();
#endif

    RC<NetRequestThread> netRequestThread = MakeRefCountedPtr<NetRequestThread>();
    SetGlobalNetRequestThread(netRequestThread);
    netRequestThread->Start();

    // g_streamingManager->Start();

    // must start after net request thread
    if (GetCommandLineArguments()["Profile"])
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpointUrl */ GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    m_finalPass = MakeUnique<FinalPass>(g_renderBackend->GetSwapchain()->HandleFromThis());
    m_finalPass->Create();

    m_debugDrawer = MakeUnique<DebugDrawer>();
    m_debugDrawer->Initialize();

    m_world = CreateObject<World>();

    SetReady(true);
}

bool Engine::IsRenderLoopActive() const
{
    return m_renderThread != nullptr
        && m_renderThread->IsRunning();
}

bool Engine::StartRenderLoop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

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

    RenderApi_Shutdown();

    return true;
}

void Engine::RequestStop()
{
    if (m_renderThread != nullptr)
    {
        if (m_renderThread->IsRunning())
        {
            m_renderThread->Stop();
        }
    }
}

void Engine::FinalizeStop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

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

    m_world.Reset();

    m_debugDrawer.Reset();

    m_finalPass.Reset();

    g_safeDeleter->ForceDeleteAll();

    m_renderThread->Join();
    m_renderThread.Reset();
}

HYP_API void Engine::RenderNextFrame()
{
    HYP_PROFILE_BEGIN;

    FrameBase* frame = g_renderBackend->PrepareNextFrame();

    PreFrameUpdate(frame);

    if (m_world->IsReady())
    {
        g_renderGlobalState->gpuBuffers[GRB_WORLDS]->WriteBufferData(0, RenderApi_GetWorldBufferData(), sizeof(WorldShaderData));

        RenderSetup rs { m_world, nullptr };
        g_renderGlobalState->mainRenderer->RenderFrame(frame, rs);

        m_finalPass->Render(frame, rs);
    }

    g_renderGlobalState->UpdateBuffers(frame);

    g_renderBackend->PresentFrame(frame);
}

void Engine::PreFrameUpdate(FrameBase* frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    RenderObjectDeleter::Iterate();

    g_safeDeleter->PerformEnqueuedDeletions();
}

#pragma endregion Engine

} // namespace hyperion

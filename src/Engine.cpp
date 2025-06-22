/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Engine.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <asset/Assets.hpp>

#include <streaming/StreamingManager.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/World.hpp>
#include <scene/Texture.hpp>

#include <core/debug/StackDump.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/net/NetRequestThread.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/object/HypClassUtils.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <streaming/StreamingThread.hpp>

#include <scripting/ScriptingService.hpp>

#include <HyperionEngine.hpp>

#define HYP_LOG_FRAMES_PER_SECOND

namespace hyperion {

using renderer::FillMode;
using renderer::GPUBufferType;

namespace renderer {
static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <rendering/inl/DescriptorSets.inl>
    }
} g_global_descriptor_sets_declarations;
} // namespace renderer

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread(const Handle<AppContextBase>& app_context)
        : Thread(g_render_thread, ThreadPriorityValue::HIGHEST),
          m_app_context(app_context),
          m_is_running(false)
    {
    }

    // Overrides Start() to not create a thread object. Runs the render loop on the main thread.
    bool Start()
    {
        AssertThrow(m_is_running.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // Must be current thread
        Threads::AssertOnThread(g_render_thread);

        SetCurrentThreadObject(this);
        m_scheduler.SetOwnerThread(GetID());

        (*this)();

        return true;
    }

    void Stop()
    {
        m_is_running.Set(false, MemoryOrder::RELEASE);
    }

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_is_running.Get(MemoryOrder::ACQUIRE);
    }

private:
    virtual void operator()() override
    {
        AssertThrow(m_app_context != nullptr);

        SystemEvent event;

        Queue<Scheduler::ScheduledTask> tasks;

        while (m_is_running.Get(MemoryOrder::RELAXED))
        {
            // input manager stuff
            while (m_app_context->PollEvent(event))
            {
                m_app_context->GetMainWindow()->GetInputEventSink().Push(std::move(event));
            }

            BeginFrame_RenderThread();

            if (uint32 num_enqueued = m_scheduler.NumEnqueued())
            {
                m_scheduler.AcceptAll(tasks);

                while (tasks.Any())
                {
                    tasks.Pop().Execute();
                }
            }

            g_engine->RenderNextFrame();

            EndFrame_RenderThread();
        }
    }

    Handle<AppContextBase> m_app_context;
    AtomicVar<bool> m_is_running;
};

#pragma region Render commands

struct RENDER_COMMAND(RecreateSwapchain)
    : renderer::RenderCommand
{
    WeakHandle<Engine> engine_weak;

    RENDER_COMMAND(RecreateSwapchain)(const Handle<Engine>& engine)
        : engine_weak(engine)
    {
    }

    virtual ~RENDER_COMMAND(RecreateSwapchain)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<Engine> engine = engine_weak.Lock();

        if (!engine)
        {
            HYP_LOG(Rendering, Warning, "Engine was destroyed before swapchain could be recreated");
            HYPERION_RETURN_OK;
        }

        engine->m_should_recreate_swapchain = true;

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
    : m_is_shutting_down(false),
      m_should_recreate_swapchain(false)
{
}

Engine::~Engine()
{
}

HYP_API void Engine::Init()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_main_thread);

    // Set ready to false after render thread stops running.
    HYP_DEFER({ SetReady(false); });

    AssertThrowMsg(m_app_context != nullptr, "App context must be set before initializing the engine!");

    m_render_thread = MakeUnique<RenderThread>(m_app_context);

    AssertThrow(m_app_context->GetMainWindow() != nullptr);

    // m_app_context->GetMainWindow()->OnWindowSizeChanged.Bind(
    //                                                        [this](Vec2i new_window_size)
    //                                                        {
    //                                                            HYP_LOG(Engine, Info, "Resize window to {}", new_window_size);

    //                                                            // m_final_pass->Resize(Vec2u(new_window_size));
    //                                                        })
    //     .Detach();

    TaskSystem::GetInstance().Start();

    AssertThrow(g_rendering_api != nullptr);

    g_rendering_api->GetOnSwapchainRecreatedDelegate()
        .Bind([this](SwapchainBase* swapchain)
            {
                m_final_pass = MakeUnique<FinalPass>(swapchain->HandleFromThis());
                m_final_pass->Create();
            })
        .Detach();

    // Update app configuration to reflect device, after instance is created (e.g RT is not supported)
    m_app_context->UpdateConfigurationOverrides();

    m_configuration.SetToDefaultConfiguration();
    m_configuration.LoadFromDefinitionsFile();

    if (!m_shader_compiler.LoadShaderDefinitions())
    {
        HYP_FAIL("Failed to load shaders from definitions file!");
    }

#ifdef HYP_EDITOR
    // Create script compilation service
    m_scripting_service = MakeUnique<ScriptingService>(
        GetResourceDirectory() / "scripts" / "src",
        GetResourceDirectory() / "scripts" / "projects",
        GetResourceDirectory() / "scripts" / "bin");

    m_scripting_service->Start();
#endif

    RC<NetRequestThread> net_request_thread = MakeRefCountedPtr<NetRequestThread>();
    SetGlobalNetRequestThread(net_request_thread);
    net_request_thread->Start();

    RC<StreamingThread> streaming_thread = MakeRefCountedPtr<StreamingThread>();
    SetGlobalStreamingThread(streaming_thread);
    streaming_thread->Start();

    // g_streaming_manager->Start();

    // must start after net request thread
    if (GetCommandLineArguments()["Profile"])
    {
        StartProfilerConnectionThread(ProfilerConnectionParams {
            /* endpoint_url */ GetCommandLineArguments()["TraceURL"].ToString(),
            /* enabled */ true });
    }

    m_material_descriptor_set_manager = MakeUnique<MaterialDescriptorSetManager>();
    m_material_descriptor_set_manager->Initialize();

    m_graphics_pipeline_cache = MakeUnique<GraphicsPipelineCache>();
    m_graphics_pipeline_cache->Initialize();

    m_final_pass = MakeUnique<FinalPass>(g_rendering_api->GetSwapchain()->HandleFromThis());
    m_final_pass->Create();

    m_debug_drawer = MakeUnique<DebugDrawer>();

    m_world = CreateObject<World>();

    SetReady(true);
}

bool Engine::IsRenderLoopActive() const
{
    return m_render_thread != nullptr
        && m_render_thread->IsRunning();
}

bool Engine::StartRenderLoop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_main_thread);

    if (m_render_thread == nullptr)
    {
        HYP_LOG(Engine, Error, "Render thread is not initialized!");
        return false;
    }

    if (m_render_thread->IsRunning())
    {
        HYP_LOG(Engine, Warning, "Render thread is already running!");
        return true;
    }

    m_render_thread->Start();

    return true;
}

void Engine::RequestStop()
{
    if (m_render_thread != nullptr)
    {
        if (m_render_thread->IsRunning())
        {
            m_render_thread->Stop();
        }
    }
}

void Engine::FinalizeStop()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_main_thread);

    m_is_shutting_down.Set(true, MemoryOrder::SEQUENTIAL);

    HYP_LOG(Engine, Info, "Stopping all engine processes");

    m_delegates.OnShutdown();

    if (m_scripting_service)
    {
        m_scripting_service->Stop();
        m_scripting_service.Reset();
    }

    // must stop before net request thread
    StopProfilerConnectionThread();

    // g_streaming_manager->Stop();

    if (RC<StreamingThread> streaming_thread = GetGlobalStreamingThread())
    {
        if (streaming_thread->IsRunning())
        {
            streaming_thread->Stop();
        }

        if (streaming_thread->CanJoin())
        {
            streaming_thread->Join();
        }

        SetGlobalStreamingThread(nullptr);
    }

    if (RC<NetRequestThread> net_request_thread = GetGlobalNetRequestThread())
    {
        if (net_request_thread->IsRunning())
        {
            net_request_thread->Stop();
        }

        if (net_request_thread->CanJoin())
        {
            net_request_thread->Join();
        }

        SetGlobalNetRequestThread(nullptr);
    }

    m_world.Reset();

    if (TaskSystem::GetInstance().IsRunning())
    { // Stop task system
        HYP_LOG(Tasks, Info, "Stopping task system");

        TaskSystem::GetInstance().Stop();

        HYP_LOG(Tasks, Info, "Task system stopped");
    }

    m_graphics_pipeline_cache->Destroy();
    m_graphics_pipeline_cache.Reset();

    m_material_descriptor_set_manager.Reset();

    m_debug_drawer.Reset();

    m_final_pass.Reset();

    g_safe_deleter->ForceDeleteAll();

    m_render_thread->Join();
    m_render_thread.Reset();
}

HYP_API void Engine::RenderNextFrame()
{
    HYP_PROFILE_BEGIN;

#ifdef HYP_ENABLE_RENDER_STATS
    m_render_stats_calculator.Advance(m_render_stats);
    OnRenderStatsUpdated(m_render_stats);
#endif

    FrameBase* frame = g_rendering_api->PrepareNextFrame();

    PreFrameUpdate(frame);

    // temp
    if (m_world->IsReady())
    {
        m_world->GetRenderResource().Render(frame);

        m_final_pass->Render(frame, &m_world->GetRenderResource());

        g_render_global_state->UpdateBuffers(frame);

        m_world->GetRenderResource().PostRender(frame);
    }

    g_rendering_api->PresentFrame(frame);
}

void Engine::PreFrameUpdate(FrameBase* frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    // Add/remove pending descriptor sets before flushing render commands or updating buffers and descriptor sets.
    // otherwise we'll have an issue when render commands expect the descriptor sets to be there.
    m_material_descriptor_set_manager->UpdatePendingDescriptorSets(frame);
    m_material_descriptor_set_manager->Update(frame);

    HYPERION_ASSERT_RESULT(renderer::RenderCommands::Flush());

    if (m_world->IsReady())
        m_world->GetRenderResource().PreRender(frame);

    RenderObjectDeleter<renderer::Platform::current>::Iterate();

    g_safe_deleter->PerformEnqueuedDeletions();
}

#pragma endregion Engine

} // namespace hyperion

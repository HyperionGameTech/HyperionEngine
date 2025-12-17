/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/threads/RenderThread.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/DebugDrawer.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderSwapchain.hpp>
#include <rendering/RenderConfig.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <asset/Assets.hpp>

#include <scene/World.hpp>

#include <system/AppContext.hpp>

#include <core/threading/Threads.hpp>

namespace hyperion {

extern void HandleSignal(int signum);

extern EngineStatTimer g_renderThreadUpdateTimer;

RenderThread::RenderThread()
    : Thread(g_renderThread, ThreadPriorityValue::HIGHEST)
{
}

RenderThread::~RenderThread()
{
}

bool RenderThread::Start()
{
    signal(SIGINT, HandleSignal);
    signal(SIGSEGV, HandleSignal);

    // RenderOnMainThread option
    if (m_id == g_mainThread)
    {
        Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // DO NOT call SetCurrentThreadObject() if using RenderOnMainThread

        (*this)();

        return true;
    }

    return Thread::Start();
}

void RenderThread::Update()
{
    ENGINE_STAT_SCOPE(&g_renderThreadUpdateTimer);

    RenderApi::BeginFrame_RenderThread();

    Queue<Scheduler::ScheduledTask> tasks;
    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    Frame* frame = g_renderBackend->PrepareNextFrame();
    Assert(frame != nullptr);

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

    Swapchain* swapchain = nullptr;

    if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
    {
        swapchain = mainWindow->GetSwapchain();
    }

    Array<World*>& worldsToRender = g_engineDriver->m_worldsToRenderPerFrame[RenderApi::GetRingIndex()];

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

    g_renderBackend->SubmitCommandBuffers(swapchain);

    if (swapchain != nullptr)
    {
        g_renderBackend->PresentToSwapchain(swapchain);
    }

    RenderApi::EndFrame_RenderThread();
}

void RenderThread::operator()()
{
    RenderApi::Init();

    // init window swapchain after rendering api is initialized
    if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
    {
        mainWindow->CreateSwapchain();
    }

    /// HAX !!! We should only upload gpu resources on first use for debug draer
    InitObject(g_engineDriver->GetDebugDrawer());

    if (m_id != g_mainThread) // !RenderOnMainThread
    {
        while (m_isRunning.Get(MemoryOrder::RELAXED))
        {
            Update();
        }
    }
}

} // namespace hyperion
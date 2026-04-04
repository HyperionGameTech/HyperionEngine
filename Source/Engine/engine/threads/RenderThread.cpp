/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/threads/RenderThread.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineMemory.hpp>
#include <rendering/DebugDrawer.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/Device.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/Frame.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <asset/Assets.hpp>

#include <scene/World.hpp>

#include <system/AppContext.hpp>

#include <Core/threading/Threads.hpp>

#include <semaphore>

namespace Hyperion {

extern void HandleSignal(int signum);

extern EngineStatTimer g_statRenderUpdate;

extern ThreadSignal g_renderInitSignal;

RenderThread::RenderThread()
    : Thread(g_renderThread, ThreadPriorityValue::HIGHEST)
{
}

RenderThread::~RenderThread() = default;

bool RenderThread::Start()
{
    signal(SIGINT, HandleSignal);
    signal(SIGSEGV, HandleSignal);
    // handle integer division by zero
    signal(SIGFPE, HandleSignal);

    AddOnExitCallback([](void)
        {
            g_renderInterface->Shutdown();

            delete g_renderInterface;
            g_renderInterface = nullptr;
        });

    // -RenderOnMainThread option
    if (m_id == g_mainThread)
    {
        Assert(m_isRunning.Load() == false);
        m_isRunning.Store(true);

        // DO NOT call SetCurrentThreadObject() if using RenderOnMainThread

        (*this)();

        return true;
    }

    return Thread::Start();
}

void RenderThread::Stop()
{
    Thread::Stop();

    if (m_id == g_mainThread)
    {
        AssertOnThread(g_mainThread);

        m_isRunning.Store(false);

        OnExit();
    }
}

void RenderThread::Update()
{
    ENGINE_STAT_SCOPE(&g_statRenderUpdate);

    g_renderInterface->BeginFrame(&m_stopRequested);

    if (m_stopRequested.Load())
    {
        return;
    }

    Queue<Scheduler::ScheduledTask> tasks;
    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    Frame* frame = g_renderInterface->PrepareNextFrame();
    Assert(frame != nullptr);

    // Check if any swapchains need to be recreated
    Array<Swapchain*, RenderTempAllocator> swapchains;

    if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
    {
        Swapchain* swapchain = mainWindow->GetSwapchain();

        if (swapchain != nullptr && swapchain->IsCreated())
        {
            swapchains.PushBack(swapchain);
        }
    }

    g_renderInterface->gpuBuffers[GRB_WORLDS]->WriteBufferData(0, GetWorldBufferData(), sizeof(WorldShaderData));
    
    for (Swapchain* swapchain : swapchains)
    {
        g_renderInterface->PrepareSwapchain(swapchain);
    }

    Swapchain* swapchain = swapchains.Any() ? swapchains[0] : nullptr;

    Span<World*> worldsToRender = GetActiveWorlds();

    if (worldsToRender)
    {
        uint32 numViewsRendered = 0;

        RendererBase* mainRenderer = g_renderInterface->globalRenderers[GRT_MAIN][0];
        AssertDebug(mainRenderer != nullptr);

        RenderSetup renderSetup {};

        if (swapchain != nullptr)
        {
            renderSetup.swapchain = swapchain;
            renderSetup.viewport = Viewport { swapchain->GetExtent() };
        }

        for (World* world : worldsToRender)
        {
            AssertDebug(world != nullptr && world->IsReady());

            renderSetup.world = world;

            if (world->GetViews().Size() != 0)
            {
                mainRenderer->RenderFrame(frame, renderSetup);

                numViewsRendered += world->GetViews().Size();
            }
        }

        renderSetup.world = nullptr;

        if (!g_renderInterface->finalPass)
        {
            g_renderInterface->finalPass = HYP_POOL_NEW(g_renderPool, FinalPass);
            g_renderInterface->finalPass->Create();
        }

        g_renderInterface->finalPass->Render(frame, renderSetup);
    }

    // update shared global descriptor sets
    for (DescriptorSet* ds : g_renderInterface->globalDescriptorTable->GetSets()[frame->GetFrameIndex()])
    {
        bool dirty = false;
        ds->UpdateDirtyState(&dirty);

        if (dirty)
            ds->Update();
    }

    g_renderInterface->commandRecorderAllocator.UpdateQueue();

    g_renderInterface->UpdateBuffers(frame);

    g_renderInterface->SubmitCommandBuffers(swapchain);

    if (swapchain != nullptr)
    {
        g_renderInterface->PresentToSwapchain(swapchain);
    }

    g_renderInterface->EndFrame();

    g_renderArena->Reset();
}

void RenderThread::operator()()
{
    AssertDebug(g_renderArena == nullptr);
    g_renderArena = new Arena(RenderArenaSize);

    AddOnExitCallback([]()
        {
            delete g_renderArena;
            g_renderArena = nullptr;
        });

#if HYP_VULKAN
    g_renderInterface = new VulkanRenderInterface();
#elif HYP_DX12
    g_renderInterface = new DX12RenderInterface();
#else
    HYP_FAIL("Not compiled with any rendering backend - cannot initialize renderer!");
#endif

    if (!CheckResult(g_renderInterface->Initialize()))
    {
        HYP_FAIL("Failed to initialize rendering backend");
    }

    g_renderInitSignal.Signal();

    if (m_id != g_mainThread) // !RenderOnMainThread
    {
        while (!m_stopRequested.Load())
        {
            HYP_PROFILE_BEGIN;

            Update();
        }
    }
}

} // namespace Hyperion

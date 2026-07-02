/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Threads/RenderThread.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/EngineMemory.hpp>
#include <Framework/CVarManager.hpp>

#include <Rendering/PostFX.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/FinalPass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/RenderCommand.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/AsyncCompute.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/Device.hpp>
#include <Rendering/Swapchain.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/DebugDrawer.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/FrameLimiter.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Core.hpp>

#include <Asset/Assets.hpp>

#include <Scene/World.hpp>

#include <System/AppContext.hpp>

#include <semaphore>

namespace Hyperion {

extern void HandleSignal(int signum);

extern EngineStatTimer g_statRenderUpdate;

extern ThreadSignal g_renderInitSignal;

namespace PlatformUtils {
ENGINE_API extern bool IsOnBatteryPower();
} // namespace PlatformUtils

static constexpr float IdleMaxFrameRate = 15.0f;
static constexpr float BatteryMaxFrameRate = 30.0f;

CVar<float> g_cvTargetFrameRate("Rendering.TargetFrameRate", 0);                             // 0    = no limit
CVar<bool> g_cvLimitFrameRateOnBatteryPower("Rendering.LimitFrameRateOnBatteryPower", true); // true = enable framerate cap when on battery
CVar<bool> g_cvLimitFrameRateWhenIdle("Rendering.LimitFrameRateWhenIdle", true);             // true = enable framerate cap when idling in standalone
CVar<int> g_cvSkipRendering("Rendering.SkipRendering", 0); // -1 = True, set by SkipRenderingWhenIdle, 0 = False, 1 = True (manually set)
CVar<int> g_cvSkipRenderingWhenIdle("Editor.SkipRenderingWhenIdle", -1); // -1   = set dynamically based on if editor mode

static FrameLimiter g_frameLimiter { 0 };

static bool g_wasFocused = true;

RenderThread::RenderThread()
    : Thread(g_renderThread, ThreadPriorityValue::HIGHEST)
{
    if (g_cvSkipRenderingWhenIdle.Get() < 0)
    {
        g_cvSkipRenderingWhenIdle.Set(EngineGlobals::IsEditor() ? 1 : 0);
    }
}

RenderThread::~RenderThread() = default;

bool RenderThread::Start()
{
    signal(SIGINT, HandleSignal);
    signal(SIGSEGV, HandleSignal);
    // handle integer division by zero
    signal(SIGFPE, HandleSignal);

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
    if (m_id == g_mainThread)
    {
        AssertOnThread(g_mainThread);

        m_isRunning.Store(false);

        OnExit();
    }

    Thread::Stop();
}

void RenderThread::Update()
{
    ENGINE_STAT_SCOPE(&g_statRenderUpdate);

    RI.BeginFrame(&m_stopRequested);

    if (HYP_UNLIKELY(m_stopRequested.LoadVolatile()))
    {
        return;
    }

    ApplicationWindow* mainWindow = g_appContext->GetMainWindow();

    float targetFrameRate = g_cvTargetFrameRate.Get();

    if (g_cvLimitFrameRateWhenIdle.Get() && (!mainWindow || !mainWindow->HasFocus()))
    {
        targetFrameRate = (targetFrameRate > 0) ? MathUtil::Min(targetFrameRate, IdleMaxFrameRate) : IdleMaxFrameRate;

        if (g_wasFocused)
        {
            g_wasFocused = false;
        }
    }
    else if (g_cvLimitFrameRateOnBatteryPower.Get() && PlatformUtils::IsOnBatteryPower())
    {
        targetFrameRate = (targetFrameRate > 0) ? MathUtil::Min(targetFrameRate, BatteryMaxFrameRate) : BatteryMaxFrameRate;
    }
    else
    {
        if (!g_wasFocused)
        {
            g_wasFocused = true;
        }
    }

    { // Execute enqueued tasks
        Array<Scheduler::ScheduledTask, ThreadAllocator> tasks;

        if (m_scheduler->NumEnqueued())
        {
            m_scheduler->AcceptAll(tasks);

            for (auto& task : tasks)
            {
                task.Execute();
            }
        }
    }

    if (EngineGlobals::IsEditor())
    {
        const int skipRenderingValue = g_cvSkipRendering.Get();
        
        if (skipRenderingValue < 1)
        {
            if (g_cvSkipRenderingWhenIdle.Get() > 0)
            {
                const bool skipRenderingThisFrame = (!mainWindow || !mainWindow->HasFocus());

                if ((skipRenderingValue != 0) != skipRenderingThisFrame)
                {
                    g_cvSkipRendering.Set(skipRenderingThisFrame ? -1 : 0);
                }
            }
        }
    }

    Frame* frame = RI.GetCurrentFrame();
    Assert(frame != nullptr);

    // Check if any swapchains need to be recreated
    Array<Swapchain*, RenderTempAllocator> swapchains;

    if (mainWindow != nullptr)
    {
        Swapchain* swapchain = mainWindow->GetSwapchain();

        if (swapchain != nullptr && swapchain->IsCreated())
        {
            swapchains.PushBack(swapchain);
        }
    }

    RI.namedBuffers[NamedBuffer::Worlds].Write(0, sizeof(WorldShaderData), GetWorldBufferData());

    Swapchain* swapchain = nullptr;

    if (swapchains.Any())
    {
        for (Swapchain* swapchain : swapchains)
        {
            RI.PrepareSwapchain(swapchain);
        }

        swapchain = swapchains[0];
    }

    Span<World*> worldsToRender = GetActiveWorlds();

    if (worldsToRender)
    {
        PassBase* mainRenderer = RI.namedPasses[NamedPass::Deferred][0];
        AssertDebug(mainRenderer != nullptr);

        RenderSetup renderSetup {};

        if (swapchain != nullptr)
        {
            renderSetup.swapchain = swapchain;

            const Vec2u swapchainExtent = swapchain->GetExtent();

            const float renderTargetScale = mainWindow->GetRenderTargetScale();
            const Vec2u renderExtent = Vec2u(Vec2f(swapchainExtent) * renderTargetScale);

            renderSetup.viewport = Viewport { renderExtent };
        }

        for (World* world : worldsToRender)
        {
            AssertDebug(world != nullptr);

            renderSetup.world = world;

            if (world->GetViews().Size() != 0)
            {
                mainRenderer->RenderFrame(frame, renderSetup);
            }
        }

        renderSetup.world = nullptr;

        if (!RI.finalPass)
        {
            RI.finalPass = HYP_POOL_NEW(g_renderPool, FinalPass);
            RI.finalPass->Create();
        }

        RI.finalPass->Render(frame, renderSetup);
    }
    
    // update shared global descriptor sets
    for (DescriptorSet* ds : RI.globalDescriptorTable->GetSets()[frame->GetFrameIndex()])
    {
        bool dirty = false;
        ds->UpdateDirtyState(&dirty);

        if (dirty)
        {
            ds->Update();
        }
    }

    CommandBuffer& commandBuffer = *RI.GetCurrentCommandBuffer();

    if (RI.deferredFlushBuffers.Any())
    {
        for (RawBuffer* buffer : RI.deferredFlushBuffers)
        {
            if (!buffer->IsDirty())
            {
                continue;
            }

            buffer->FlushInto(commandBuffer);
        }

        RI.deferredFlushBuffers.Clear();
    }

    RI.commandRecorderAllocator.UpdateQueue();

    RI.WriteCommandBuffer();

    RI.PresentToSwapchain(swapchain);

    RI.EndFrame();

    g_renderArena->Reset();

    // Wait AFTER the frame is rendered to allow sim thread to catch up,
    // as we want buffered data to keep being written even as we wait.
    if (targetFrameRate > 0.0f)
    {
        g_frameLimiter.SetTargetFPS(static_cast<int>(targetFrameRate));
        g_frameLimiter.Wait();
    }
}

void RenderThread::operator()()
{
    const bool isRenderOnMainThread = (m_id == g_mainThread);

    if (!isRenderOnMainThread)
    {
        // Init our thread's stack allocator
        InitThreadAllocator();
    }

    if (!CheckResult(RI.Initialize()))
    {
        HYP_FAIL("Failed to initialize rendering backend");
    }

    g_renderInitSignal.Signal();

    if (!isRenderOnMainThread)
    {
        InitThreadAllocator();

        while (!m_stopRequested.LoadVolatile())
        {
            HYP_PROFILE_BEGIN;

            Update();

            m_threadAllocator->Reset();
        }

        RI.Shutdown();

        g_renderInitSignal.Reset();
    }
    else
    {
        AddOnExitCallback(
            []()
            {
                RI.Shutdown();

                g_renderInitSignal.Reset();
            });
    }
}

} // namespace Hyperion

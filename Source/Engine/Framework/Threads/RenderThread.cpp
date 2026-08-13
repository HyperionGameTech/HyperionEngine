/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Threads/RenderThread.hpp>
#include <Framework/Threads/RenderWorkerThread.hpp>

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

extern EngineStatTimer g_statRenderUpdate;
extern EngineStatTimer g_statTotalStallTime;

EngineStatTimer g_statFrameLimiterWait("Rendering/CPU/FrameLimiterWait");

extern ThreadSignal g_renderInitSignal;

namespace PlatformUtils {
ENGINE_API extern bool IsOnBatteryPower();
} // namespace PlatformUtils

static constexpr float IdleMaxFrameRate = 15.0f;
static constexpr float BatteryMaxFrameRate = 30.0f;

CVar<float> g_cvTargetFrameRate("Rendering.TargetFrameRate", 0);                             // 0    = no limit
CVar<bool> g_cvLimitFrameRateOnBatteryPower("Rendering.LimitFrameRateOnBatteryPower", true); // true = enable framerate cap when on battery
CVar<bool> g_cvLimitFrameRateWhenIdle("Rendering.LimitFrameRateWhenIdle", true);             // true = enable framerate cap when idling in standalone
CVar<int> g_cvSkipRendering("Rendering.SkipRendering", 0);                                   // -1   = True, set by SkipRenderingWhenIdle, 0 = False, 1 = True (manually set)
CVar<int> g_cvSkipRenderingWhenIdle("Editor.SkipRenderingWhenIdle", -1);                     // -1   = set dynamically based on if editor mode

static FrameLimiter g_frameLimiter { 0 };

// Effective frame rate cap for the current frame (0 = unlimited), after idle/battery throttling
// has been folded in. Exposed so other systems (e.g. the debug overlay) can tell whether the
// frame limiter is actually pacing the frame right now, without duplicating this logic.
AtomicVar<uint32> g_currentFrameRateLimit { 0 };

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

    g_currentFrameRateLimit.Set(uint32(MathUtil::Max(targetFrameRate, 0.0f)), MemoryOrder::RELAXED);

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

            const Vec2u& swapchainExtent = swapchain->GetExtent();

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

    if (targetFrameRate > 0.0f)
    {
        g_frameLimiter.SetTargetFPS(int(targetFrameRate));

        ENGINE_STAT_SCOPE(&g_statFrameLimiterWait);
        ENGINE_STAT_SCOPE(&g_statTotalStallTime);

        g_frameLimiter.Wait();
    }
}

static void ResetWorkerThreadAllocators()
{
    if (!g_renderWorkerThreadPool)
    {
        return;
    }

    auto& poolThreads = g_renderWorkerThreadPool->GetThreads();

    for (const UniquePtr<ThreadBase>& taskThread : g_renderWorkerThreadPool->GetThreads())
    {
        Assert(taskThread != nullptr);
        
        static_cast<RenderWorkerThread&>(*taskThread).ResetThreadLinearAllocator();
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

    if (!Check(RI.Initialize()))
    {
        HYP_FAIL("Failed to initialize rendering backend");
    }

    g_renderInitSignal.Signal();

    if (!isRenderOnMainThread)
    {
        while (!m_stopRequested.LoadVolatile())
        {
            HYP_PROFILE_BEGIN;

            Update();

            m_threadAllocator->Reset();
            
            ResetWorkerThreadAllocators();
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

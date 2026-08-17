/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Threads/SimThread.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/Game.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>
#include <Core/Threading/ThreadSignal.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/ClockTimer.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <System/AppContext.hpp>

#include <Input/Event.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>

#include <Asset/Assets.hpp>

#include <Input/InputManager.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Rendering/RenderInterface.hpp>

#ifdef HYP_EDITOR
#include <Editor/EditorState.hpp>
#endif

#if HYP_SCRIPT
#include <Lang/HypScript.hpp>
#endif

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(SimThread);

EngineStatTimer g_statSimUpdate("SimThread");

namespace CoreApi {
CORE_API extern const CommandLineArguments& GetCommandLineArguments();
} // namespace CoreApi

extern void DestroyDetachedScenes();

extern ThreadSignal g_renderInitSignal;

struct LaunchGameAsync
{
    Game* gameInstance;

    explicit LaunchGameAsync(Game* gameInstance)
        : gameInstance(gameInstance)
    {
        Assert(gameInstance != nullptr);
    }

    void operator()()
    {
        if (!EngineGlobals::IsHeadless() && !g_renderInitSignal.IsSignalled())
        {
            // Wait until signalled
            g_simThreadInstance->GetScheduler().Enqueue(std::move(*this), TaskEnqueueFlags::FIRE_AND_FORGET);
            return;
        }

        InitObject(gameInstance);

        gameInstance->Initialize();

        g_simThreadInstance->m_gameInstance = gameInstance;
    }
};

#pragma region SimThread

SimThread::SimThread()
    : Thread(g_simThread, ThreadPriorityValue::HIGHEST),
      m_gameInstance(nullptr)
{
}

SimThread::~SimThread() = default;

bool SimThread::Start()
{
    AddOnExitCallback(DestroyDetachedScenes);

    // -SimulateOnMainThread option
    if (m_id == g_mainThread)
    {
        Assert(m_isRunning.Load() == false);
        m_isRunning.Store(true);

        // DO NOT call SetCurrentThreadObject() if using -SimulateOnMainThread

        (*this)();

        return true;
    }

    return Thread::Start();
}

void SimThread::Stop()
{
    Thread::Stop();

    if (m_id == g_mainThread)
    {
        AssertOnThread(g_mainThread);

        m_isRunning.Store(false);

        OnExit();
    }
}

void SimThread::SetGameInstance(Game* gameInstance)
{
    if (IsRunning())
    {
        if (IsOnThread(m_id))
        {
            LaunchGameAsync { gameInstance }();
        }
        else
        {
            HYP_LOG(SimThread, Verbose, "Setting game instance from thread {} (async) ...", CurrentThreadId().GetName());

            GetScheduler().Enqueue(LaunchGameAsync(gameInstance), TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
    else
    {
        m_gameInstance = gameInstance;
    }
}

void SimThread::Update()
{
    ENGINE_STAT_SCOPE(&g_statSimUpdate);

    m_counter.NextTick();

    g_assetManager->Update(m_counter.delta);
    g_streamingManager->Update(m_counter.delta);

#ifdef HYP_EDITOR
    g_editorState->Update(m_counter.delta);
#endif

    if constexpr (UseRingBuffer)
    {
        BeginSimRenderSyncBlock(&m_stopRequested);
    }

    if (HYP_UNLIKELY(m_stopRequested.LoadVolatile()))
    {
        return;
    }

    { // execute posted tasks
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

    if (m_gameInstance != nullptr)
    {
        // game instance should be null if not launched yet
        AssertDebug(m_gameInstance->m_isLaunched.Get(MemoryOrder::RELAXED));

        m_gameInstance->m_gameState.deltaTime = m_counter.delta;
    }

    if (g_appContext.IsValid())
    {
        if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
        {
            Event event;
            while (mainWindow->GetInputManager()->PollEvent(event))
            {
                if (m_gameInstance != nullptr)
                {
                    m_gameInstance->HandleEvent(std::move(event));
                }
            }
        }
    }

    g_engineDriver->Simulate(m_counter.delta, m_gameInstance);

    if constexpr (UseRingBuffer)
    {
        EndSimRenderSyncBlock();
    }
    
    g_sceneArena->Reset();
}

void SimThread::operator()()
{
    const bool isSimulateOnMainThread = (m_id == g_mainThread);

    if (!isSimulateOnMainThread)
    {
        InitThreadAllocator();
    }

#if HYP_SCRIPT
    HypScript::Initialize();
    AddOnExitCallback(&HypScript::Shutdown);
#endif

    if (m_gameInstance != nullptr)
    {
        LaunchGameAsync { m_gameInstance }();
    }

    if (!isSimulateOnMainThread)
    {
        g_renderInitSignal.Wait();

        while (!m_stopRequested.LoadVolatile())
        {
            HYP_PROFILE_BEGIN;

            Update();

            m_threadAllocator->Reset();
        }
    }
}

#pragma endregion SimThread

} // namespace Hyperion

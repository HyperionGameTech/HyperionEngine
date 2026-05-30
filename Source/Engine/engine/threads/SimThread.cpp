/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <engine/threads/SimThread.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/Game.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/ThreadSignal.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/cli/CommandLine.hpp>

#include <system/AppContext.hpp>

#include <input/Event.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <asset/Assets.hpp>

#include <input/InputManager.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/DebugDrawer.hpp>

#if HYP_EDITOR
#include <editor/EditorState.hpp>
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
        if (!g_renderInitSignal.IsSignalled())
        {
            // Wait until signalled
            g_simThreadInstance->GetScheduler().Enqueue(std::move(*this), TaskEnqueueFlags::FIRE_AND_FORGET);
            return;
        }

        InitObject(gameInstance);

        if (!gameInstance->m_isLaunched.Get(MemoryOrder::RELAXED))
        {
            gameInstance->Initialize();

            gameInstance->OnLaunch();
            gameInstance->m_isLaunched.Set(true, MemoryOrder::RELEASE);

            const Handle<World>& world = gameInstance->GetWorld();
            Assert(world.IsValid());

            g_engineDriver->AddWorld(world);

            gameInstance->OnLaunched();
        }

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

    static const bool s_isDetached = CoreApi::GetCommandLineArguments()["Detached"].ToBool();

    m_counter.NextTick();

    BeginFrameSim(&m_stopRequested);

    if (HYP_UNLIKELY(m_stopRequested.Load()))
    {
        return;
    }

    // execute posted tasks
    Array<Scheduler::ScheduledTask, SceneAllocator> tasks;
    if (uint32 numEnqueued = m_scheduler->NumEnqueued())
    {
        m_scheduler->AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.PopBack().Execute();
        }
    }

    g_assetManager->Update(m_counter.delta);

    if (m_gameInstance != nullptr)
    {
        // game instance should be null if not launched yet
        AssertDebug(m_gameInstance->m_isLaunched.Get(MemoryOrder::RELAXED));

        m_gameInstance->m_gameState.deltaTime = m_counter.delta;
    }

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

#if HYP_EDITOR
    g_editorState->Update(m_counter.delta);
#endif

    g_engineDriver->UpdateSim(m_counter.delta);

    if (m_gameInstance != nullptr)
    {
        m_gameInstance->OnUpdate(m_counter.delta);

        m_gameInstance->m_gameState.gameTime += m_counter.delta;
    }

    DebugDrawer::GetInstance().Update();

    EndFrameSim();
}

void SimThread::operator()()
{
#if HYP_SCRIPT
    HypScript::Initialize();
    AddOnExitCallback(&HypScript::Shutdown);
#endif

    if (m_gameInstance != nullptr)
    {
        LaunchGameAsync { m_gameInstance }();
    }

    // Handle -SimulateOnMainThread
    if (m_id != g_mainThread)
    {
        while (!m_stopRequested.Load())
        {
            HYP_PROFILE_BEGIN;

            Update();
        }
    }
}

#pragma endregion SimThread

} // namespace Hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/threads/SimThread.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/DebugDrawer.hpp>

#include <core/threading/Threads.hpp>

#include <core/math/MathUtil.hpp>

#include <game/Game.hpp>

#include <system/AppContext.hpp>
#include <input/Event.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <asset/Assets.hpp>

#include <input/InputManager.hpp>

#include <rendering/RenderInterface.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorState.hpp>
#endif

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(SimThread);

EngineStatTimer g_simTimer("SimThread/Update");

struct LaunchGameAsync
{
    Handle<Game> gameInstance;
    bool success;

    explicit LaunchGameAsync(const Handle<Game>& gameInstance)
        : gameInstance(gameInstance),
          success(false)
    {
        Assert(gameInstance != nullptr);
    }

    void operator()()
    {
        // ensure instance is still the one we are launching, otherwise, cancel the task
        if (!RenderApi::IsInit())
        {
            HYP_LOG(SimThread, Info, "Delaying game launch until Render API is initialized...");

            g_simThreadInstance->GetScheduler().Enqueue(*this, TaskEnqueueFlags::FIRE_AND_FORGET);

            return;
        }

        InitObject(gameInstance);

        if (!gameInstance->m_isLaunched.Get(MemoryOrder::RELAXED))
        {
            gameInstance->OnLaunch();
            gameInstance->m_isLaunched.Set(true, MemoryOrder::RELEASE);

            gameInstance->OnLaunched();
        }

        g_simThreadInstance->m_game = gameInstance;

        success = true;
    }
};

#pragma region SimThread

SimThread::SimThread()
    : Thread(g_simThread, ThreadPriorityValue::HIGHEST)
{
}

bool SimThread::Start()
{
    // -SimulateOnMainThread option
    if (m_id == g_mainThread)
    {
        Assert(m_isRunning.Exchange(true, MemoryOrder::ACQUIRE_RELEASE) == false);

        // DO NOT call SetCurrentThreadObject() if using -SimulateOnMainThread

        (*this)();

        return true;
    }

    return Thread::Start();
}

void SimThread::SetGame(const Handle<Game>& game)
{
    auto impl = [this, game = game]()
    {
        if (m_game == game)
        {
            // same instance, nothing to do
            return;
        }

        LaunchGameAsync launchTask { game };
        launchTask();

        if (!launchTask.success)
        {
            GetScheduler().Enqueue(std::move(launchTask), TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    };

    if (IsOnThread(m_id) && IsRunning())
    {
        impl();
    }
    else
    {
        HYP_LOG(SimThread, Info, "Setting game instance from thread {} (async) ...", CurrentThreadId().GetName());

        GetScheduler().Enqueue(std::move(impl), TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void SimThread::Update()
{
    ENGINE_STAT_SCOPE(&g_simTimer);
    HYP_PROFILE_BEGIN;

    m_counter.NextTick();

    RenderApi::BeginFrameSim();

    // execute posted tasks
    Queue<Scheduler::ScheduledTask> tasks;
    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    g_assetManager->Update(m_counter.delta);

    if (m_game != nullptr)
    {
        // game instance should be null if not launched yet
        AssertDebug(m_game->m_isLaunched.Get(MemoryOrder::RELAXED));
    }

    if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
    {
        Event event;
        while (mainWindow->GetInputManager()->PollEvent(event))
        {
            if (m_game != nullptr)
            {
                m_game->HandleEvent(std::move(event));
            }
        }
    }

#ifdef HYP_EDITOR
    g_editorState->Update(m_counter.delta);
#endif

    g_engineDriver->UpdateSim(m_counter.delta);

    if (m_game != nullptr)
    {
        m_game->OnUpdate(m_counter.delta);
    }

    g_engineDriver->GetDebugDrawer()->Update(m_counter.delta);

    RenderApi::EndFrameSim();
}

void SimThread::operator()()
{
    // create fallback world
    Handle<World> defaultWorld = CreateObject<World>(NAME("DefaultWorld"), WorldFlags::NONE);
    InitObject(defaultWorld);
    g_engineDriver->SetDefaultWorld(defaultWorld);

    // Handle -SimulateOnMainThread
    if (m_id != g_mainThread)
    {
        while (!m_stopRequested.Get(MemoryOrder::RELAXED))
        {
            Update();
        }
    }
}

#pragma endregion SimThread

} // namespace hyperion
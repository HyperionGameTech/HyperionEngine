/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/threads/SimThread.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/DebugDrawer.hpp>

#include <core/threading/Threads.hpp>

#include <core/math/MathUtil.hpp>

#include <engine/Game.hpp>

#include <system/AppContext.hpp>
#include <input/Event.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <asset/Assets.hpp>

#include <input/InputManager.hpp>

#include <rendering/RenderInterface.hpp>

#if HYP_EDITOR
#include <editor/EditorState.hpp>
#endif

#if HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(SimThread);

extern void DestroyDetachedScenes();

EngineStatTimer g_simTimer("SimThread/Update");

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
        InitObject(gameInstance);

        if (!gameInstance->m_isLaunched.Get(MemoryOrder::RELAXED))
        {
            gameInstance->OnLaunch();
            gameInstance->m_isLaunched.Set(true, MemoryOrder::RELEASE);

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

bool SimThread::Start()
{
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

void SimThread::SetGameInstance(Game* gameInstance)
{
    if (IsOnThread(m_id) && IsRunning())
    {
        LaunchGameAsync { gameInstance }();
    }
    else
    {
        HYP_LOG(SimThread, Info, "Setting game instance from thread {} (async) ...", CurrentThreadId().GetName());

        GetScheduler().Enqueue(LaunchGameAsync(gameInstance), TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void SimThread::Update()
{
    ENGINE_STAT_SCOPE(&g_simTimer);
    HYP_PROFILE_BEGIN;

    m_counter.NextTick();

    BeginFrameSim();

    // execute posted tasks
    Array<Scheduler::ScheduledTask, SceneAllocator> tasks;
    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

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

#ifdef HYP_EDITOR
    g_editorState->Update(m_counter.delta);
#endif

    g_engineDriver->UpdateSim(m_counter.delta);

    if (m_gameInstance != nullptr)
    {
        m_gameInstance->OnUpdate(m_counter.delta);

        m_gameInstance->m_gameState.gameTime += m_counter.delta;
    }

    g_engineDriver->GetDebugDrawer()->Update(m_counter.delta);

    EndFrameSim();
}

void SimThread::operator()()
{
#if HYP_SCRIPT
    HypScript::GetInstance().Initialize();
#endif

    // create fallback world
    Handle<World> defaultWorld = MakeHandle<World>(NAME("DefaultWorld"), WorldFlags::NONE);
    InitObject(defaultWorld);
    g_engineDriver->SetDefaultWorld(defaultWorld);

    // Handle -SimulateOnMainThread
    if (m_id != g_mainThread)
    {
        while (!m_stopRequested.Load())
        {
            Update();
        }
    }

    DestroyDetachedScenes();
}

#pragma endregion SimThread

} // namespace Hyperion

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
    if (IsRunning())
    {
        auto impl = [this, game = game]()
        {
            if (m_game == game)
            {
                // same instance, nothing to do
                return;
            }

            m_game = game;

            if (m_game != nullptr)
            {
                InitObject(m_game);

                // m_isLaunched is only ever modified from this thread
                // so we use RELAXED for the load here
                if (!m_game->m_isLaunched.Get(MemoryOrder::RELAXED))
                {
                    m_game->OnLaunch();
                    m_game->m_isLaunched.Set(true, MemoryOrder::RELEASE);

                    m_game->OnLaunched();
                }
            }
        };

        if (IsOnThread(m_id))
        {
            impl();
        }
        else
        {
            HYP_LOG(SimThread, Info, "Setting game instance from thread {} (async) ...", CurrentThreadId().GetName());

            GetScheduler().Enqueue(std::move(impl), TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        return;
    }

    m_game = game;
}

void SimThread::Update()
{
    ENGINE_STAT_SCOPE(&g_simTimer);

    HYP_PROFILE_BEGIN;

    RenderApi::BeginFrameSim();

    m_counter.NextTick();

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

    while (!RenderApi::IsInit())
    {
        ThreadSleep(10); // wait for rendering subsystem to initialize before launching game
    }

    HYP_LOG(SimThread, Info, "Render api initialized, starting game loop...");

    if (m_game != nullptr)
    {
        InitObject(m_game);

        if (!m_game->m_isLaunched.Get(MemoryOrder::RELAXED))
        {
            m_game->OnLaunch();
            m_game->m_isLaunched.Set(true, MemoryOrder::RELEASE);

            m_game->OnLaunched();
        }
    }

    // Handle -SimulateOnMainThread
    if (m_id != g_mainThread)
    {
        while (!m_stopRequested.Get(MemoryOrder::RELAXED))
        {
            Update();
        }
    }
}

} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/threads/GameThread.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/DebugDrawer.hpp>

#include <core/threading/Threads.hpp>

#include <core/math/MathUtil.hpp>

#include <game/Game.hpp>

#include <util/GameCounter.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <asset/Assets.hpp>

#include <rendering/RenderGlobalState.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorState.hpp>
#endif

// #define HYP_GAME_THREAD_LOCKED 1

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(GameThread);

EngineStatTimer g_gameThreadUpdateTimer("GameThread/Update");

GameThread::GameThread()
    : Thread(g_gameThread, ThreadPriorityValue::HIGHEST)
{
}

void GameThread::SetGame(const Handle<Game>& game)
{
    if (IsRunning())
    {
        Task<void> future;

        GetScheduler().Enqueue([this, game = game, promise = future.Promise()]()
            {
                m_game = game;

                Assert(m_game != nullptr);

                InitObject(m_game);

                promise->Fulfill();
            });

        future.Await();

        return;
    }

    m_game = game;
}

void GameThread::operator()()
{
    // create fallback world
    Handle<World> defaultWorld = CreateObject<World>(NAME("DefaultWorld"), WorldFlags::NONE);
    InitObject(defaultWorld);
    g_engineDriver->SetDefaultWorld(defaultWorld);

    GameCounter counter;

    Assert(m_game != nullptr);
    InitObject(m_game);

    m_game->OnLaunch();
    m_game->m_isLaunched.Set(true, MemoryOrder::RELEASE);

    // @TODO Make this less fragile
    while (!RenderApi::IsInit())
    {
        ThreadSleep(10); // wait for rendering subsystem to initialize
    }

    Queue<Scheduler::ScheduledTask> tasks;
    SystemEvents events;

    while (!m_stopRequested.Get(MemoryOrder::RELAXED))
    {
        ENGINE_STAT_SCOPE(&g_gameThreadUpdateTimer);

#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting())
        {
            continue;
        }
#endif

        HYP_PROFILE_BEGIN;

        RenderApi::BeginFrame_GameThread();

        counter.NextTick();

        // execute posted tasks
        if (uint32 numEnqueued = m_scheduler.NumEnqueued())
        {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any())
            {
                tasks.Pop().Execute();
            }
        }

        g_assetManager->Update(counter.delta);

        if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
        {
            if (mainWindow->GetInputEventSink().Poll(events))
            {
                for (SystemEvent& event : events)
                {
                    g_inputManager->CheckEvent(&event);

                    m_game->HandleEvent(std::move(event));
                }
            }
        }

        events.Clear();

#ifdef HYP_EDITOR
        g_editorState->Update(counter.delta);
#endif

        g_engineDriver->GameThreadUpdate(counter.delta);

        m_game->OnUpdate(counter.delta);

        g_engineDriver->GetDebugDrawer()->Update(counter.delta);

        RenderApi::EndFrame_GameThread();
    }

    // flush scheduler
    m_scheduler.Flush([](auto& operation)
        {
            operation.Execute();
        });
}

} // namespace hyperion
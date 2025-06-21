/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <GameThread.hpp>
#include <Game.hpp>
#include <GameCounter.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Defines.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <scene/World.hpp>

#include <asset/Assets.hpp>

#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <Engine.hpp>

// #define HYP_GAME_THREAD_LOCKED 1

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(GameThread);

static constexpr float game_thread_target_ticks_per_second = 120.0f;

GameThread::GameThread(const Handle<AppContextBase>& app_context)
    : Thread(g_game_thread, ThreadPriorityValue::HIGHEST),
      m_app_context(app_context)
{
    AssertThrow(m_app_context.IsValid());
}

void GameThread::SetGame(const Handle<Game>& game)
{
    if (IsRunning())
    {
        Task<void> future;

        GetScheduler().Enqueue([this, game = game, promise = future.Promise()]()
            {
                m_game = game;

                if (m_game.IsValid())
                {
                    m_game->SetAppContext(m_app_context);

                    InitObject(m_game);
                }

                promise->Fulfill();
            });

        future.Await();

        return;
    }

    m_game = game;
}

void GameThread::operator()()
{
    uint32 num_frames = 0;
    float delta_time_accum = 0.0f;

#if HYP_GAME_THREAD_LOCKED
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);
#else
    GameCounter counter;
#endif

    // temp. commented out
    // g_engine->GetDebugDrawer()->Initialize();

    if (m_game.IsValid())
    {
        m_game->SetAppContext(m_app_context);

        InitObject(m_game);
    }

    Queue<Scheduler::ScheduledTask> tasks;
    Array<SystemEvent> events;

    while (!m_stop_requested.Get(MemoryOrder::RELAXED))
    {
#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting())
        {
            continue;
        }
#endif

        HYP_PROFILE_BEGIN;

        counter.NextTick();

        delta_time_accum += counter.delta;
        num_frames++;

        if (delta_time_accum >= 1.0f)
        {
            HYP_LOG(GameThread, Debug, "Game thread ticks per second: {}", 1.0f / (delta_time_accum / float(num_frames)));

            delta_time_accum = 0.0f;
            num_frames = 0;
        }

        g_engine->GetDebugDrawer()->Update(counter.delta);

        AssetManager::GetInstance()->Update(counter.delta);

        if (m_app_context->GetMainWindow()->GetInputEventSink().Poll(events))
        {
            for (SystemEvent& event : events)
            {
                m_app_context->GetInputManager()->CheckEvent(&event);

                if (m_game.IsValid())
                {
                    m_game->HandleEvent(std::move(event));
                }
            }

            events.Clear();
        }

        if (uint32 num_enqueued = m_scheduler.NumEnqueued())
        {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any())
            {
                tasks.Pop().Execute();
            }
        }

        BeginFrame_GameThread();

        if (m_game.IsValid())
        {
            m_game->Update(counter.delta);
        }

        EndFrame_GameThread();
    }

    // flush scheduler
    m_scheduler.Flush([](auto& operation)
        {
            operation.Execute();
        });
}

} // namespace hyperion
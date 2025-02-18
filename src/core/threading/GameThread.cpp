/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/GameThread.hpp>
#include <core/logging/Logger.hpp>
#include <core/Defines.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <asset/Assets.hpp>

#include <math/MathUtil.hpp>

#include <Game.hpp>
#include <GameCounter.hpp>

#define HYP_GAME_THREAD_LOCKED 0

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(GameThread);

static constexpr float game_thread_target_ticks_per_second = 60.0f;

GameThread::GameThread()
    : Thread(Threads::GetStaticThreadID(ThreadName::THREAD_GAME), ThreadPriorityValue::HIGHEST),
      m_is_running { false },
      m_stop_requested { false }
{
}

void GameThread::Stop()
{
    m_stop_requested.Set(true, MemoryOrder::RELAXED);
}

void GameThread::operator()(Game *game)
{
    uint32 num_frames = 0;
    float delta_time_accum = 0.0f;

#if HYP_GAME_THREAD_LOCKED
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);
#else
    GameCounter counter;
#endif

    m_is_running.Set(true, MemoryOrder::RELAXED);
    
    Queue<Scheduler::ScheduledTask> tasks;

    while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting()) {
            continue;
        }
#endif
        
        HYP_PROFILE_BEGIN;

        counter.NextTick();

        delta_time_accum += counter.delta;
        num_frames++;

        if (delta_time_accum >= 1.0f) {
            HYP_LOG(GameThread, Debug, "Game thread ticks per second: {}", 1.0f / (delta_time_accum / float(num_frames)));

            delta_time_accum = 0.0f;
            num_frames = 0;
        }

        AssetManager::GetInstance()->Update(counter.delta);

        if (uint32 num_enqueued = m_scheduler.NumEnqueued()) {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any()) {
                tasks.Pop().Execute();
            }
        }
        
        game->Update(counter.delta);
    }

    // flush scheduler
    m_scheduler.Flush([](auto &operation)
    {
        operation.Execute();
    });

    game->Teardown();

    m_is_running.Set(false, MemoryOrder::RELAXED);
}

} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/GameThread.hpp>
#include <core/logging/Logger.hpp>
#include <core/Defines.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <asset/Assets.hpp>

#include <math/MathUtil.hpp>

#include <Engine.hpp>
#include <Game.hpp>
#include <GameCounter.hpp>

#define HYP_GAME_THREAD_LOCKED 1

namespace hyperion {

HYP_DEFINE_LOG_CHANNEL(GameThread);

static constexpr float game_thread_target_ticks_per_second = 60.0f;

GameThread::GameThread()
    : Thread(Threads::thread_ids.At(ThreadName::THREAD_GAME)),
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
#if HYP_GAME_THREAD_LOCKED
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);
#else
    GameCounter counter;
#endif

    m_is_running.Set(true, MemoryOrder::RELAXED);

    game->Init();
    
    Queue<Scheduler::ScheduledTask> tasks;

    while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting()) {
            continue;
        }
#endif

        counter.NextTick();
        
        HYP_PROFILE_BEGIN;

        g_asset_manager->Update(counter.delta);
        
        game->Update(counter.delta);

        if (auto num_enqueued = m_scheduler.NumEnqueued()) {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any()) {
                tasks.Pop().Execute(counter.delta);
            }
        }
    }

    // flush scheduler
    m_scheduler.Flush([](auto &fn)
    {
        fn(MathUtil::epsilon_f);
    });

    game->Teardown();

    m_is_running.Set(false, MemoryOrder::RELAXED);
}

} // namespace hyperion
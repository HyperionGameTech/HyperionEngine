#include <Engine.hpp>
#include <Game.hpp>
#include <GameCounter.hpp>
#include <util/Defines.hpp>
#include <math/MathUtil.hpp>

#define HYP_GAME_THREAD_LOCKED 1

namespace hyperion::v2 {

static constexpr float game_thread_target_ticks_per_second = 120.0f;

GameThread::GameThread()
    : Thread(Threads::thread_ids.At(THREAD_GAME)),
      m_is_running { false }
{
}

void GameThread::Stop()
{
    m_is_running.Set(false, MemoryOrder::RELAXED);
}

void GameThread::operator()(Game *game)
{
#if HYP_GAME_THREAD_LOCKED
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);
#else
    GameCounter counter;
#endif

    m_is_running.Set(true, MemoryOrder::RELAXED);

    game->InitGame();
    
    Queue<Scheduler::ScheduledTask> tasks;

    while (m_is_running.Get(MemoryOrder::RELAXED)) {
        if (auto num_enqueued = m_scheduler.NumEnqueued()) {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any()) {
                tasks.Pop().Execute(counter.delta);
            }
        }

#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting()) {
            continue;
        }
#endif

        counter.NextTick();
        
        game->Update(counter.delta);
    }

    // flush scheduler
    m_scheduler.Flush([](auto &fn)
    {
        fn(MathUtil::epsilon_f);
    });

    game->Teardown();
}

} // namespace hyperion::v2
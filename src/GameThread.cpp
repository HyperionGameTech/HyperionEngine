#include "GameThread.hpp"
#include "Engine.hpp"
#include "Game.hpp"
#include "GameCounter.hpp"
#include <util/Defines.hpp>
#include <math/MathUtil.hpp>

#define HYP_GAME_THREAD_LOCKED 1

namespace hyperion::v2 {

static constexpr float game_thread_target_ticks_per_second = 60.0f;

GameThread::GameThread()
    : Thread(Threads::thread_ids.At(THREAD_GAME)),
      m_is_running { false }
{
}

void GameThread::operator()(Engine *engine, Game *game, SystemWindow *window)
{
#ifdef HYP_WINDOWS
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif

#if HYP_GAME_THREAD_LOCKED
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);
#else
    GameCounter counter;
#endif

    m_is_running.store(true, std::memory_order_relaxed);

    game->InitGame(engine);

    while (engine->m_running.load(std::memory_order_relaxed)) {
        if (auto num_enqueued = m_scheduler.NumEnqueued()) {
            m_scheduler.Flush([last_delta = counter.delta](auto &fn) {
                fn(last_delta);
            });
        }

#if HYP_GAME_THREAD_LOCKED
        if (counter.Waiting()) {
            continue;
        }
#endif

        counter.NextTick();
        
        game->Update(engine, counter.delta);
    }

    // flush scheduler
    m_scheduler.Flush([](auto &fn) {
        fn(MathUtil::epsilon<GameCounter::TickUnit>);
    });

    game->Teardown(engine);

    m_is_running.store(false, std::memory_order_relaxed);
}

} // namespace hyperion::v2
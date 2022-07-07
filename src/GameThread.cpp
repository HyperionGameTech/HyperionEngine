#include "GameThread.hpp"
#include "Engine.hpp"
#include "Game.hpp"
#include "GameCounter.hpp"

#define HYP_GAME_THREAD_LOCKED 1

namespace hyperion::v2 {

static constexpr float game_thread_target_ticks_per_second = 60.0f;

GameThread::GameThread()
    : Thread(Threads::thread_ids.At(THREAD_GAME))
{
}

void GameThread::operator()(Engine *engine, Game *game, SystemWindow *window)
{
#if HYP_GAME_THREAD_LOCKED
    LockstepGameCounter counter(1.0f / game_thread_target_ticks_per_second);
#else
    GameCounter counter;
#endif

    game->OnPostInit(engine);

    while (engine->m_running) {
#if HYP_GAME_THREAD_LOCKED
        while (counter.Waiting()) {
            /* wait */
        }
#endif

        counter.NextTick();
        
        if (auto num_enqueued = m_scheduler->NumEnqueued()) {
            m_scheduler->Flush([delta = counter.delta](auto &fn) {
                fn(delta);
            });
        }
        
        game->Logic(engine, counter.delta);
    }

    game->Teardown(engine);
}

} // namespace hyperion::v2
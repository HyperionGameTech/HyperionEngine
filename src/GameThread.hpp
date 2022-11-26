#ifndef HYPERION_V2_GAME_THREAD_H
#define HYPERION_V2_GAME_THREAD_H

#include "GameCounter.hpp"

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>

#include <atomic>

namespace hyperion::v2 {

class Engine;
class Game;

class GameThread
    : public Thread<Scheduler<Task<void, GameCounter::TickUnit>>,  Game *>
{
public:
    GameThread();

    /*! \brief Atomically load the boolean value indicating that this thread is actively running */
    bool IsRunning() const
        { return m_is_running.load(std::memory_order_relaxed); }

private:
    virtual void operator()(Game *game) override;

    std::atomic_bool m_is_running;
};

} // namespace hyperion::v2

#endif
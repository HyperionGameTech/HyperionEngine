#ifndef HYPERION_V2_GAME_THREAD_H
#define HYPERION_V2_GAME_THREAD_H

#include "GameCounter.hpp"

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>

#include <atomic>

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

class Engine;
class Game;

class GameThread : public Thread<Scheduler<ScheduledFunction<void, GameCounter::TickUnit>>,
                                 Engine *, Game *, SystemWindow *>
{
public:
    GameThread();

    bool IsRunning() const
        { return m_is_running.load(std::memory_order_relaxed); }

private:
    virtual void operator()(Engine *engine, Game *game, SystemWindow *window) override;

    std::atomic_bool m_is_running;
};

} // namespace hyperion::v2

#endif
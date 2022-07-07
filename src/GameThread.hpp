#ifndef HYPERION_V2_GAME_THREAD_H
#define HYPERION_V2_GAME_THREAD_H

#include "GameCounter.hpp"

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>

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

private:
    virtual void operator()(Engine *engine, Game *game, SystemWindow *window) override;
};

} // namespace hyperion::v2

#endif
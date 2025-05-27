/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAME_THREAD_HPP
#define HYPERION_GAME_THREAD_HPP

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <GameCounter.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

class Engine;
class Game;

class GameThread final : public Thread<Scheduler, Game*>
{
public:
    GameThread();

    /*! \brief Atomically load the boolean value indicating that this thread is actively running */
    bool IsRunning() const
    {
        return m_is_running.Get(MemoryOrder::RELAXED);
    }

    void Stop();

private:
    virtual void operator()(Game* game) override;

    AtomicVar<bool> m_is_running;
    AtomicVar<bool> m_stop_requested;
};

} // namespace hyperion

#endif
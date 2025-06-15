/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAME_THREAD_HPP
#define HYPERION_GAME_THREAD_HPP

#include <core/Handle.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <GameCounter.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

namespace sys {
class AppContextBase;
} // namespace sys

using sys::AppContextBase;

class Engine;
class Game;

class GameThread final : public Thread<Scheduler>
{
public:
    GameThread(const Handle<AppContextBase>& app_context);

    void SetGame(const Handle<Game>& game);

private:
    virtual void operator()() override;

    Handle<AppContextBase> m_app_context;
    Handle<Game> m_game;
};

} // namespace hyperion

#endif
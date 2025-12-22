/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <util/GameCounter.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(GameThread);

class AppContextBase;
class Game;

class GameThread final : public Thread<Scheduler>
{
public:
    GameThread();

    bool Start();

    void SetGame(const Handle<Game>& game);

    void Update();

private:
    virtual void operator()() override;

    Handle<Game> m_game;
    GameCounter m_counter;
};

} // namespace hyperion
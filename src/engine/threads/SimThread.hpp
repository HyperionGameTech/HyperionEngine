/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/utilities/ClockTimer.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(SimThread);

class AppContextBase;
class Game;

class SimThread final : public Thread<Scheduler>
{
    friend struct LaunchGameAsync;

public:
    SimThread();

    bool Start();

    void SetGameInstance(Game* gameInstance);

    HYP_FORCE_INLINE Game* GetGameInstance() const
    {
        return m_gameInstance;
    }

    void Update();

private:
    virtual void operator()() override;

    Game* m_gameInstance;
    ClockTimer m_counter;
};

} // namespace Hyperion
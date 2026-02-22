/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/threading/Thread.hpp>
#include <Core/threading/Scheduler.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <Core/utilities/ClockTimer.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(SimThread);

class AppContextBase;
class Game;

class SimThread final : public Thread<Scheduler>
{
    friend struct LaunchGameAsync;

public:
    SimThread();
    ~SimThread() override;

    bool Start();
    void Stop() override;

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
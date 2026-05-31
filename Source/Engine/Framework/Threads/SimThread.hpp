/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Core/Utilities/ClockTimer.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(SimThread);

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

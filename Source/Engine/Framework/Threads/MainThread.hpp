/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

namespace Hyperion {

class MainThread final : public Thread<Scheduler>
{
public:
    MainThread();
    ~MainThread();

    bool Start();
    virtual void Stop() override;

    void Update();

private:
    virtual void operator()() override;
};

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

namespace Hyperion {

class RenderThread final : public Thread<Scheduler>
{
public:
    RenderThread();
    ~RenderThread() override;

    bool Start();
    void Stop() override;

    void Update();

private:
    virtual void operator()() override;
};

} // namespace Hyperion

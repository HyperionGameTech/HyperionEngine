/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Scheduler.hpp>

namespace Hyperion {

class RTCServer;

class ENGINE_API RTCServerThread final : public Thread<Scheduler, RTCServer*>
{
public:
    RTCServerThread();
    virtual ~RTCServerThread() override = default;

private:
    virtual void operator()(RTCServer*) override;
};

} // namespace Hyperion

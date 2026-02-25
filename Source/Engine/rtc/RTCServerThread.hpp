/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/threading/Thread.hpp>
#include <Core/threading/Scheduler.hpp>

namespace Hyperion {

class RTCServer;

class HYP_API RTCServerThread final : public Thread<Scheduler, RTCServer*>
{
public:
    RTCServerThread();
    virtual ~RTCServerThread() override = default;

private:
    virtual void operator()(RTCServer*) override;
};

} // namespace Hyperion

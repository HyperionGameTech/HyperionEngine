/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

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

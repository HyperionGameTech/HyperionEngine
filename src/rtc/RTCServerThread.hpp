/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RTC_SERVER_THREAD_HPP
#define HYPERION_RTC_SERVER_THREAD_HPP

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>

namespace hyperion {

class RTCServer;

class HYP_API RTCServerThread final : public Thread<Scheduler, RTCServer*>
{
public:
    RTCServerThread();
    virtual ~RTCServerThread() override = default;

private:
    virtual void operator()(RTCServer*) override;
};

} // namespace hyperion

#endif // HYPERION_RTC_SERVER_THREAD_HPP
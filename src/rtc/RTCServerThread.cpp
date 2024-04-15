/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCServerThread.hpp>

#include <core/lib/Queue.hpp>

namespace hyperion::v2 {

RTCServerThread::RTCServerThread()
    : Thread(HYP_NAME(RTCServerThread))
{
}

void RTCServerThread::Stop()
{
    m_is_running.Set(false, MemoryOrder::RELAXED);
}

void RTCServerThread::operator()(RTCServer *server)
{
    m_is_running.Set(true, MemoryOrder::RELAXED);
    
    Queue<Scheduler::ScheduledTask> tasks;

    while (m_is_running.Get(MemoryOrder::RELAXED)) {
        if (auto num_enqueued = m_scheduler.NumEnqueued()) {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any()) {
                tasks.Pop().Execute();
            }
        }
    }

    // flush scheduler
    m_scheduler.Flush([](auto &fn) {
        fn();
    });
}

} // namespace hyperion::v2
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCServerThread.hpp>

#include <core/containers/Queue.hpp>

namespace hyperion {

RTCServerThread::RTCServerThread()
    : Thread(NAME("RTCServerThread"))
{
}

void RTCServerThread::operator()(RTCServer* server)
{
    Queue<Scheduler::ScheduledTask> tasks;

    while (!m_stop_requested.Get(MemoryOrder::RELAXED))
    {
        if (uint32 num_enqueued = m_scheduler.NumEnqueued())
        {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any())
            {
                tasks.Pop().Execute();
            }
        }
    }

    // flush scheduler
    m_scheduler.Flush([](auto& operation)
        {
            operation.Execute();
        });
}

} // namespace hyperion
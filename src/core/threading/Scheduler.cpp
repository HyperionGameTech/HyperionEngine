/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Scheduler.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {
namespace threading {

void SchedulerBase::RequestStop()
{
    m_stop_requested.Set(true, MemoryOrder::RELAXED);

    if (!Threads::IsOnThread(m_owner_thread)) {
        WakeUpOwnerThread();
    }
}

bool SchedulerBase::WaitForTasks(std::unique_lock<std::mutex> &lock)
{
    // must be locked before calling this function

    if (m_stop_requested.Get(MemoryOrder::RELAXED)) {
        return false;
    }

    m_has_tasks.wait(
        lock,
        [this]()
        {
            if (m_stop_requested.Get(MemoryOrder::RELAXED)) {
                return true;
            }

            return m_num_enqueued.Get(MemoryOrder::ACQUIRE) != 0;
        }
    );

    return !m_stop_requested.Get(MemoryOrder::RELAXED);
}

void SchedulerBase::WakeUpOwnerThread()
{
    m_has_tasks.notify_all();
}

} // namespace threading
} // namespace hyperion
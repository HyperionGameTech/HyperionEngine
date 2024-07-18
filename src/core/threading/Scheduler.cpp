/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Scheduler.hpp>

namespace hyperion {
namespace threading {

void SchedulerBase::RequestStop()
{
    Threads::AssertOnThread(~m_owner_thread.value, "RequestStop() called from owner thread");

    m_stop_requested.Set(true, MemoryOrder::RELAXED);

    WakeUpOwnerThread();
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
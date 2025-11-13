/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Scheduler.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {
namespace threading {

void SchedulerBase::RequestStop()
{
    m_stopRequested.Set(true, MemoryOrder::RELAXED);

    if (!IsOnThread(m_ownerThread))
    {
        WakeUpOwnerThread();
    }
}

bool SchedulerBase::WaitForTasks(std::unique_lock<std::mutex>& lock)
{
    // must be locked before calling this function

    if (m_stopRequested.Get(MemoryOrder::RELAXED))
    {
        return false;
    }

    m_hasTasks.wait(
        lock,
        [this]()
        {
            if (m_stopRequested.Get(MemoryOrder::RELAXED))
            {
                return true;
            }

            return m_numEnqueued.Get(MemoryOrder::ACQUIRE) != 0;
        });

    return !m_stopRequested.Get(MemoryOrder::RELAXED);
}

} // namespace threading
} // namespace hyperion
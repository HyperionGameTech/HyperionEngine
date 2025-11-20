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

bool SchedulerBase::WaitForTasks(Mutex& mtx)
{
    // must be locked before calling this function

    if (m_stopRequested.Get(MemoryOrder::RELAXED))
    {
        return false;
    }

    while (!m_stopRequested.Get(MemoryOrder::RELAXED) && m_numEnqueued.Get(MemoryOrder::ACQUIRE) == 0)
    {
        m_hasTasksCV.Wait(mtx);
    }

    return !m_stopRequested.Get(MemoryOrder::RELAXED);
}

} // namespace threading
} // namespace hyperion

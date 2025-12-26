/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Scheduler.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace Hyperion {
namespace threading {

void SchedulerBase::RequestStop()
{
    m_stopRequested.Set(true, MemoryOrder::RELAXED);

    if (!IsOnThread(m_ownerThread))
    {
        WakeUpOwnerThread();
    }
}

void SchedulerBase::WaitForTasks(Mutex& mtx, bool* outStopRequested)
{
    // must be locked before calling this function

    if (m_stopRequested.Get(MemoryOrder::RELAXED))
    {
        if (outStopRequested)
        {
            *outStopRequested = true;
        }

        return;
    }

    while (!m_stopRequested.Get(MemoryOrder::RELAXED) && m_numEnqueued.Get(MemoryOrder::ACQUIRE) == 0)
    {
        m_hasTasksCV.Wait(mtx);
    }

    if (outStopRequested)
    {
        *outStopRequested = m_stopRequested.Get(MemoryOrder::RELAXED);
    }
}

} // namespace threading
} // namespace Hyperion

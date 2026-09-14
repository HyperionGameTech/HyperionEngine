/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Threading/Scheduler.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

namespace Hyperion {
namespace threading {

void SchedulerBase::RequestStop()
{
    m_stopRequested.Store(true);

    if (!IsOnThread(m_ownerThread))
    {
        WakeUpOwnerThread();
    }
}

void SchedulerBase::WaitForTasks(Mutex& mtx, bool* outStopRequested, uint32 timeoutMs)
{
    // must be locked before calling this function

    if (HYP_UNLIKELY(m_stopRequested.LoadVolatile()))
    {
        if (outStopRequested)
        {
            *outStopRequested = true;
        }

        return;
    }

    while (!m_stopRequested.LoadVolatile() && m_numEnqueued.Get(MemoryOrder::ACQUIRE) == 0)
    {
        if (timeoutMs == 0)
        {
            m_hasTasksCV.Wait(mtx);

            continue;
        }

        if (!m_hasTasksCV.WaitFor(mtx, timeoutMs))
        {
            break;
        }
    }

    if (outStopRequested)
    {
        *outStopRequested = m_stopRequested.LoadVolatile();
    }
}

} // namespace threading
} // namespace Hyperion

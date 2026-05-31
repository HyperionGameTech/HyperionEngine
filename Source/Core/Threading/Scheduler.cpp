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

void SchedulerBase::WaitForTasks(Mutex& mtx, bool* outStopRequested)
{
    // must be locked before calling this function

    if (m_stopRequested.Load())
    {
        if (outStopRequested)
        {
            *outStopRequested = true;
        }

        return;
    }

    while (!m_stopRequested.Load() && m_numEnqueued.Get(MemoryOrder::ACQUIRE) == 0)
    {
        m_hasTasksCV.Wait(mtx);
    }

    if (outStopRequested)
    {
        *outStopRequested = m_stopRequested.Load();
    }
}

} // namespace threading
} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>
#include <RTC/RTCServerThread.hpp>

namespace Hyperion {

RTCServerThread::RTCServerThread()
    : Thread(NAME("RTCServerThread"))
{
}

void RTCServerThread::operator()(RTCServer* server)
{
    Queue<Scheduler::ScheduledTask> tasks;

    while (!m_stopRequested.LoadVolatile())
    {
        if (uint32 numEnqueued = m_scheduler->NumEnqueued())
        {
            m_scheduler->AcceptAll(tasks);

            while (tasks.Any())
            {
                tasks.Pop().Execute();
            }
        }
    }

    // flush scheduler
    m_scheduler->Flush([](auto& operation)
        {
            operation.Execute();
        });
}

} // namespace Hyperion

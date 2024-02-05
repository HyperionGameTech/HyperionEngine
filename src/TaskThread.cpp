#include <TaskThread.hpp>

namespace hyperion::v2 {

TaskThread::TaskThread(const ThreadID &thread_id)
    : Thread(thread_id),
      m_is_running(false),
      m_is_free(false)
{
}

void TaskThread::Stop()
{
    // m_is_running.Set(false, MemoryOrder::RELAXED);

    m_scheduler.RequestStop();
}

void TaskThread::operator()()
{
    m_is_running.Set(true, MemoryOrder::RELAXED);
    m_is_free.Set(true, MemoryOrder::RELAXED);

    while (IsRunning()) {
        m_scheduler.WaitForTasks(m_task_queue);

        const bool was_free = m_task_queue.Empty();
        m_is_free.Set(was_free, MemoryOrder::RELAXED);
        
        // do not execute within lock
        while (m_task_queue.Any()) {
            m_task_queue.Pop().Execute();
        }
        
        if (!was_free) {
            m_is_free.Set(true, MemoryOrder::RELAXED);
        }
    }
}
} // namespace hyperion::v2
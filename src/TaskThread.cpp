/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <TaskThread.hpp>

namespace hyperion {

TaskThread::TaskThread(const ThreadID &thread_id, ThreadPriorityValue priority)
    : Thread(thread_id, priority),
      m_is_running(false),
      m_stop_requested(false),
      m_is_free(false)
{
}

void TaskThread::Stop()
{
    m_stop_requested.Set(true, MemoryOrder::RELAXED);

    m_scheduler.RequestStop();
}

void TaskThread::operator()()
{
    m_is_running.Set(true, MemoryOrder::RELAXED);
    m_is_free.Set(true, MemoryOrder::RELAXED);

    while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
        m_scheduler.WaitForTasks(m_task_queue);

        const bool was_free = m_task_queue.Empty();
        m_is_free.Set(was_free, MemoryOrder::RELAXED);
        
        // execute all tasks outside of lock
        while (m_task_queue.Any()) {
            m_task_queue.Pop().Execute();
        }
        
        if (!was_free) {
            m_is_free.Set(true, MemoryOrder::RELAXED);
        }
    }

    m_is_running.Set(false, MemoryOrder::RELAXED);
}
} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskThread.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion {
namespace threading {

TaskThread::TaskThread(const ThreadID &thread_id, ThreadPriorityValue priority)
    : Thread(thread_id, priority),
      m_is_running(false),
      m_stop_requested(false),
      m_num_tasks(0)
{
}

void TaskThread::Stop()
{
    m_stop_requested.Set(true, MemoryOrder::RELAXED);

    m_scheduler.RequestStop();
}

bool TaskThread::IsWaitingOnTaskFromThread(const ThreadID &thread_id) const
{
    return m_scheduler.IsWaitingOnTaskFromThread(thread_id);
}

void TaskThread::operator()()
{
    m_is_running.Set(true, MemoryOrder::RELAXED);
    m_num_tasks.Set(0, MemoryOrder::RELAXED);

    while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
        {
            // HYP_PROFILE_BEGIN;
            // HYP_NAMED_SCOPE("Waiting for tasks");

            m_scheduler.WaitForTasks(m_task_queue);
        }

        HYP_PROFILE_BEGIN;

        const uint32 num_tasks = m_task_queue.Size();
        m_num_tasks.Set(num_tasks, MemoryOrder::RELAXED);

        BeforeExecuteTasks();

        {
            HYP_NAMED_SCOPE("Executing tasks");
        
            // execute all tasks outside of lock
            while (m_task_queue.Any()) {
                m_task_queue.Pop().Execute();
            }
            
            if (num_tasks != 0) {
                m_num_tasks.Decrement(num_tasks, MemoryOrder::RELAXED);
            }
        }

        AfterExecuteTasks();
    }

    m_is_running.Set(false, MemoryOrder::RELAXED);
}
} // namespace threading
} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskThread.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/profiling/PerformanceClock.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Tasks);

namespace threading {

static const double g_task_thread_lag_spike_threshold = 50.0;
static const double g_task_thread_single_task_lag_spike_threshold = 10.0;

// #define HYP_ENABLE_LAG_SPIKE_DETECTION

TaskThread::TaskThread(const ThreadID &thread_id, ThreadPriorityValue priority)
    : Thread(thread_id, priority),
      m_is_running(false),
      m_stop_requested(false),
      m_num_tasks(0)
{
}

TaskThread::TaskThread(Name name, ThreadPriorityValue priority)
    : Thread(ThreadID(name, THREAD_CATEGORY_TASK), priority),
      m_is_running(false),
      m_stop_requested(false),
      m_num_tasks(0)
{
}

void TaskThread::Stop()
{
    m_stop_requested.Set(true, MemoryOrder::RELAXED);

    m_scheduler.RequestStop();
    
    m_is_running.Set(false, MemoryOrder::RELAXED);
}

void TaskThread::operator()()
{
    m_is_running.Set(true, MemoryOrder::RELAXED);
    m_num_tasks.Set(m_task_queue.Size(), MemoryOrder::RELEASE);

    while (!m_stop_requested.Get(MemoryOrder::RELAXED)) {
        if (m_task_queue.Empty()) {
            if (!m_scheduler.WaitForTasks(m_task_queue)) {
                Stop();

                break;
            }
        } else {
            // append all tasks from the scheduler
            m_scheduler.AcceptAll(m_task_queue);
        }

        HYP_PROFILE_BEGIN;

        const uint32 num_tasks = m_task_queue.Size();
        m_num_tasks.Set(num_tasks, MemoryOrder::RELEASE);

        BeforeExecuteTasks();

        {
            HYP_NAMED_SCOPE("Executing tasks");

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
            uint32 num_executed_tasks = 0;

            PerformanceClock performance_clock;
            performance_clock.Start();
#endif
        
            // execute all tasks outside of lock
            while (m_task_queue.Any()) {
#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
                PerformanceClock task_performance_clock;
                task_performance_clock.Start();
#endif

                Scheduler::ScheduledTask scheduled_task = m_task_queue.Pop();
                scheduled_task.Execute();

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
                task_performance_clock.Stop();

                ++num_executed_tasks;

                if (task_performance_clock.Elapsed() / 1000.0 > g_task_thread_single_task_lag_spike_threshold) {
                    HYP_LOG(Tasks, Warning, "Task thread {} lag spike detected in single task \"{}\": {}ms",
                        GetID().GetName(),
                        scheduled_task.debug_name.value ? scheduled_task.debug_name.value : "<unnamed task>",
                        task_performance_clock.Elapsed() / 1000.0);
                }
#endif
            }

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
            performance_clock.Stop();

            if (performance_clock.Elapsed() / 1000.0 > g_task_thread_lag_spike_threshold) {
                HYP_LOG(Tasks, Warning, "Task thread {} lag spike detected executing {} tasks: {}ms", GetID().GetName(), num_executed_tasks, performance_clock.Elapsed() / 1000.0);
            }
#endif
            
            m_num_tasks.Decrement(num_tasks, MemoryOrder::RELEASE);
        }

        AfterExecuteTasks();
    }

    m_is_running.Set(false, MemoryOrder::RELAXED);
}
} // namespace threading
} // namespace hyperion
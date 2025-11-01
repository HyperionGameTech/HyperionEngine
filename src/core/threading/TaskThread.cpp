/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskThread.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/profiling/PerformanceClock.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Tasks);

namespace threading {

static const double TaskThreadLagSpikeThreshold = 50.0;
static const double TaskThreadSingleTaskLagSpikeThreshold = 10.0;

// #define HYP_ENABLE_LAG_SPIKE_DETECTION

TaskThread::TaskThread(const ThreadId& threadId, ThreadPriorityValue priority)
    : Thread(threadId, priority),
      m_numTasks(0)
{
}

TaskThread::TaskThread(Name name, ThreadPriorityValue priority)
    : Thread(ThreadId(name, THREAD_CATEGORY_TASK), priority),
      m_numTasks(0)
{
}

void TaskThread::SetPriority(ThreadPriorityValue priority)
{
    /// \todo
    HYP_NOT_IMPLEMENTED();
}

void TaskThread::operator()()
{
    m_numTasks.Set(m_taskQueue.Size(), MemoryOrder::RELEASE);

    while (!m_stopRequested.Get(MemoryOrder::RELAXED))
    {
        if (m_taskQueue.Empty())
        {
            if (!m_scheduler.WaitForTasks(m_taskQueue))
            {
                Stop();

                break;
            }
        }
        else
        {
            // append all tasks from the scheduler
            m_scheduler.AcceptAll(m_taskQueue);
        }

        HYP_PROFILE_BEGIN;

        const uint32 numTasks = m_taskQueue.Size();
        m_numTasks.Set(numTasks, MemoryOrder::RELEASE);

        BeforeExecuteTasks();

        {
            HYP_NAMED_SCOPE("Executing tasks");

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
            uint32 numExecutedTasks = 0;

            PerformanceClock performanceClock;
            performanceClock.Start();
#endif

            // execute all tasks outside of lock
            while (m_taskQueue.Any())
            {
#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
                PerformanceClock taskPerformanceClock;
                taskPerformanceClock.Start();
#endif

                Scheduler::ScheduledTask scheduledTask = m_taskQueue.Pop();
                scheduledTask.Execute();

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
                taskPerformanceClock.Stop();

                ++numExecutedTasks;

                if (taskPerformanceClock.Elapsed() / 1000.0 > TaskThreadSingleTaskLagSpikeThreshold)
                {
                    HYP_LOG(Tasks, Warning, "Task thread {} lag spike detected in single task \"{}\": {}ms",
                        Id().GetName(),
                        scheduledTask.debugName.value ? scheduledTask.debugName.value : "<unnamed task>",
                        taskPerformanceClock.Elapsed() / 1000.0);
                }
#endif
            }

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
            performanceClock.Stop();

            if (performanceClock.Elapsed() / 1000.0 > TaskThreadLagSpikeThreshold)
            {
                HYP_LOG(Tasks, Warning, "Task thread {} lag spike detected executing {} tasks: {}ms", Id().GetName(), numExecutedTasks, performanceClock.Elapsed() / 1000.0);
            }
#endif

            m_numTasks.Decrement(numTasks, MemoryOrder::RELEASE);
        }

        AfterExecuteTasks();
    }
}
} // namespace threading
} // namespace hyperion
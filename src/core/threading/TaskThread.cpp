/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskThread.hpp>
#include <core/threading/ThreadPool.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>
#include <core/profiling/PerformanceClock.hpp>

namespace Hyperion {

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
    while (!m_stopRequested.Get(MemoryOrder::RELAXED))
    {
        Scheduler::ScheduledTask scheduledTask;
        bool gotTask = false;

        if (m_scheduler.TryPop(scheduledTask))
        {
            gotTask = true;
        }
        else if (m_ownerPool != nullptr && m_ownerPool->TryStealTask(this, scheduledTask))
        {
            gotTask = true;
        }

        if (!gotTask)
        {
            bool stopRequested = false;
            m_scheduler.WaitForTasks(&stopRequested);

            if (stopRequested)
            {
                Stop();

                break;
            }

            continue;
        }

        HYP_PROFILE_BEGIN;

        m_numTasks.Set(m_scheduler.NumEnqueued() + 1, MemoryOrder::RELEASE);

        BeforeExecuteTasks();

        {
            HYP_NAMED_SCOPE("Executing tasks");

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
            PerformanceClock taskPerformanceClock;
            taskPerformanceClock.Start();
#endif

            scheduledTask.Execute();

#ifdef HYP_ENABLE_LAG_SPIKE_DETECTION
            taskPerformanceClock.Stop();

            if (taskPerformanceClock.Elapsed() / 1000.0 > TaskThreadSingleTaskLagSpikeThreshold)
            {
                HYP_LOG(Tasks, Warning, "Task thread {} lag spike detected in single task \"{}\": {}ms",
                    Id().GetName(),
                    scheduledTask.debugName.value ? scheduledTask.debugName.value : "<unnamed task>",
                    taskPerformanceClock.Elapsed() / 1000.0);
            }
#endif
        }

        AfterExecuteTasks();

        m_numTasks.Set(m_scheduler.NumEnqueued(), MemoryOrder::RELEASE);
    }
}
} // namespace threading
} // namespace Hyperion
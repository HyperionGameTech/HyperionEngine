/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/threading/Task.hpp>
#include <Core/threading/Scheduler.hpp>

namespace Hyperion {

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
extern Pool* g_taskPool;
#endif

namespace threading {

static Mutex s_deferredTasksMutex;
static Array<TaskExecutorBase*> s_deferredTasks;

static constexpr uint32 DeferredTasksCleanupThreshold = 32;

HYP_API Pool* GetTaskPool()
{
#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    AssertDebug(g_taskPool != nullptr);
    return g_taskPool;
#else
    static Pool s_taskPool(4 * 1024 * 1024, PF_THREAD_SAFE);
    return &s_taskPool;
#endif
}

static void CleanupDeferredTasks()
{
    Array<TaskExecutorBase*> toDelete;

    for (auto it = s_deferredTasks.Begin(); it != s_deferredTasks.End();)
    {
        if ((*it)->IsCompleted())
        {
            toDelete.PushBack(*it);

            it = s_deferredTasks.Erase(it);
        }
        else
        {
            ++it;
        }
    }

    s_deferredTasksMutex.Unlock();

    if (toDelete.Any())
    {
        for (TaskExecutorBase* taskExecutor : toDelete)
        {
            delete taskExecutor;
        }
    }
}

HYP_API void Task_DeleteAllDeferredTasks()
{
    Array<TaskExecutorBase*> toDelete;

    s_deferredTasksMutex.Lock();

    toDelete = std::move(s_deferredTasks);

    s_deferredTasks.Clear();

    s_deferredTasksMutex.Unlock();

    if (toDelete.Any())
    {
        for (TaskExecutorBase* taskExecutor : toDelete)
        {
            AssertDebug(taskExecutor->IsCompleted());

            delete taskExecutor;
        }
    }
}

HYP_API void Task_DeferTaskDeletion(TaskExecutorBase* taskExecutor)
{
    if (!taskExecutor)
    {
        return;
    }

    if (taskExecutor->IsCompleted())
    {
        delete taskExecutor;
        return;
    }

    s_deferredTasksMutex.Lock();
    s_deferredTasks.PushBack(taskExecutor);

    if (s_deferredTasks.Size() >= DeferredTasksCleanupThreshold)
    {
        CleanupDeferredTasks();
    }
    else
    {
        s_deferredTasksMutex.Unlock();
    }
}

#pragma region TaskCallbackChain

TaskCallbackChain::TaskCallbackChain(TaskCallbackChain&& other) noexcept
{
    Mutex::Guard guard(other.m_mutex);

    m_callbacks = std::move(other.m_callbacks);
    m_numCallbacks.Set(other.m_numCallbacks.Exchange(0, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);
}

TaskCallbackChain& TaskCallbackChain::operator=(TaskCallbackChain&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Mutex::Guard thisGuard(m_mutex);
    Mutex::Guard otherGuard(other.m_mutex);

    m_callbacks = std::move(other.m_callbacks);
    m_numCallbacks.Set(other.m_numCallbacks.Exchange(0, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

    return *this;
}

void TaskCallbackChain::Add(Proc<void()>&& callback)
{
    /// \todo : Smarter implementation possibly using semaphores that are set up with a value when task is first initialized,
    // need a way to tell if the added callback will never be executed because the task completed.
    Mutex::Guard guard(m_mutex);

    m_callbacks.PushBack(std::move(callback));

    m_numCallbacks.Increment(1, MemoryOrder::RELEASE);
}

void TaskCallbackChain::operator()()
{
    if (m_numCallbacks.Get(MemoryOrder::ACQUIRE))
    {
        Mutex::Guard guard(m_mutex);

        for (Proc<void()>& proc : m_callbacks)
        {
            proc();
        }
    }
}

#pragma endregion TaskCallbackChain

#pragma region TaskBase

bool TaskBase::Cancel()
{
    if (!m_id.IsValid() || !m_assignedScheduler)
    {
        return false;
    }

    if (m_assignedScheduler->Dequeue(m_id))
    {
        m_id = {};
        m_assignedScheduler = nullptr;

        // Reset the task state since it was dequeued.
        Reset();

        return true;
    }

    return false;
}

void TaskBase::Await_Internal() const
{
    AssertDebug(IsValid());

    TaskExecutorBase* executor = GetTaskExecutor();
    AssertDebug(executor != nullptr);

    executor->GetNotifier().Await();
}

#pragma endregion TaskBase

} // namespace threading
} // namespace Hyperion
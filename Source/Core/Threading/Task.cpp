/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Threading/Task.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {
namespace threading {

static Mutex s_deferredTasksMutex;
static Array<TaskExecutorBase*> s_deferredTasks;

static constexpr uint32 DeferredTasksCleanupThreshold = 32;

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

CORE_API void Task_DeleteAllDeferredTasks()
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

CORE_API void Task_DeferTaskDeletion(TaskExecutorBase* taskExecutor)
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
    m_completed.Set(other.m_completed.Exchange(false, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);
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
    m_completed.Set(other.m_completed.Exchange(false, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

    return *this;
}

TaskCallbackChain::~TaskCallbackChain() = default;

void TaskCallbackChain::Add(Proc<void()>&& callback)
{
    if (!callback.IsValid())
    {
        return;
    }

    m_mutex.Lock();

    if (m_completed.Get(MemoryOrder::ACQUIRE))
    {
        // The task is already done, so nothing will ever run this for us. Invoking outside of the
        // lock keeps a callback that registers further callbacks from deadlocking.
        m_mutex.Unlock();

        callback();

        return;
    }

    m_callbacks.PushBack(std::move(callback));

    m_mutex.Unlock();
}

TaskCallbackChain::CallbackList TaskCallbackChain::Detach()
{
    CallbackList callbacks;

    m_mutex.Lock();

    if (!m_completed.Exchange(true, MemoryOrder::ACQUIRE_RELEASE))
    {
        callbacks = std::move(m_callbacks);
    }

    m_mutex.Unlock();

    return callbacks;
}

#pragma endregion TaskCallbackChain

#pragma region TaskExecutorBase

void TaskExecutorBase::Complete(TaskCompleteNotifier* notifier, ProcRef<void()> onNotifierSignalled)
{
    // Read everything we need off of the executor up front: releasing the notifier lets a thread
    // waiting on the task destroy it from under us.
    TaskCallbackChain::CallbackList callbacks = m_callbackChain.Detach();
    const bool deferredDeletionEnabled = m_deferredDeletionEnabled;

    if (notifier != nullptr)
    {
        notifier->Release(1, onNotifierSignalled);
    }
    else if (onNotifierSignalled.IsValid())
    {
        onNotifierSignalled();
    }

    for (Proc<void()>& callback : callbacks)
    {
        callback();
    }

    if (deferredDeletionEnabled
        && m_deletionState.Exchange(DeletionState::COMPLETED, MemoryOrder::ACQUIRE_RELEASE) == DeletionState::OWNER_RELEASED)
    {
        // The owning Task is already gone, so cleaning up is left to us.
        delete this;
    }
}

#pragma endregion TaskExecutorBase

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

void TaskBase::OnComplete(Proc<void()>&& callback)
{
    AssertDebug(IsValid(), "Cannot add a completion callback to an invalid Task");

    TaskExecutorBase* executor = GetTaskExecutor();

    if (executor == nullptr)
    {
        return;
    }

    executor->GetCallbackChain().Add(std::move(callback));
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

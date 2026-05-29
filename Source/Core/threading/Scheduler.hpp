/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Array.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/StaticMessage.hpp>

#include <Core/threading/SchedulerFwd.hpp>
#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Thread.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/Mutex.hpp>
#include <Core/threading/ConditionVariable.hpp>

#include <Core/utilities/FunctionTraits.hpp>
#include <Core/Defines.hpp>

#include <Core/Types.hpp>

#include <utility>
#include <type_traits>

namespace Hyperion {
namespace threading {

class CORE_API SchedulerBase
{
public:
    SchedulerBase() = delete;
    SchedulerBase(const SchedulerBase& other) = delete;
    SchedulerBase& operator=(const SchedulerBase& other) = delete;
    SchedulerBase(SchedulerBase&& other) noexcept = delete;
    SchedulerBase& operator=(SchedulerBase&& other) noexcept = delete;
    virtual ~SchedulerBase() = default;

    HYP_FORCE_INLINE ThreadId GetOwnerThread() const
    {
        return m_ownerThread;
    }

    /*! \brief Set the given thread Id to be the owner thread of this Scheduler.
     *  Tasks are to be enqueued from any other thread, and executed only from the owner thread.
     */
    HYP_FORCE_INLINE void SetOwnerThread(ThreadId ownerThread)
    {
        m_ownerThread = ownerThread;
    }

    void WakeUpOwnerThread()
    {
        m_hasTasksCV.NotifyAll();
    }

    void RequestStop();

    virtual TaskID EnqueueTaskExecutor(TaskExecutorBase* executorPtr, TaskCompleteNotifier* notifier, OnTaskCompletedCallback&& callback = nullptr, const StaticMessage& debugName = StaticMessage()) = 0;

    virtual bool Dequeue(TaskID id) = 0;

    virtual bool TakeOwnershipOfTask(TaskID id, TaskExecutorBase* executor) = 0;

    /*! \brief Has \p threadId given us work to complete?
     *  Returns true if \p threadId might be waiting on us to complete some work for them. */
    virtual bool HasWorkAssignedFromThread(ThreadId threadId) const = 0;

protected:
    SchedulerBase(ThreadId ownerThread)
        : m_ownerThread(ownerThread)
    {
    }

    void WaitForTasks(Mutex& mtx, bool* outStopRequested);

    uint32 m_idCounter = 0;
    AtomicVar<uint32> m_numEnqueued { 0 };
    AtomicFlag m_stopRequested;

    mutable Mutex m_mutex;
    ConditionVariable m_hasTasksCV;
    ConditionVariable m_taskExecutedCV;

    ThreadId m_ownerThread;
};

class Scheduler final : public SchedulerBase
{
    friend class TaskThreadPool;

public:
    struct ScheduledTask
    {
        // The executor/task memory
        TaskExecutorBase* executor = nullptr;

        // If the executor is owned by the scheduler, it will be deleted when this object is destroyed
        bool ownsExecutor = false;

        // Task notifier to signal when the task is completed (used for batch tasks)
        TaskCompleteNotifier* notifier = nullptr;

        // Condition variable to notify when the task has been executed (owned by the scheduler)
        ConditionVariable* pTaskExecutedCV = nullptr;

        // Callback to be executed after the task is completed
        OnTaskCompletedCallback callback;

        StaticMessage debugName;

        ScheduledTask() = default;

        ScheduledTask(const ScheduledTask& other) = delete;
        ScheduledTask& operator=(const ScheduledTask& other) = delete;

        ScheduledTask(ScheduledTask&& other) noexcept
            : executor(other.executor),
              ownsExecutor(other.ownsExecutor),
              notifier(other.notifier),
              pTaskExecutedCV(other.pTaskExecutedCV),
              callback(std::move(other.callback)),
              debugName(std::move(other.debugName))
        {
            other.executor = nullptr;
            other.ownsExecutor = false;
            other.notifier = nullptr;
            other.pTaskExecutedCV = nullptr;
        }

        ScheduledTask& operator=(ScheduledTask&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            if (ownsExecutor)
            {
                delete executor;
            }

            executor = other.executor;
            ownsExecutor = other.ownsExecutor;
            notifier = other.notifier;
            pTaskExecutedCV = other.pTaskExecutedCV;
            callback = std::move(other.callback);
            debugName = std::move(other.debugName);

            other.executor = nullptr;
            other.ownsExecutor = false;
            other.notifier = nullptr;
            other.pTaskExecutedCV = nullptr;

            return *this;
        }

        ~ScheduledTask()
        {
            if (ownsExecutor)
            {
                delete executor;
            }
        }

        template <class Lambda>
        void ExecuteWithLambda(Lambda&& lambda)
        {
            lambda(*executor);

            int counterValue = 0;

            if (notifier != nullptr)
            {
                counterValue = notifier->Release(1, callback);
            }
            else if (callback.IsValid())
            {
                callback();
            }

            pTaskExecutedCV->NotifyAll();
        }

        void Execute()
        {
            executor->Execute();

            int counterValue = 0;

            if (notifier != nullptr)
            {
                counterValue = notifier->Release(1, callback);
            }
            else if (callback.IsValid())
            {
                callback();
            }

            pTaskExecutedCV->NotifyAll();
        }
    };

    Scheduler(ThreadId ownerThreadId = CurrentThreadId())
        : SchedulerBase(ownerThreadId)
    {
    }

    Scheduler(const Scheduler& other) = delete;
    Scheduler& operator=(const Scheduler& other) = delete;
    Scheduler(Scheduler&& other) noexcept = delete;
    Scheduler& operator=(Scheduler&& other) noexcept = delete;
    virtual ~Scheduler() override = default;

    HYP_FORCE_INLINE uint32 NumEnqueued() const
    {
        return m_numEnqueued.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE const Array<ScheduledTask>& GetEnqueuedTasks() const
    {
        return m_queue;
    }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     *  called from a non-owner thread.
     *  \param fn The function to execute
     *  \param flags Flags to control the behavior of the task - see TaskEnqueueFlags */
    template <class Function>
    auto Enqueue(Function&& fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        return Enqueue(HYP_STATIC_MESSAGE("<no debug name>"), std::forward<Function>(fn), flags);
    }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     *  called from a non-owner thread.
     *  \param debugName A StaticMessage instance containing the name of the task (for debugging)
     *  \param fn The function to execute
     *  \param flags Flags to control the behavior of the task - see TaskEnqueueFlags */
    template <class Function>
    auto Enqueue(const StaticMessage& debugName, Function&& fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        using ReturnType = typename FunctionTraits<Function>::ReturnType;

        TaskExecutorInstance<ReturnType>* executor;

        {
            Mutex::Guard guard(m_mutex);

            executor = new TaskExecutorInstance<ReturnType>(std::forward<Function>(fn));

            ScheduledTask scheduledTask;
            scheduledTask.executor = executor;
            scheduledTask.ownsExecutor = (flags & TaskEnqueueFlags::FIRE_AND_FORGET);
            scheduledTask.notifier = &executor->GetNotifier();
            scheduledTask.pTaskExecutedCV = &m_taskExecutedCV;
            scheduledTask.callback = OnTaskCompletedCallback(&executor->GetCallbackChain());
            scheduledTask.debugName = debugName;

            Enqueue_Internal(std::move(scheduledTask));
        }

        Task<ReturnType> task(executor->GetTaskID(), this, executor, !(flags & TaskEnqueueFlags::FIRE_AND_FORGET));

        WakeUpOwnerThread();

        return task;
    }

    /*! \brief Enqueue a task to be executed on the owner thread. This is to be
     *  called from a non-owner thread.
     *  \internal Used by TaskSystem to enqueue batches of tasks.
     *  \param executorPtr The TaskExecutor to execute (owned by the caller)
     *  \param notifier A TaskCompleteNotifier to be used to notify when the task is completed.
     *  \param callback A callback to be executed after the task is completed.
     *  \param debugName A StaticMessage instance containing the name of the task (for debugging) */
    virtual TaskID EnqueueTaskExecutor(TaskExecutorBase* executorPtr, TaskCompleteNotifier* notifier, OnTaskCompletedCallback&& callback = nullptr, const StaticMessage& debugName = StaticMessage()) override
    {
        TaskID taskId;

        {
            Mutex::Guard guard(m_mutex);

            ScheduledTask scheduledTask;
            scheduledTask.executor = executorPtr;
            scheduledTask.ownsExecutor = false;
            scheduledTask.notifier = notifier;
            scheduledTask.pTaskExecutedCV = &m_taskExecutedCV;
            scheduledTask.callback = std::move(callback);
            scheduledTask.debugName = debugName;

            Enqueue_Internal(std::move(scheduledTask));

            taskId = executorPtr->GetTaskID();
        }

        WakeUpOwnerThread();

        return taskId;
    }

    /*! \brief Remove a function from the owner thread's queue, if it exists
     * @returns a boolean value indicating whether or not the function was successfully dequeued */
    virtual bool Dequeue(TaskID id) override
    {
        if (!id)
        {
            return false;
        }

        Mutex::Guard guard(m_mutex);

        if (Dequeue_Internal(id))
        {
            return true;
        }

        return false;
    }

    virtual bool TakeOwnershipOfTask(TaskID id, TaskExecutorBase* executor) override
    {
        HYP_CORE_ASSERT(!IsOnThread(m_ownerThread));

        HYP_CORE_ASSERT(id.IsValid());
        HYP_CORE_ASSERT(executor != nullptr);

        TaskExecutorBase* executorCasted = static_cast<TaskExecutorBase*>(executor);

        Mutex::Guard guard(m_mutex);

        const auto it = m_queue.FindIf([id](const ScheduledTask& item)
            {
                if (!item.executor)
                {
                    return false;
                }

                return item.executor->GetTaskID() == id;
            });

        HYP_CORE_ASSERT(it != m_queue.End());

        // if (it == m_queue.End()) {
        //     return false;
        // }

        ScheduledTask& scheduledTask = *it;

        if (scheduledTask.ownsExecutor)
        {
            HYP_CORE_ASSERT(scheduledTask.executor != nullptr);
            HYP_CORE_ASSERT(scheduledTask.executor != executorCasted);

            delete scheduledTask.executor;
        }

        // Release memory from the UniquePtr and assign it to the ScheduledTask
        // the ScheduledTask will delete the executor when it is destructed.
        scheduledTask.executor = executorCasted;
        scheduledTask.notifier = &executorCasted->GetNotifier();
        scheduledTask.ownsExecutor = true;

        return true;
    }

    virtual bool HasWorkAssignedFromThread(ThreadId threadId) const override
    {
        Mutex::Guard guard(m_mutex);

        return AnyOf(m_queue, [threadId](const ScheduledTask& item)
            {
                return item.executor->GetInitiatorThreadId() == threadId;
            });
    }

    bool TryPop(ScheduledTask& outTask)
    {
        Mutex::Guard guard(m_mutex);

        if (m_queue.Empty())
        {
            return false;
        }

        outTask = std::move(m_queue.Front());
        m_queue.PopFront();
        m_numEnqueued.Decrement(1, MemoryOrder::RELEASE);

        return true;
    }

    /*! \brief Blocks the current thread until there are tasks to execute, or the scheduler is stopped.
     * @param outStopRequested Pointer to a boolean that will be set to true if the scheduler was stopped.
     */
    void WaitForTasks(bool* outStopRequested)
    {
        HYP_CORE_ASSERT(IsOnThread(m_ownerThread));

        Mutex::Guard guard(m_mutex);

        SchedulerBase::WaitForTasks(m_mutex, outStopRequested);
    }

    /*! \brief Execute all scheduled tasks. May only be called from the creation thread. */
    template <class Lambda>
    void Flush(Lambda&& lambda)
    {
        HYP_CORE_ASSERT(IsOnThread(m_ownerThread));

        {
            Mutex::Guard guard(m_mutex);

            while (m_queue.Any())
            {
                ScheduledTask& front = m_queue.Front();

                front.ExecuteWithLambda(lambda);

                m_numEnqueued.Decrement(1, MemoryOrder::RELEASE);
                m_queue.PopFront();
            }
        }

        WakeUpOwnerThread();
    }

    /* Move all tasks in the queue to an external container. */
    template <class Container>
    void AcceptAll(Container& outContainer)
    {
        HYP_CORE_ASSERT(IsOnThread(m_ownerThread));

        {
            Mutex::Guard guard(m_mutex);

            for (auto it = m_queue.Begin(); it != m_queue.End(); ++it)
            {
                outContainer.Add(std::move(*it));
                m_numEnqueued.Decrement(1, MemoryOrder::RELEASE);
            }

            m_queue.Clear();
        }

        WakeUpOwnerThread();
    }

private:
    /*! \brief Attempt to steal a task from the back of the queue.
     *  Used by ThreadPool to steal work from other schedulers.
     * @param outTask The stolen task, if any.
     * @returns true if a task was stolen, false otherwise. */
    bool TryStealFrom(ScheduledTask& outTask)
    {
        Mutex::Guard guard(m_mutex);

        if (m_queue.Empty())
        {
            return false;
        }

        // Steal from back
        outTask = std::move(m_queue.Back());
        m_queue.PopBack();
        m_numEnqueued.Decrement(1, MemoryOrder::RELEASE);

        return true;
    }

    void Enqueue_Internal(ScheduledTask&& scheduledTask)
    {
        const TaskID taskId { ++m_idCounter };

        scheduledTask.executor->SetTaskID(taskId);
        scheduledTask.executor->SetInitiatorThreadId(CurrentThreadId());
        scheduledTask.executor->SetAssignedScheduler(this);

        m_queue.PushBack(std::move(scheduledTask));
        m_numEnqueued.Increment(1, MemoryOrder::RELEASE);
    }

    bool Dequeue_Internal(TaskID id)
    {
        const auto it = m_queue.FindIf([&id](const auto& item)
            {
                return item.executor->GetTaskID() == id;
            });
        ;

        if (it == m_queue.End())
        {
            return false;
        }

        m_queue.Erase(it);

        m_numEnqueued.Decrement(1, MemoryOrder::RELEASE);

        return true;
    }

    Array<ScheduledTask> m_queue;
};
} // namespace threading

using threading::Scheduler;
using threading::SchedulerBase;

} // namespace Hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_SCHEDULER_HPP
#define HYPERION_CORE_SCHEDULER_HPP

#include <core/containers/Array.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/StaticMessage.hpp>

#include <core/algorithm/AnyOf.hpp>

#include <core/threading/SchedulerFwd.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Threads.hpp>

#include <core/Traits.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

#include <utility>
#include <type_traits>
#include <mutex>
#include <condition_variable>

namespace hyperion {
namespace threading {

class HYP_API SchedulerBase
{
public:
    SchedulerBase() = delete;
    SchedulerBase(const SchedulerBase& other) = delete;
    SchedulerBase& operator=(const SchedulerBase& other) = delete;
    SchedulerBase(SchedulerBase&& other) noexcept = delete;
    SchedulerBase& operator=(SchedulerBase&& other) noexcept = delete;
    virtual ~SchedulerBase() = default;

    HYP_FORCE_INLINE ThreadID GetOwnerThread() const
    {
        return m_owner_thread;
    }

    /*! \brief Set the given thread ID to be the owner thread of this Scheduler.
     *  Tasks are to be enqueued from any other thread, and executed only from the owner thread.
     */
    HYP_FORCE_INLINE void SetOwnerThread(ThreadID owner_thread)
    {
        m_owner_thread = owner_thread;
    }

    void RequestStop();

    virtual void Await(TaskID id) = 0;

    virtual TaskID EnqueueTaskExecutor(TaskExecutorBase* executor_ptr, TaskCompleteNotifier* notifier, OnTaskCompletedCallback&& callback = nullptr, const StaticMessage& debug_name = StaticMessage()) = 0;

    virtual bool Dequeue(TaskID id) = 0;

    virtual bool TakeOwnershipOfTask(TaskID id, ITaskExecutor* executor) = 0;

    /*! \brief Has \ref{thread_id} given us work to complete?
     *  Returns true if \ref{thread_id} might be waiting on us to complete some work for them. */
    virtual bool HasWorkAssignedFromThread(ThreadID thread_id) const = 0;

protected:
    SchedulerBase(ThreadID owner_thread)
        : m_owner_thread(owner_thread)
    {
    }

    void WakeUpOwnerThread()
    {
        m_has_tasks.notify_all();
    }

    bool WaitForTasks(std::unique_lock<std::mutex>& lock);

    uint32 m_id_counter = 0;
    AtomicVar<uint32> m_num_enqueued { 0 };
    AtomicVar<bool> m_stop_requested { false };

    mutable std::mutex m_mutex;
    std::condition_variable m_has_tasks;
    std::condition_variable m_task_executed;

    ThreadID m_owner_thread;
};

class Scheduler final : public SchedulerBase
{
public:
    struct ScheduledTask
    {
        // The executor/task memory
        TaskExecutorBase* executor = nullptr;

        // If the executor is owned by the scheduler, it will be deleted when this object is destroyed
        bool owns_executor = false;

        // Task notifier to signal when the task is completed (used for batch tasks)
        TaskCompleteNotifier* notifier = nullptr;

        // Condition variable to notify when the task has been executed (owned by the scheduler)
        std::condition_variable* task_executed = nullptr;

        // Callback to be executed after the task is completed
        OnTaskCompletedCallback callback;

        StaticMessage debug_name;

        ScheduledTask() = default;

        ScheduledTask(const ScheduledTask& other) = delete;
        ScheduledTask& operator=(const ScheduledTask& other) = delete;

        ScheduledTask(ScheduledTask&& other) noexcept
            : executor(other.executor),
              owns_executor(other.owns_executor),
              notifier(other.notifier),
              task_executed(other.task_executed),
              callback(std::move(other.callback)),
              debug_name(std::move(other.debug_name))
        {
            other.executor = nullptr;
            other.owns_executor = false;
            other.notifier = nullptr;
            other.task_executed = nullptr;
        }

        ScheduledTask& operator=(ScheduledTask&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            if (owns_executor)
            {
                delete executor;
            }

            executor = other.executor;
            owns_executor = other.owns_executor;
            notifier = other.notifier;
            task_executed = other.task_executed;
            callback = std::move(other.callback);
            debug_name = std::move(other.debug_name);

            other.executor = nullptr;
            other.owns_executor = false;
            other.notifier = nullptr;
            other.task_executed = nullptr;

            return *this;
        }

        ~ScheduledTask()
        {
            if (owns_executor)
            {
                delete executor;
            }
        }

        template <class Lambda>
        void ExecuteWithLambda(Lambda&& lambda)
        {
            lambda(*executor);

            int counter_value = 0;

            if (notifier != nullptr)
            {
                counter_value = notifier->Release(1, callback);
            }
            else if (callback.IsValid())
            {
                callback();
            }

            task_executed->notify_all();
        }

        void Execute()
        {
            executor->Execute();

            int counter_value = 0;

            if (notifier != nullptr)
            {
                counter_value = notifier->Release(1, callback);
            }
            else if (callback.IsValid())
            {
                callback();
            }

            task_executed->notify_all();
        }
    };

    Scheduler(ThreadID owner_thread_id = Threads::CurrentThreadID())
        : SchedulerBase(owner_thread_id)
    {
    }

    Scheduler(const Scheduler& other) = delete;
    Scheduler& operator=(const Scheduler& other) = delete;
    Scheduler(Scheduler&& other) noexcept = delete;
    Scheduler& operator=(Scheduler&& other) noexcept = delete;
    virtual ~Scheduler() override = default;

    HYP_FORCE_INLINE uint32 NumEnqueued() const
    {
        return m_num_enqueued.Get(MemoryOrder::ACQUIRE);
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
     *  \param debug_name A StaticMessage instance containing the name of the task (for debugging)
     *  \param fn The function to execute
     *  \param flags Flags to control the behavior of the task - see TaskEnqueueFlags */
    template <class Function>
    auto Enqueue(const StaticMessage& debug_name, Function&& fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        using ReturnType = typename FunctionTraits<Function>::ReturnType;

        std::unique_lock lock(m_mutex);

        TaskExecutorInstance<ReturnType>* executor = new TaskExecutorInstance<ReturnType>(std::forward<Function>(fn));

        ScheduledTask scheduled_task;
        scheduled_task.executor = executor;
        scheduled_task.owns_executor = (flags & TaskEnqueueFlags::FIRE_AND_FORGET);
        scheduled_task.notifier = &executor->GetNotifier();
        scheduled_task.task_executed = &m_task_executed;
        scheduled_task.callback = OnTaskCompletedCallback(&executor->GetCallbackChain());
        scheduled_task.debug_name = debug_name;

        Enqueue_Internal(std::move(scheduled_task));

        Task<ReturnType> task(executor->GetTaskID(), this, executor, !(flags & TaskEnqueueFlags::FIRE_AND_FORGET));

        lock.unlock();
        WakeUpOwnerThread();

        return task;
    }

    /*! \brief Enqueue a task to be executed on the owner thread. This is to be
     *  called from a non-owner thread.
     *  \internal Used by TaskSystem to enqueue batches of tasks.
     *  \param executor_ptr The TaskExecutor to execute (owned by the caller)
     *  \param notifier A TaskCompleteNotifier to be used to notify when the task is completed.
     *  \param callback A callback to be executed after the task is completed.
     *  \param debug_name A StaticMessage instance containing the name of the task (for debugging) */
    virtual TaskID EnqueueTaskExecutor(TaskExecutorBase* executor_ptr, TaskCompleteNotifier* notifier, OnTaskCompletedCallback&& callback = nullptr, const StaticMessage& debug_name = StaticMessage()) override
    {
        std::unique_lock lock(m_mutex);

        ScheduledTask scheduled_task;
        scheduled_task.executor = executor_ptr;
        scheduled_task.owns_executor = false;
        scheduled_task.notifier = notifier;
        scheduled_task.task_executed = &m_task_executed;
        scheduled_task.callback = std::move(callback);
        scheduled_task.debug_name = debug_name;

        Enqueue_Internal(std::move(scheduled_task));

        const TaskID task_id = executor_ptr->GetTaskID();

        lock.unlock();

        WakeUpOwnerThread();

        return task_id;
    }

    /*! \brief Wait until the given task has been executed (no longer in the queue). */
    virtual void Await(TaskID id) override
    {
        AssertThrow(!Threads::IsOnThread(m_owner_thread));

        std::unique_lock lock(m_mutex);

        if (m_queue.Empty())
        {
            return;
        }

        if (!AnyOf(m_queue, [id](const auto& item)
                {
                    return item.executor->GetTaskID() == id;
                }))
        {
            return;
        }

        m_task_executed.wait(lock, [this, id]
            {
                if (m_queue.Empty())
                {
                    return true;
                }

                return !AnyOf(m_queue, [id](const auto& item)
                    {
                        return item.executor->GetTaskID() == id;
                    });
            });
    }

    /*! \brief Remove a function from the owner thread's queue, if it exists
     * @returns a boolean value indicating whether or not the function was successfully dequeued */
    virtual bool Dequeue(TaskID id) override
    {
        if (!id)
        {
            return false;
        }

        std::unique_lock lock(m_mutex);

        if (Dequeue_Internal(id))
        {
            return true;
        }

        return false;
    }

    virtual bool TakeOwnershipOfTask(TaskID id, ITaskExecutor* executor) override
    {
        AssertThrow(!Threads::IsOnThread(m_owner_thread));

        AssertThrow(id.IsValid());
        AssertThrow(executor != nullptr);

        TaskExecutorBase* executor_casted = static_cast<TaskExecutorBase*>(executor);

        std::unique_lock lock(m_mutex);

        const auto it = m_queue.FindIf([id](const ScheduledTask& item)
            {
                if (!item.executor)
                {
                    return false;
                }

                return item.executor->GetTaskID() == id;
            });

        AssertThrow(it != m_queue.End());

        // if (it == m_queue.End()) {
        //     return false;
        // }

        ScheduledTask& scheduled_task = *it;

        if (scheduled_task.owns_executor)
        {
            AssertThrow(scheduled_task.executor != nullptr);
            AssertThrow(scheduled_task.executor != executor_casted);

            delete scheduled_task.executor;
        }

        // Release memory from the UniquePtr and assign it to the ScheduledTask
        // the ScheduledTask will delete the executor when it is destructed.
        scheduled_task.executor = executor_casted;
        scheduled_task.notifier = &executor_casted->GetNotifier();
        scheduled_task.owns_executor = true;

        return true;
    }

    virtual bool HasWorkAssignedFromThread(ThreadID thread_id) const override
    {
        std::unique_lock lock(m_mutex);

        return AnyOf(m_queue, [thread_id](const ScheduledTask& item)
            {
                return item.executor->GetInitiatorThreadID() == thread_id;
            });
    }

    // /* Move all the next pending task in the queue to an external container. */
    // template <class Container>
    // void AcceptNext(Container &out_container)
    // {
    //     AssertThrow(Threads::IsOnThread(m_owner_thread));

    //     std::unique_lock lock(m_mutex);

    //     if (m_queue.Any()) {
    //         auto &front = m_queue.Front();
    //         out_container.Push(std::move(front));
    //         m_queue.PopFront();

    //         m_num_enqueued.Decrement(1u, MemoryOrder::RELEASE);
    //     }
    // }

    /* Move all tasks in the queue to an external container. */
    template <class Container>
    void AcceptAll(Container& out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

        std::unique_lock lock(m_mutex);

        for (auto it = m_queue.Begin(); it != m_queue.End(); ++it)
        {
            out_container.Push(std::move(*it));
            m_num_enqueued.Decrement(1, MemoryOrder::RELEASE);
        }

        m_queue.Clear();

        lock.unlock();

        WakeUpOwnerThread();
    }

    /*! \brief Move all tasks in the queue to an external container. Blocks the current thread until there are tasks to execute, or the scheduler is stopped.
     * @returns a boolean value indicating whether or not the scheduler was stopped.
     */
    template <class Container>
    bool WaitForTasks(Container& out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

#ifdef HYP_DEBUG_MODE
        if (!out_container.Empty())
        {
            DebugLog(LogType::Warn, "Warning: Container is not empty when calling WaitForTasks().");
        }
#endif

        std::unique_lock lock(m_mutex);

        if (!SchedulerBase::WaitForTasks(lock))
        {
            return false;
        }

        for (auto it = m_queue.Begin(); it != m_queue.End(); ++it)
        {
            out_container.Push(std::move(*it));
            m_num_enqueued.Decrement(1, MemoryOrder::RELEASE);
        }

        m_queue.Clear();

        lock.unlock();

        WakeUpOwnerThread();

        return true;
    }

    /*! \brief Execute all scheduled tasks. May only be called from the creation thread. */
    template <class Lambda>
    void Flush(Lambda&& lambda)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

        AssertThrowMsg(
            !m_stop_requested.Get(MemoryOrder::RELAXED),
            "Scheduler::Flush() called after stop requested");

        std::unique_lock lock(m_mutex);

        while (m_queue.Any())
        {
            ScheduledTask& front = m_queue.Front();

            front.ExecuteWithLambda(lambda);

            m_num_enqueued.Decrement(1, MemoryOrder::RELEASE);
            m_queue.PopFront();
        }

        lock.unlock();

        WakeUpOwnerThread();
    }

private:
    void Enqueue_Internal(ScheduledTask&& scheduled_task)
    {
        const TaskID task_id { ++m_id_counter };

        scheduled_task.executor->SetTaskID(task_id);
        scheduled_task.executor->SetInitiatorThreadID(Threads::CurrentThreadID());
        scheduled_task.executor->SetAssignedScheduler(this);

        m_queue.PushBack(std::move(scheduled_task));
        m_num_enqueued.Increment(1, MemoryOrder::RELEASE);
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

        m_num_enqueued.Decrement(1, MemoryOrder::RELEASE);

        return true;
    }

    Array<ScheduledTask> m_queue;
};
} // namespace threading

using threading::Scheduler;
using threading::SchedulerBase;

} // namespace hyperion

#endif

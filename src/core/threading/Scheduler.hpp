/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_SCHEDULER_HPP
#define HYPERION_CORE_SCHEDULER_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/functional/Proc.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Threads.hpp>
#include <core/Util.hpp>

#include <Types.hpp>
#include <core/Defines.hpp>

#include <utility>
#include <type_traits>
#include <mutex>
#include <condition_variable>

namespace hyperion {
namespace threading {

class HYP_API SchedulerBase
{
public:
    using SemaphoreType = Semaphore<int32, detail::ConditionVarSemaphoreImpl<int32>>;

    SchedulerBase()                                             = delete;
    SchedulerBase(const SchedulerBase &other)                   = delete;
    SchedulerBase &operator=(const SchedulerBase &other)        = delete;
    SchedulerBase(SchedulerBase &&other) noexcept               = delete;
    SchedulerBase &operator=(SchedulerBase &&other) noexcept    = delete;
    virtual ~SchedulerBase()                                    = default;

    HYP_FORCE_INLINE ThreadID GetOwnerThread() const
        { return m_owner_thread; }

    /*! \brief Set the given thread ID to be the owner thread of this Scheduler.
     *  Tasks are to be enqueued from any other thread, and executed only from the owner thread.
     */
    HYP_FORCE_INLINE void SetOwnerThread(ThreadID owner_thread)
        { m_owner_thread = owner_thread; }

    void RequestStop();

    virtual void Await(TaskID id) = 0;
    virtual bool Dequeue(TaskID id) = 0;
    virtual bool TakeOwnershipOfTask(TaskID id, TaskExecutorBase *executor) = 0;
    virtual bool IsWaitingOnTaskFromThread(ThreadID thread_id) const = 0;

protected:
    SchedulerBase(ThreadID owner_thread)
        : m_owner_thread(owner_thread)
    {
    }

    void WakeUpOwnerThread();
    bool WaitForTasks(std::unique_lock<std::mutex> &lock);

    uint                    m_id_counter = 0;
    AtomicVar<uint>         m_num_enqueued { 0 };
    AtomicVar<bool>         m_stop_requested { false };

    mutable std::mutex      m_mutex;
    std::condition_variable m_has_tasks;
    std::condition_variable m_task_executed;

    ThreadID                m_owner_thread;
};

template <class... TaskArgTypes>
class Scheduler : public SchedulerBase
{
public:
    using TaskExecutorType = TaskExecutor<TaskArgTypes...>;

    struct ScheduledTask
    {
        // The executor/task memory
        TaskExecutorType            *executor = nullptr;

        // If the executor is owned by the scheduler, it will be deleted when this object is destroyed
        bool                        owns_executor = false;

        // Atomic counter to increment when the task is completed (used for batch tasks)
        SemaphoreType               *semaphore = nullptr;

        // Condition variable to notify when the task has been executed (owned by the scheduler)
        std::condition_variable     *task_executed = nullptr;

        // Callback to be executed after the task is completed
        OnTaskCompletedCallback     callback;

        ScheduledTask() = default;

        ScheduledTask(const ScheduledTask &other)               = delete;
        ScheduledTask &operator=(const ScheduledTask &other)    = delete;

        ScheduledTask(ScheduledTask &&other) noexcept
            : executor(other.executor),
              owns_executor(other.owns_executor),
              semaphore(other.semaphore),
              task_executed(other.task_executed),
              callback(std::move(other.callback))
        {
            other.executor = nullptr;
            other.owns_executor = false;
            other.semaphore = nullptr;
            other.task_executed = nullptr;
        }

        ScheduledTask &operator=(ScheduledTask &&other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            if (owns_executor) {
                delete executor;
            }

            executor = other.executor;
            owns_executor = other.owns_executor;
            semaphore = other.semaphore;
            task_executed = other.task_executed;
            callback = std::move(other.callback);

            other.executor = nullptr;
            other.owns_executor = false;
            other.semaphore = nullptr;
            other.task_executed = nullptr;

            return *this;
        }

        ~ScheduledTask()
        {
            if (owns_executor) {
                delete executor;
            }
        }

        template <class Lambda>
        void ExecuteWithLambda(Lambda &&lambda)
        {
            lambda(*executor);

            int counter_value = 0;

            if (semaphore != nullptr) {
                counter_value = semaphore->Release(1);
            }

            if (callback.IsValid()) {
                callback(counter_value);
            }

            task_executed->notify_all();
        }

        template <class ...Args>
        void Execute(Args &&... args)
        {
            executor->Execute(std::forward<Args>(args)...);

            int counter_value = 0;

            if (semaphore != nullptr) {
                counter_value = semaphore->Release(1);
            }

            if (callback.IsValid()) {
                callback(counter_value);
            }

            task_executed->notify_all();
        }
    };
    
    Scheduler()
        : SchedulerBase(Threads::CurrentThreadID())
    {
    }

    Scheduler(const Scheduler &other)                   = delete;
    Scheduler &operator=(const Scheduler &other)        = delete;
    Scheduler(Scheduler &&other) noexcept               = default;
    Scheduler &operator=(Scheduler &&other) noexcept    = default;
    virtual ~Scheduler() override                       = default;

    HYP_NODISCARD HYP_FORCE_INLINE
    uint NumEnqueued() const
        { return m_num_enqueued.Get(MemoryOrder::ACQUIRE); }

    HYP_NODISCARD HYP_FORCE_INLINE
    const Array<ScheduledTask> &GetEnqueuedTasks() const
        { return m_queue; }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     *  called from a non-owner thread. 
     *  \param fn The function to execute */
    template <class Lambda>
    auto Enqueue(Lambda &&fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Lambda>::ReturnType, TaskArgTypes...>
    {
        using ReturnType = typename FunctionTraits<Lambda>::ReturnType;

        std::unique_lock lock(m_mutex);

        TaskExecutorInstance<ReturnType, TaskArgTypes...> *executor = new TaskExecutorInstance<ReturnType, TaskArgTypes...>(std::forward<Lambda>(fn));

        ScheduledTask scheduled_task;
        scheduled_task.executor = executor;
        scheduled_task.owns_executor = (flags & TaskEnqueueFlags::FIRE_AND_FORGET);
        scheduled_task.task_executed = &m_task_executed;

        EnqueueInternal(std::move(scheduled_task));

        lock.unlock();

        WakeUpOwnerThread();

        return Task<ReturnType, TaskArgTypes...>(
            executor->GetTaskID(),
            this,
            executor,
            !(flags & TaskEnqueueFlags::FIRE_AND_FORGET)
        );
    }

    /*! \brief Enqueue a TaskExecutor to be executed on the owner thread. This is to be
     *  called from a non-owner thread.
     *  \internal Used by TaskSystem to enqueue batches of tasks.
     *  \param executor_ptr The TaskExecutor to execute (owned by the caller)
     *  \param atomic_counter A pointer to an atomic uint variable that is incremented upon completion.
     *  \param callback A callback to be executed after the task is completed. */
    TaskID EnqueueTaskExecutor(TaskExecutorType *executor_ptr, SemaphoreType *semaphore, OnTaskCompletedCallback &&callback = nullptr)
    {
        std::unique_lock lock(m_mutex);

        ScheduledTask scheduled_task;
        scheduled_task.executor = executor_ptr;
        scheduled_task.owns_executor = false;
        scheduled_task.semaphore = semaphore;
        scheduled_task.task_executed = &m_task_executed;
        scheduled_task.callback = std::move(callback);

        EnqueueInternal(std::move(scheduled_task));

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

        if (!m_queue.Any() || !m_queue.Any([id](const auto &item) { return item.executor->GetTaskID() == id; })) {
            return;
        }

        m_task_executed.wait(lock, [this, id]
        {
            return !m_queue.Any() || !m_queue.Any([id](const auto &item)
            {
                return item.executor->GetTaskID() == id;
            });
        });
    }
    
    /*! \brief Remove a function from the owner thread's queue, if it exists
     * @returns a boolean value indicating whether or not the function was successfully dequeued */
    virtual bool Dequeue(TaskID id) override
    {
        if (!id) {
            return false;
        }

        std::unique_lock lock(m_mutex);

        if (DequeueInternal(id)) {
            return true;
        }

        return false;
    }

    virtual bool TakeOwnershipOfTask(TaskID id, TaskExecutorBase *executor) override
    {
        AssertThrow(!Threads::IsOnThread(m_owner_thread));

        AssertThrow(id.IsValid());

        TaskExecutorType *executor_casted = dynamic_cast<TaskExecutorType *>(executor);
        AssertThrowMsg(executor_casted != nullptr, "Invalid TaskExecutor type");

        std::unique_lock lock(m_mutex);

        const auto it = m_queue.FindIf([id](const ScheduledTask &item)
        {
            if (!item.executor) {
                return false;
            }

            return item.executor->GetTaskID() == id;
        });

        AssertThrow(it != m_queue.End());

        // if (it == m_queue.End()) {
        //     return false;
        // }

        ScheduledTask &scheduled_task = *it;

        if (scheduled_task.owns_executor) {
            AssertThrow(scheduled_task.executor != nullptr);
            AssertThrow(scheduled_task.executor != executor_casted);

            delete scheduled_task.executor;
        }

        // Release memory from the UniquePtr and assign it to the ScheduledTask
        // the ScheduledTask will delete the executor when it is destructed.
        scheduled_task.executor = executor_casted;
        scheduled_task.owns_executor = true;

        return true;
    }

    virtual bool IsWaitingOnTaskFromThread(ThreadID thread_id) const override
    {
        std::unique_lock lock(m_mutex);

        return m_queue.Any([thread_id](const ScheduledTask &item)
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
    void AcceptAll(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

        std::unique_lock lock(m_mutex);

        for (auto it = m_queue.Begin(); it != m_queue.End(); ++it) {
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
    bool WaitForTasks(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

#ifdef HYP_DEBUG_MODE
        if (!out_container.Empty()) {
            DebugLog(LogType::Warn, "Warning: Container is not empty when calling WaitForTasks().");
        }
#endif

        std::unique_lock lock(m_mutex);

        if (!SchedulerBase::WaitForTasks(lock)) {
            return false;
        }

        for (auto it = m_queue.Begin(); it != m_queue.End(); ++it) {
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
    void Flush(Lambda &&lambda)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

        AssertThrowMsg(
            !m_stop_requested.Get(MemoryOrder::RELAXED),
            "Scheduler::Flush() called after stop requested"
        );

        std::unique_lock lock(m_mutex);

        while (m_queue.Any()) {
            ScheduledTask &front = m_queue.Front();
            
            front.ExecuteWithLambda(lambda);

            m_num_enqueued.Decrement(1, MemoryOrder::RELEASE);
            m_queue.PopFront();
        }

        lock.unlock();

        WakeUpOwnerThread();
    }
    
private:
    void EnqueueInternal(ScheduledTask &&scheduled_task)
    {
        const TaskID task_id { ++m_id_counter };

        scheduled_task.executor->SetTaskID(task_id);
        scheduled_task.executor->SetInitiatorThreadID(Threads::CurrentThreadID());
        scheduled_task.executor->SetAssignedScheduler(this);

        m_queue.PushBack(std::move(scheduled_task));
        m_num_enqueued.Increment(1, MemoryOrder::RELEASE);
    }

    bool DequeueInternal(TaskID id)
    {
        const auto it = m_queue.FindIf([&id](const auto &item)
        {
            return item.executor->GetTaskID() == id;
        });;

        if (it == m_queue.End()) {
            return false;
        }

        m_queue.Erase(it);

        m_num_enqueued.Decrement(1, MemoryOrder::RELEASE);

        return true;
    }

    Array<ScheduledTask>    m_queue;
};
} // namespace threading

using threading::Scheduler;
using threading::SchedulerBase;

} // namespace hyperion

#endif

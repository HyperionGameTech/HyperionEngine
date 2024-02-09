#ifndef HYPERION_V2_CORE_SCHEDULER_H
#define HYPERION_V2_CORE_SCHEDULER_H

// #define HYP_SCHEDULER_USE_ATOMIC_LOCK 1

#include <core/lib/AtomicSemaphore.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/Proc.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/lib/Optional.hpp>
#include <core/Thread.hpp>
#include <core/Task.hpp>
#include <core/Util.hpp>
#include <Threads.hpp>

#include <Types.hpp>
#include <util/Defines.hpp>

#include <utility>
#include <type_traits>
#include <mutex>
#include <condition_variable>

namespace hyperion::v2 {

class SchedulerBase
{
public:
    SchedulerBase()                                             = delete;
    SchedulerBase(const SchedulerBase &other)                   = delete;
    SchedulerBase &operator=(const SchedulerBase &other)        = delete;
    SchedulerBase(SchedulerBase &&other) noexcept               = delete;
    SchedulerBase &operator=(SchedulerBase &&other) noexcept    = delete;
    ~SchedulerBase()                                            = default;

    /*! \brief Set the given thread ID to be the owner thread of this Scheduler.
     *  Tasks are to be enqueued from any other thread, and executed only from the owner thread.
     */
    void SetOwnerThread(ThreadID owner_thread)
    {
        m_owner_thread = owner_thread;
    }

    void RequestStop();

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

    std::mutex              m_mutex;
    std::condition_variable m_has_tasks;

    ThreadID                m_owner_thread;
};

template <class TaskType>
class Scheduler : public SchedulerBase
{
public:
    using Task = TaskType;

    struct ScheduledTask
    {
        Task            task;
        AtomicVar<uint> *atomic_counter = nullptr;

        ScheduledTask() = default;

        ScheduledTask(Task &&task, AtomicVar<uint> *atomic_counter)
            : task(std::move(task)),
              atomic_counter(atomic_counter)
        {
        }

        ScheduledTask(const ScheduledTask &other) = delete;
        ScheduledTask &operator=(const ScheduledTask &other) = delete;

        ScheduledTask(ScheduledTask &&other) noexcept
            : task(std::move(other.task)),
              atomic_counter(other.atomic_counter)
        {
            other.atomic_counter = nullptr;
        }

        ScheduledTask &operator=(ScheduledTask &&other) noexcept
        {
            if (std::addressof(other) == this) {
                return *this;
            }

            task = std::move(other.task);
            atomic_counter = other.atomic_counter;

            other.atomic_counter = nullptr;

            return *this;
        }

        ~ScheduledTask() = default;

        template <class Lambda>
        void ExecuteWithLambda(Lambda &&lambda)
        {
            lambda(task);

            if (atomic_counter != nullptr) {
                atomic_counter->Increment(1, MemoryOrder::RELAXED);
            }
        }

        template <class ...Args>
        void Execute(Args &&... args)
        {
            task.Execute(std::forward<Args>(args)...);

            if (atomic_counter != nullptr) {
                atomic_counter->Increment(1, MemoryOrder::RELAXED);
            }
        }
    };

    using ScheduledFunctionQueue    = Array<ScheduledTask>;
    using Iterator                  = typename ScheduledFunctionQueue::Iterator;
    
    Scheduler()
        : SchedulerBase(Threads::CurrentThreadID())
    {
    }

    Scheduler(const Scheduler &other) = delete;
    Scheduler &operator=(const Scheduler &other) = delete;
    Scheduler(Scheduler &&other) = default;
    Scheduler &operator=(Scheduler &&other) = default;
    ~Scheduler() = default;

    HYP_FORCE_INLINE
    uint NumEnqueued() const
    {
        return m_num_enqueued.Get(MemoryOrder::RELAXED);
    }

    HYP_FORCE_INLINE
    const Array<ScheduledTask> &GetEnqueuedTasks() const
        { return m_enqueued_tasks; }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     * called from a non-owner thread. 
     * @param fn The task to execute
     *
     */
    TaskID Enqueue(Task &&fn)
    {
        std::unique_lock lock(m_mutex);

        auto result = EnqueueInternal(std::forward<Task>(fn), nullptr /* No atomic counter */);

        lock.unlock();

        WakeUpOwnerThread();

        return result;
    }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     * called from a non-owner thread.
     * @param fn The task to execute
     * @param atomic_counter A pointer to an atomic uint variable that is incremented upon completion.
     */
    TaskID Enqueue(Task &&fn, AtomicVar<uint> *atomic_counter)
    {
        // debugging
        std::unique_lock lock(m_mutex);

        auto result = EnqueueInternal(std::forward<Task>(fn), atomic_counter);

        lock.unlock();

        WakeUpOwnerThread();

        return result;
    }
    
    /*! \brief Remove a function from the owner thread's queue, if it exists
     * @returns a boolean value indicating whether or not the function was successfully dequeued */
    bool Dequeue(TaskID id)
    {
        if (id == Task::empty_id) {
            return false;
        }

        std::unique_lock lock(m_mutex);

        if (DequeueInternal(id)) {
            return true;
        }

        return false;
    }

    /* Move all the next pending task in the queue to an external container. */
    template <class Container>
    void AcceptNext(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));
        
        std::unique_lock lock(m_mutex);

        if (m_enqueued_tasks.Any()) {
            auto &front = m_enqueued_tasks.Front();
            out_container.Push(std::move(front));
            m_enqueued_tasks.PopFront();

            m_num_enqueued.Decrement(1u, MemoryOrder::RELAXED);
        }
    }
    
    /* Move all tasks in the queue to an external container. */
    template <class Container>
    void AcceptAll(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

        std::unique_lock lock(m_mutex);

        for (auto it = m_enqueued_tasks.Begin(); it != m_enqueued_tasks.End(); ++it) {
            out_container.Push(std::move(*it));
        }

        m_enqueued_tasks.Clear();
        m_num_enqueued.Set(0, MemoryOrder::RELAXED);
    }
    
    /*! \brief Move all tasks in the queue to an external container. Blocks the current thread until there are tasks to execute, or the scheduler is stopped.
        * @returns a boolean value indicating whether or not the scheduler was stopped.
    */
    template <class Container>
    bool WaitForTasks(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

        std::unique_lock lock(m_mutex);

        if (!SchedulerBase::WaitForTasks(lock)) {
            return false;
        }

        // If stop was requested, return immediately
        if (m_stop_requested.Get(MemoryOrder::RELAXED)) {
            return false;
        }

        for (auto it = m_enqueued_tasks.Begin(); it != m_enqueued_tasks.End(); ++it) {
            out_container.Push(std::move(*it));
        }

        m_enqueued_tasks.Clear();
        m_num_enqueued.Set(0, MemoryOrder::RELAXED);

        return true;
    }

    /*! Execute all scheduled tasks. May only be called from the creation thread.
     */
    template <class Executor>
    void Flush(Executor &&executor)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

        AssertThrowMsg(
            !m_stop_requested.Get(MemoryOrder::RELAXED),
            "Scheduler::Flush() called after stop requested"
        );

        std::unique_lock lock(m_mutex);

        while (m_enqueued_tasks.Any()) {
            auto &front = m_enqueued_tasks.Front();
            
            executor(front.task);

            if (front.atomic_counter != nullptr) {
                front.atomic_counter->Increment(1, MemoryOrder::RELAXED);
            }

            m_enqueued_tasks.PopFront();
        }

        m_num_enqueued.Set(0, MemoryOrder::RELAXED);
    }
    
private:
    TaskID EnqueueInternal(Task &&fn, AtomicVar<uint> *atomic_counter = nullptr)
    {
        fn.id = ++m_id_counter;

        m_enqueued_tasks.PushBack(ScheduledTask(std::forward<Task>(fn), atomic_counter));

        m_num_enqueued.Increment(1, MemoryOrder::RELAXED);

        return m_enqueued_tasks.Back().task.id;
    }

    Iterator Find(TaskID id)
    {
        return m_enqueued_tasks.FindIf([&id](const auto &item)
        {
            return item.task.id == id;
        });
    }

    bool DequeueInternal(TaskID id)
    {
        const auto it = Find(id);

        if (it == m_enqueued_tasks.End()) {
            return false;
        }

        m_enqueued_tasks.Erase(it);

        m_num_enqueued.Decrement(1, MemoryOrder::RELAXED);

        return true;
    }

    ScheduledFunctionQueue  m_enqueued_tasks;
};

} // namespace hyperion::v2

#endif

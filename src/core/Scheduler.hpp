#ifndef HYPERION_V2_CORE_SCHEDULER_H
#define HYPERION_V2_CORE_SCHEDULER_H

#define HYP_SCHEDULER_USE_ATOMIC_LOCK 1

#include "lib/AtomicSemaphore.hpp"
#include "lib/DynArray.hpp"
#include "lib/FixedArray.hpp"
#include "lib/Proc.hpp"
#include <core/Thread.hpp>
#include <Threads.hpp>
#include <core/lib/Optional.hpp>

#include <Util.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>

#include <functional>
#include <atomic>
#include <thread>
#include <tuple>
#include <utility>
#include <deque>
#include <type_traits>

#if !HYP_SCHEDULER_USE_ATOMIC_LOCK
#include <mutex>
#include <condition_variable>
#endif

namespace hyperion::v2 {

enum class TaskPriority : UInt
{
    MEDIUM = 0,
    HIGH = 1
};

struct TaskID
{
    UInt value { 0 };
    
    TaskID &operator=(UInt id)
    {
        value = id;

        return *this;
    }
    
    TaskID &operator=(const TaskID &other) = default;

    bool operator==(UInt id) const { return value == id; }
    bool operator!=(UInt id) const { return value != id; }
    bool operator==(const TaskID &other) const { return value == other.value; }
    bool operator!=(const TaskID &other) const { return value != other.value; }
    bool operator<(const TaskID &other) const { return value < other.value; }

    explicit operator bool() const { return value != 0; }
};

template <class ReturnType, class ...Args>
struct Task
{
    using Function = Proc<ReturnType, Args...>;

    TaskID id;
    Function fn;
    TaskPriority priority = TaskPriority::MEDIUM;

    constexpr static TaskID empty_id = TaskID { 0 };

    template <class Lambda>
    Task(Lambda &&lambda)
        : id { },
          fn(std::forward<Lambda>(lambda))
    {
    }

    Task(const Task &other) = delete;
    Task &operator=(const Task &other) = delete;
    Task(Task &&other) noexcept
        : id(other.id),
          fn(std::move(other.fn)),
          priority(other.priority)
    {
        other.id = {};
        other.priority = TaskPriority::MEDIUM;
    }

    Task &operator=(Task &&other) noexcept
    {
        id = other.id;
        fn = std::move(other.fn);
        priority = other.priority;

        other.id = {};
        other.priority = TaskPriority::MEDIUM;

        return *this;
    }
    
    ~Task() = default;
    
    HYP_FORCE_INLINE ReturnType Execute(Args... args)
        { return fn(std::forward<Args>(args)...); }
    
    HYP_FORCE_INLINE ReturnType operator()(Args... args)
        { return fn(std::forward<Args>(args)...); }
};

template <class TaskType>
class Scheduler
{
public:
    using Task = TaskType;
    using TaskID = TaskID;

    struct ScheduledTask
    {
        Task task;
        std::atomic<UInt> *atomic_counter = nullptr;

        ScheduledTask() = default;

        ScheduledTask(Task &&task, std::atomic<UInt> *atomic_counter)
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
                atomic_counter->fetch_add(1u, std::memory_order_relaxed);
            }
        }

        template <class ...Args>
        void Execute(Args &&... args)
        {
            task.Execute(std::forward<Args>(args)...);

            if (atomic_counter != nullptr) {
                atomic_counter->fetch_add(1u, std::memory_order_relaxed);
            }
        }
    };

    using ScheduledFunctionQueue = Array<ScheduledTask>;
    using Iterator = typename ScheduledFunctionQueue::Iterator;
    
    Scheduler()
        : m_owner_thread(Threads::CurrentThreadID())
    {
    }

    Scheduler(const Scheduler &other) = delete;
    Scheduler &operator=(const Scheduler &other) = delete;
    Scheduler(Scheduler &&other) = default;
    Scheduler &operator=(Scheduler &&other) = default;
    ~Scheduler() = default;

    HYP_FORCE_INLINE UInt NumEnqueued() const
        { return m_num_enqueued.load(std::memory_order_relaxed); }

    /*! \brief Set the given thread ID to be the owner thread of this Scheduler.
     *  Tasks are to be enqueued from any other thread, and executed only from the owner thread.
     */
    void SetOwnerThread(ThreadID owner_thread)
    {
        m_owner_thread = owner_thread;
    }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     * called from a non-owner thread. 
     * @param fn The task to execute
     *
     */
    TaskID Enqueue(Task &&fn)
    {
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        auto result = EnqueueInternal(std::forward<Task>(fn), nullptr /* No atomic counter */);

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#endif

        return result;
    }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     * called from a non-owner thread.
     * @param fn The task to execute
     * @param atomic_counter A pointer to an atomic UInt variable that is incremented upon completion.
     */
    TaskID Enqueue(Task &&fn, std::atomic<UInt> *atomic_counter)
    {
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        auto result = EnqueueInternal(std::forward<Task>(fn), atomic_counter);

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#endif

        return result;
    }
    
    /*! \brief Remove a function from the owner thread's queue, if it exists
     * @returns a boolean value indicating whether or not the function was successfully dequeued */
    bool Dequeue(TaskID id)
    {
        if (id == Task::empty_id) {
            return false;
        }
        
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        if (DequeueInternal(id)) {
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
            m_sp.Signal();
#else
            if (m_scheduled_functions.Empty()) {
                lock.unlock();

                m_is_flushed.notify_all();
            }
#endif

            return true;
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#endif

        return false;
    }

    /*! \brief For each item, remove a function from the owner thread's queue, if it exists
     * @returns a FixedArray of booleans, each holding whether or not the corresponding function was successfully dequeued */
    template <SizeType Sz>
    FixedArray<bool, Sz> DequeueMany(const FixedArray<TaskID, Sz> &ids)
    {
        FixedArray<bool, Sz> results {};

        if (ids.Empty()) {
            return results;
        }
        
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        for (SizeType i = 0; i < ids.Size(); i++) {
            const auto &id = ids[i];

            if (id == Task::empty_id) {
                results[i] = false;

                continue;
            }

            if (DequeueInternal(id)) {
                results[i] = true;
            }
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        if (m_scheduled_functions.Empty()) {
            lock.unlock();

            m_is_flushed.notify_all();
        }
#endif

        return results;
    }

    /*! If the current thread is the creation thread, the scheduler will be flushed and immediately return.
     * Otherwise, the thread will block until all tasks have been executed.
     */
    HYP_FORCE_INLINE void FlushOrWait()
    {
        FlushOrWait([](auto &fn) { fn(); });
    }

    /*! If the current thread is the creation thread, the scheduler will be flushed and immediately return.
     * Otherwise, the thread will block until all tasks have been executed.
     */
    template <class Executor>
    void FlushOrWait(Executor &&executor)
    {
        if (Threads::IsOnThread(m_owner_thread)) {
            Flush(std::forward<Executor>(executor));

            return;
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        while (m_num_enqueued.load() != 0u);
#else
        std::unique_lock lock(m_mutex);
        m_is_flushed.wait(lock, [this] { return m_num_enqueued.load() == 0u; });
#endif
    }

    /* Move all the next pending task in the queue to an external container. */
    template <class Container>
    void AcceptNext(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));
        
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        if (m_scheduled_functions.Any()) {
            auto &front = m_scheduled_functions.Front();
            out_container.Push(std::move(front));
            m_scheduled_functions.PopFront();

            m_num_enqueued.fetch_sub(1u, std::memory_order_relaxed);
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        lock.unlock();

        m_is_flushed.notify_all();
#endif
    }
    
    /* Move all tasks in the queue to an external container. */
    template <class Container>
    void AcceptAll(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        for (auto it = m_scheduled_functions.Begin(); it != m_scheduled_functions.End(); ++it) {
            out_container.Push(std::move(*it));
        }

        m_scheduled_functions.Clear();
        m_num_enqueued.store(0u, std::memory_order_relaxed);

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        lock.unlock();

        m_is_flushed.notify_all();
#endif
    }

    /*! Execute all scheduled tasks. May only be called from the creation thread.
     */
    template <class Executor>
    void Flush(Executor &&executor)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        while (m_scheduled_functions.Any()) {
            auto &front = m_scheduled_functions.Front();
            
            executor(front.task);

            if (front.atomic_counter != nullptr) {
                front.atomic_counter->fetch_add(1u, std::memory_order_relaxed);
            }

            m_scheduled_functions.PopFront();
        }

        m_num_enqueued.store(0u, std::memory_order_relaxed);

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        lock.unlock();

        m_is_flushed.notify_all();
#endif
    }
    
private:
    TaskID EnqueueInternal(Task &&fn, std::atomic<UInt> *atomic_counter = nullptr)
    {
        fn.id = ++m_id_counter;

        m_scheduled_functions.PushBack(ScheduledTask(std::forward<Task>(fn), atomic_counter));

        m_num_enqueued.fetch_add(1u, std::memory_order_relaxed);

        return m_scheduled_functions.Back().task.id;
    }

    Iterator Find(TaskID id)
    {
        return m_scheduled_functions.FindIf([&id](const auto &item) {
            return item.task.id == id;
        });
    }

    bool DequeueInternal(TaskID id)
    {
        const auto it = Find(id);

        if (it == m_scheduled_functions.End()) {
            return false;
        }

        m_scheduled_functions.Erase(it);

        m_num_enqueued.fetch_sub(1u, std::memory_order_relaxed);

        return true;
    }

    UInt m_id_counter = 0;
    std::atomic_uint m_num_enqueued { 0 };
    ScheduledFunctionQueue m_scheduled_functions;

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
    BinarySemaphore m_sp;
#else
    std::mutex m_mutex;
    std::condition_variable m_is_flushed;
#endif

    ThreadID m_owner_thread;
};

} // namespace hyperion::v2

#endif

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TASK_SYSTEM_HPP
#define HYPERION_TASK_SYSTEM_HPP

#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/functional/Proc.hpp>
#include <core/functional/Delegate.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Defines.hpp>
#include <core/Traits.hpp>

#include <core/debug/Debug.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/math/MathUtil.hpp>

#include <Types.hpp>

#include <atomic>

// #define HYP_TASK_BATCH_DATA_RACE_DETECTION

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Tasks);

namespace threading {    

enum TaskThreadPoolName : uint32
{
    THREAD_POOL_GENERIC,
    THREAD_POOL_RENDER,
    THREAD_POOL_BACKGROUND,
    THREAD_POOL_MAX
};

class TaskThreadPool;
class TaskSystem;

using OnTaskBatchCompletedCallback = Proc<void>;

class TaskBatch
{
public:
    TaskSemaphore                           semaphore;
    uint32                                  num_enqueued = 0;

    /*! \brief The priority / pool lane for which to place
     * all of the threads in this batch into
     */
    TaskThreadPool                          *pool = nullptr;

    /* Tasks must remain constant from creation of the TaskBatch to completion. */
    Array<TaskExecutorInstance<void>>       executors;

    /* TaskRefs to be set by the TaskSystem, holding task ids and pointers to the threads
     * each task has been scheduled to. */
    Array<TaskRef>                          task_refs;

    /*! \brief An optional dependent batch to be ran after this one has been completed.
     *  \note This TaskBatch does not own next_batch, and will not delete it.
     *  proper cleanup must be done by the user. */
    TaskBatch                               *next_batch = nullptr;

#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
    DataRaceDetector                        data_race_detector;
#endif

    /*! \brief Delegate that is called when the TaskBatch is complete (before next_batch has completed) */
    Delegate<void>                          OnComplete;

    Array<StaticMessage>                    debug_names;

    ~TaskBatch()
    {
        AssertThrowMsg(IsCompleted(), "TaskBatch must be in completed state by the time the destructor is called!");
    }

    /*! \brief Add a task to be ran with this batch
     *  \note Do not call this function after the TaskBatch has been enqueued (before it has been completed). */
    HYP_FORCE_INLINE void AddTask(TaskExecutorInstance<void> &&executor, const StaticMessage &debug_name = StaticMessage())
    {
#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
        HYP_MT_CHECK_RW(data_race_detector);
#endif

        executors.PushBack(std::move(executor));
        debug_names.PushBack(debug_name);

        ++num_enqueued;

        semaphore.Produce(1);
    }

    /*! \brief Check if all tasks in the batch have been completed. */
    HYP_API bool IsCompleted() const;

    /*! \brief Block the current thread until all tasks have been marked as completed. */
    HYP_API void AwaitCompletion();

    /*! \brief Execute each non-enqueued task in serial (not async).
     *  \param execute_dependent_batches If true, the next_batch will also be executed. */
    void ExecuteBlocking(bool execute_dependent_batches = false)
    {
#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
        HYP_MT_CHECK_RW(data_race_detector);
#endif

        for (auto it = executors.Begin(); it != executors.End(); ++it) {
            it->Execute();

            semaphore.Release(1);
        }

        OnComplete();

        if (execute_dependent_batches && next_batch != nullptr) {
            next_batch->ExecuteBlocking(execute_dependent_batches);
        }
    }

    void ResetState()
    {
#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
        HYP_MT_CHECK_RW(data_race_detector);
#endif

        AssertThrowMsg(IsCompleted(), "TaskBatch::ResetState() must be called after all tasks have been completed");
        
        semaphore.SetValue(0);
        num_enqueued = 0;
        executors.Clear();
        task_refs.Clear();
        debug_names.Clear();
        next_batch = nullptr;

        OnComplete.RemoveAll(/* thread_safe */ false);
    }
};

class HYP_API TaskThreadPool
{
protected:
    TaskThreadPool();
    TaskThreadPool(Array<UniquePtr<TaskThread>> &&threads);

    TaskThreadPool(Name base_name, uint32 num_threads)
        : TaskThreadPool(TypeWrapper<TaskThread>(), base_name, num_threads)
    {
    }

    template <class TaskThreadType>
    TaskThreadPool(TypeWrapper<TaskThreadType>, Name base_name, uint32 num_threads)
    {
        static_assert(std::is_base_of_v<TaskThread, TaskThreadType>, "TaskThreadType must be a subclass of TaskThread");

        m_thread_mask = 0;

        m_threads.Reserve(num_threads);

        for (uint32 thread_index = 0; thread_index < num_threads; thread_index++) {
            UniquePtr<TaskThread> &thread = m_threads.PushBack(MakeUnique<TaskThreadType>(CreateTaskThreadID(base_name, thread_index)));

            m_thread_mask |= thread->GetID().GetMask();
        }
    }

public:
    TaskThreadPool(const TaskThreadPool &)                  = delete;
    TaskThreadPool &operator=(const TaskThreadPool &)       = delete;
    TaskThreadPool(TaskThreadPool &&) noexcept              = delete;
    TaskThreadPool &operator=(TaskThreadPool &&) noexcept   = delete;
    virtual ~TaskThreadPool();

    bool IsRunning() const;

    virtual void Start();
    virtual void Stop();
    virtual void Stop(Array<TaskThread *> &out_task_threads);

    HYP_FORCE_INLINE uint32 NumThreads() const
        { return uint32(m_threads.Size()); }

    HYP_FORCE_INLINE ThreadMask GetThreadMask() const
        { return m_thread_mask; }

    HYP_FORCE_INLINE TaskThread *GetTaskThread(ThreadID thread_id) const
    {
        const auto it = m_threads.FindIf([thread_id](const UniquePtr<TaskThread> &task_thread)
        {
            return task_thread->GetID() == thread_id;
        });

        if (it != m_threads.End()) {
            return it->Get();
        }

        return nullptr;
    }

    template <class Function>
    auto Enqueue(const StaticMessage &debug_name, Function &&fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        TaskThread *task_thread = GetNextTaskThread();

        return task_thread->GetScheduler().Enqueue(debug_name, std::forward<Function>(fn), flags);
    }

    template <class Function>
    auto Enqueue(Function &&fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        TaskThread *task_thread = GetNextTaskThread();

        return task_thread->GetScheduler().Enqueue(std::forward<Function>(fn), flags);
    }

    TaskThread *GetNextTaskThread();

protected:
    AtomicVar<uint32>               m_cycle { 0u };
    Array<UniquePtr<TaskThread>>    m_threads;
    ThreadMask                      m_thread_mask;

private:
    static ThreadID CreateTaskThreadID(Name base_name, uint32 thread_index);
};

class TaskSystem
{
public:
    HYP_API static TaskSystem &GetInstance();

    TaskSystem();

    TaskSystem(const TaskSystem &other)                 = delete;
    TaskSystem &operator=(const TaskSystem &other)      = delete;

    TaskSystem(TaskSystem &&other) noexcept             = delete;
    TaskSystem &operator=(TaskSystem &&other) noexcept  = delete;

    ~TaskSystem()                                       = default;

    HYP_FORCE_INLINE bool IsRunning() const
        { return m_running.Get(MemoryOrder::RELAXED); }

    HYP_API void Start();
    HYP_API void Stop();

    HYP_FORCE_INLINE TaskThreadPool &GetPool(TaskThreadPoolName pool_name) const
        { return *m_pools[uint32(pool_name)]; }

    HYP_FORCE_INLINE TaskThread *GetTaskThread(TaskThreadPoolName pool_name, ThreadID thread_id) const
        { return GetPool(pool_name).GetTaskThread(thread_id);  }

    HYP_FORCE_INLINE TaskThread *GetTaskThread(ThreadID thread_id) const
    {
        for (const UniquePtr<TaskThreadPool> &pool : m_pools) {
            if (TaskThread *task_thread = pool->GetTaskThread(thread_id)) {
                return task_thread;
            }
        }

        return nullptr;
    }

    template <class Function>
    auto Enqueue(const StaticMessage &debug_name, Function &&fn, TaskThreadPool &pool, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");
        
        return pool.Enqueue(debug_name, std::forward<Function>(fn), flags);
    }

    template <class Function>
    auto Enqueue(Function &&fn, TaskThreadPool &pool, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");
        
        return pool.Enqueue(std::forward<Function>(fn), flags);
    }

    template <class Function>
    auto Enqueue(const StaticMessage &debug_name, Function &&fn, TaskThreadPoolName pool_name = THREAD_POOL_GENERIC, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

        TaskThreadPool &pool = GetPool(pool_name);
        
        return pool.Enqueue(debug_name, std::forward<Function>(fn), flags);
    }

    template <class Function>
    auto Enqueue(Function &&fn, TaskThreadPoolName pool_name = THREAD_POOL_GENERIC, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

        TaskThreadPool &pool = GetPool(pool_name);
        
        return pool.Enqueue(std::forward<Function>(fn), flags);
    }

    /*! \brief Enqueue a batch of multiple Tasks. Each task will be enqueued to run in parallel.
     *  You will need to call AwaitCompletion() before the underlying TaskBatch is destroyed.
     *  If enqueuing a batch with dependent tasks via \ref{TaskBatch::next_batch}, ensure all tasks are added to the next batch (and any proceding next batches in the chain)
     *  before calling this.
     *  \param batch Pointer to the TaskBatch to enqueue
     *  \param callback Optional callback to be called when all tasks in the batch have finished executing.
     */
    HYP_API TaskBatch *EnqueueBatch(TaskBatch *batch);

    /*! \brief Dequeue each task in a TaskBatch. A potentially expensive operation,
     * as each task will have to individually be dequeued, performing a lock operation.
     * @param batch Pointer to the TaskBatch to dequeue
     * @returns A Array<bool> containing for each task that has been enqueued, whether or not
     * it was successfully dequeued.
     */
    HYP_API Array<bool> DequeueBatch(TaskBatch *batch);

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    void ParallelForEach(const StaticMessage &debug_name, TaskThreadPool &pool, uint32 num_batches, uint32 num_items, CallbackFunction &&cb)
    {
        if (num_items == 0) {
            return;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        TaskBatch batch;
        batch.pool = &pool;

        const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

        for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
            batch.AddTask([batch_index, items_per_batch, num_items, &cb](...)
            {
                const uint32 offset_index = batch_index * items_per_batch;
                const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint32 i = offset_index; i < max_index; i++) {
                    cb(i, batch_index);
                }
            }, debug_name);
        }

        EnqueueBatch(&batch);
        batch.AwaitCompletion();
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    void ParallelForEach(TaskThreadPool &pool, uint32 num_batches, uint32 num_items, CallbackFunction &&cb)
    {
        ParallelForEach(
            HYP_STATIC_MESSAGE("ParallelForEach"),
            pool,
            num_batches,
            num_items,
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(const StaticMessage &debug_name, TaskThreadPool &pool, uint32 num_items, CallbackFunction &&cb)
    {
        ParallelForEach(
            debug_name,
            pool,
            pool.NumThreads(),
            num_items,
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPool &pool, uint32 num_items, CallbackFunction &&cb)
    {
        ParallelForEach(
            pool,
            pool.NumThreads(),
            num_items,
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(const StaticMessage &debug_name, uint32 num_items, CallbackFunction &&cb)
    {
        TaskThreadPool &pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            debug_name,
            pool,
            pool.NumThreads(),
            num_items,
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(uint32 num_items, CallbackFunction &&cb)
    {
        TaskThreadPool &pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            pool,
            pool.NumThreads(),
            num_items,
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction>
    void ParallelForEach(const StaticMessage &debug_name, TaskThreadPool &pool, uint32 num_batches, Container &&items, CallbackFunction &&cb)
    {
        //static_assert(Container::is_contiguous, "Container must be contiguous to use ParallelForEach");

        const uint32 num_items = uint32(items.Size());

        if (num_items == 0) {
            return;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        TaskBatch batch;
        batch.pool = &pool;

        const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

        auto *data_ptr = items.Data();

        for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
            batch.AddTask([data_ptr, batch_index, items_per_batch, num_items, &cb](...)
            {
                const uint32 offset_index = batch_index * items_per_batch;
                const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint32 i = offset_index; i < max_index; i++) {
                    cb(*(data_ptr + i), i, batch_index);
                }
            }, debug_name);
        }

        EnqueueBatch(&batch);
        batch.AwaitCompletion();
    }
    
    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction>
    void ParallelForEach(TaskThreadPool &pool, uint32 num_batches, Container &&items, CallbackFunction &&cb)
    {
        ParallelForEach(
            HYP_STATIC_MESSAGE("ParallelForEach"),
            pool,
            num_batches,
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(const StaticMessage &debug_name, TaskThreadPool &pool, Container &&items, CallbackFunction &&cb)
    {
        ParallelForEach(
            debug_name,
            pool,
            pool.NumThreads(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPool &pool, Container &&items, CallbackFunction &&cb)
    {
        ParallelForEach(
            pool,
            pool.NumThreads(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(const StaticMessage &debug_name, Container &&items, CallbackFunction &&cb)
    {
        TaskThreadPool &pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            debug_name,
            pool,
            pool.NumThreads(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(Container &&items, CallbackFunction &&cb)
    {
        TaskThreadPool &pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            pool,
            pool.NumThreads(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    Array<Task<void>> ParallelForEach_Async(TaskThreadPool &pool, uint32 num_batches, uint32 num_items, CallbackFunction &&cb)
    {
        Array<Task<void>> tasks;

        if (num_items == 0) {
            return tasks;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

        for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
            const uint32 offset_index = batch_index * items_per_batch;
            const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

            if (offset_index >= max_index) {
                continue;
            }

            tasks.PushBack(Enqueue([batch_index, offset_index, max_index, _cb = std::forward<CallbackFunction>(cb)]() mutable -> void
            {
                for (uint32 i = offset_index; i < max_index; i++) {
                    _cb(i, batch_index);
                }
            }, pool, TaskEnqueueFlags::NONE));
        }

        return tasks;
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE Array<Task<void>> ParallelForEach_Async(TaskThreadPool &pool, uint32 num_items, CallbackFunction &&cb)
    {
        return ParallelForEach_Async(
            pool,
            pool.NumThreads(),
            num_items,
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE Array<Task<void>> ParallelForEach_Async(uint32 num_items, CallbackFunction &&cb)
    {
        TaskThreadPool &pool = GetPool(THREAD_POOL_GENERIC);

        return ParallelForEach_Async(
            pool,
            pool.NumThreads(),
            num_items,
            std::forward<CallbackFunction>(cb)
        );
    }
    
    template <class Container, class CallbackFunction, typename = std::enable_if_t< std::is_class_v< NormalizedType<Container> > > >
    HYP_NODISCARD Array<Task<void>> ParallelForEach_Async(TaskThreadPool &pool, uint32 num_batches, Container &&items, CallbackFunction &&cb)
    {
        //static_assert(Container::is_contiguous, "Container must be contiguous to use ParallelForEach");

        Array<Task<void>> tasks;

        const uint32 num_items = uint32(items.Size());

        if (num_items == 0) {
            return tasks;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

        auto *data_ptr = items.Data();

        for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
            tasks.PushBack(Enqueue([data_ptr, batch_index, items_per_batch, num_items, _cb = std::forward<CallbackFunction>(cb)]() mutable -> void
            {
                const uint32 offset_index = batch_index * items_per_batch;
                const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint32 i = offset_index; i < max_index; i++) {
                    _cb(*(data_ptr + i), i, batch_index);
                }
            }, pool, TaskEnqueueFlags::NONE));
        }

        return tasks;
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction, typename = std::enable_if_t< std::is_class_v< NormalizedType<Container> > > >
    HYP_FORCE_INLINE Array<Task<void>> ParallelForEach_Async(TaskThreadPool &pool, Container &&items, CallbackFunction &&cb)
    {
        return ParallelForEach_Async(
            pool,
            pool.NumThreads(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class CallbackFunction, typename = std::enable_if_t< std::is_class_v< NormalizedType<Container> > > >
    HYP_FORCE_INLINE Array<Task<void>> ParallelForEach_Async(Container &&items, CallbackFunction &&cb)
    {
        TaskThreadPool &pool = GetPool(THREAD_POOL_GENERIC);

        return ParallelForEach_Async(
            pool,
            pool.NumThreads(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb)
        );
    }

    HYP_FORCE_INLINE bool CancelTask(const TaskRef &task_ref)
    {
        if (!task_ref.IsValid()) {
            return false;
        }

        return task_ref.assigned_scheduler->Dequeue(task_ref.id);
    }

private:
    HYP_API TaskThread *GetNextTaskThread(TaskThreadPool &pool);

    Array<UniquePtr<TaskThreadPool>>    m_pools;
    Array<TaskBatch *>                  m_running_batches;
    AtomicVar<bool>                     m_running;
};

} // namespace threading

using TaskSystem = threading::TaskSystem;
using TaskBatch = threading::TaskBatch;
using TaskRef = threading::TaskRef;
using TaskThreadPool = threading::TaskThreadPool;
using TaskThreadPoolName = threading::TaskThreadPoolName;

} // namespace hyperion

#endif

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TASK_SYSTEM_HPP
#define HYPERION_TASK_SYSTEM_HPP

#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/logging/LoggerFwd.hpp>
#include <core/Defines.hpp>

#include <core/system/Debug.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/AtomicVar.hpp>

#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Tasks);

namespace threading {    

enum TaskThreadPoolName : uint
{
    THREAD_POOL_GENERIC,
    THREAD_POOL_RENDER,
    THREAD_POOL_MAX
};

struct TaskBatch
{
    AtomicVar<uint32>                       num_completed;
    uint32                                  num_enqueued = 0;

    /*! \brief The priority / pool lane for which to place
     * all of the threads in this batch into
     */
    TaskThreadPoolName                      pool = THREAD_POOL_GENERIC;

    /* Number of tasks must remain constant from creation of the TaskBatch,
     * to completion. */
    LinkedList<TaskExecutorInstance<void>>  executors;

    /* TaskRefs to be set by the TaskSystem, holding task ids and pointers to the threads
     * each task has been scheduled to. */
    Array<TaskRef>                          task_refs;

    /*! \brief An optional dependent batch to be ran after this one has been completed.
     *  \note This TaskBatch does not own next_batch, and will not delete it.
     *  proper cleanup must be done by the user. */
    TaskBatch                               *next_batch = nullptr;

    /*! \brief Add a task to be ran with this batch */
    HYP_FORCE_INLINE void AddTask(TaskExecutorInstance<void> &&executor)
        { executors.PushBack(std::move(executor)); }

    /*! \brief Check if all tasks in the batch have been completed.
     *  \param include_dependent_batches If true, the next_batch will also be checked. */
    HYP_FORCE_INLINE bool IsCompleted(bool include_dependent_batches = false) const
    {
        return (num_completed.Get(MemoryOrder::RELAXED) >= num_enqueued)
            && (!include_dependent_batches || !next_batch || next_batch->IsCompleted(include_dependent_batches));
    }

    /*! \brief Block the current thread until all tasks have been marked as completed.
     *  \param await_dependent_batches If true, the next_batch will also be awaited. */
    HYP_FORCE_INLINE void AwaitCompletion(bool await_dependent_batches = false)
    {
        while (!IsCompleted()) {
            HYP_WAIT_IDLE();
        }

        executors.Clear();

        if (await_dependent_batches && next_batch != nullptr) {
            next_batch->AwaitCompletion(await_dependent_batches);
        }
    }

    /*! \brief Execute each non-enqueued task in serial (not async).
     *  \param execute_dependent_batches If true, the next_batch will also be executed. */
    void ExecuteBlocking(bool execute_dependent_batches = false)
    {
        for (TaskThread::Scheduler::TaskExecutorType &executor : executors) {
            executor.Execute();
        }

        executors.Clear();

        if (execute_dependent_batches && next_batch != nullptr) {
            next_batch->ExecuteBlocking(execute_dependent_batches);
        }
    }

    void ResetState()
    {
        AssertThrowMsg(
            IsCompleted(false),
            "TaskBatch::ResetState() must be called after all tasks have been completed"
        );

        num_completed.Set(0, MemoryOrder::RELEASE);
        num_enqueued = 0;
        executors.Clear();
        task_refs.Clear();
        next_batch = nullptr;
    }
};

class TaskSystem
{
    struct TaskThreadPool
    {
        AtomicVar<uint>                 cycle { 0u };
        Array<UniquePtr<TaskThread>>    threads;
    };

public:
    HYP_API static TaskSystem &GetInstance();

    TaskSystem();

    TaskSystem(const TaskSystem &other)                 = delete;
    TaskSystem &operator=(const TaskSystem &other)      = delete;

    TaskSystem(TaskSystem &&other) noexcept             = delete;
    TaskSystem &operator=(TaskSystem &&other) noexcept  = delete;

    ~TaskSystem()                                       = default;

    HYP_FORCE_INLINE
    bool IsRunning() const
        { return m_running.Get(MemoryOrder::RELAXED); }

    HYP_API void Start();
    HYP_API void Stop();

    TaskThreadPool &GetPool(TaskThreadPoolName pool_name)
        { return m_pools[uint(pool_name)]; }

    template <class Lambda>
    auto Enqueue(Lambda &&fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE, TaskThreadPoolName pool_name = THREAD_POOL_GENERIC) -> Task<typename FunctionTraits<Lambda>::ReturnType>
    {
        AssertThrowMsg(
            IsRunning(),
            "TaskSystem::Start() must be called before enqueuing tasks"
        );
        
        TaskThreadPool &pool = GetPool(pool_name);

        TaskThread *task_thread = GetNextTaskThread(pool);

        return task_thread->GetScheduler().Enqueue(std::forward<Lambda>(fn), flags);
    }

    /*! \brief Enqueue a batch of multiple Tasks. Each task will be enqueued to run in parallel.
     * You will need to call AwaitCompletion() before the underlying TaskBatch is destroyed.
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
    template <class Lambda>
    void ParallelForEach(TaskThreadPoolName pool, uint32 num_batches, uint32 num_items, Lambda &&lambda)
    {
        if (num_items == 0) {
            return;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        TaskBatch batch;
        batch.pool = pool;

        const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

        for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
            batch.AddTask([batch_index, items_per_batch, num_items, &lambda](...)
            {
                const uint32 offset_index = batch_index * items_per_batch;
                const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint32 i = offset_index; i < max_index; i++) {
                    lambda(i, batch_index);
                }
            });
        }

        if (batch.executors.Size() > 1) {
            EnqueueBatch(&batch);
            batch.AwaitCompletion();
        } else if (batch.executors.Size() == 1) {
            // no point in enqueing for just 1 task, execute immediately
            batch.ExecuteBlocking();
        }
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class Lambda>
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPoolName priority, uint32 num_items, Lambda &&lambda)
    {
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            pool.threads.Size(),
            num_items,
            std::forward<Lambda>(lambda)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class Lambda>
    HYP_FORCE_INLINE void ParallelForEach(uint32 num_items, Lambda &&lambda)
    {
        constexpr auto priority = THREAD_POOL_GENERIC;
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            pool.threads.Size(),
            num_items,
            std::forward<Lambda>(lambda)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class Lambda>
    void ParallelForEach(TaskThreadPoolName pool, uint32 num_batches, Container &&items, Lambda &&lambda)
    {
        //static_assert(Container::is_contiguous, "Container must be contiguous to use ParallelForEach");

        const uint32 num_items = uint32(items.Size());

        if (num_items == 0) {
            return;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        TaskBatch batch;
        batch.pool = pool;

        const uint32 items_per_batch = (num_items + num_batches - 1) / num_batches;

        auto *data_ptr = items.Data();

        for (uint32 batch_index = 0; batch_index < num_batches; batch_index++) {
            batch.AddTask([data_ptr, batch_index, items_per_batch, num_items, &lambda](...)
            {
                const uint32 offset_index = batch_index * items_per_batch;
                const uint32 max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint32 i = offset_index; i < max_index; i++) {
                    lambda(*(data_ptr + i), i, batch_index);
                }
            });
        }

        if (batch.executors.Size() > 1) {
            EnqueueBatch(&batch);
            batch.AwaitCompletion();
        } else if (batch.executors.Size() == 1) {
            // no point in enqueing for just 1 task, execute immediately
            batch.ExecuteBlocking();
        }
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class Lambda>
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPoolName priority, Container &&items, Lambda &&lambda)
    {
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            pool.threads.Size(),
            std::forward<Container>(items),
            std::forward<Lambda>(lambda)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class Lambda>
    HYP_FORCE_INLINE void ParallelForEach(Container &&items, Lambda &&lambda)
    {
        constexpr auto priority = THREAD_POOL_GENERIC;
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            pool.threads.Size(),
            std::forward<Container>(items),
            std::forward<Lambda>(lambda)
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

    FixedArray<TaskThreadPool, THREAD_POOL_MAX> m_pools;
    Array<TaskBatch *>                          m_running_batches;
    AtomicVar<bool>                             m_running;
};

} // namespace threading

using TaskSystem = threading::TaskSystem;
using TaskBatch = threading::TaskBatch;
using TaskRef = threading::TaskRef;
using TaskThreadPoolName = threading::TaskThreadPoolName;

} // namespace hyperion

#endif

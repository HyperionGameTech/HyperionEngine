#ifndef HYPERION_V2_TASK_SYSTEM_HPP
#define HYPERION_V2_TASK_SYSTEM_HPP

#include <core/lib/FixedArray.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/lib/String.hpp>
#include <math/MathUtil.hpp>
#include <util/Defines.hpp>
#include <system/Debug.hpp>

#include <Threads.hpp>
#include <TaskThread.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {
    
enum TaskThreadPoolName : uint
{
    THREAD_POOL_GENERIC,
    THREAD_POOL_RENDER_COLLECT,
    THREAD_POOL_RENDER,
    THREAD_POOL_MAX
};

struct TaskRef
{
    TaskThread  *runner;
    TaskID      id;
};

struct TaskBatch
{
    AtomicVar<uint>                     num_completed;
    uint                                num_enqueued = 0;

    /*! \brief The priority / pool lane for which to place
     * all of the threads in this batch into
     */
    TaskThreadPoolName                  pool = THREAD_POOL_GENERIC;

    /* Number of tasks must remain constant from creation of the TaskBatch,
     * to completion. */
    Array<TaskThread::Scheduler::Task>  tasks;

    /* TaskRefs to be set by the TaskSystem, holding task ids and pointers to the threads
     * each task has been scheduled to. */
    Array<TaskRef>                      task_refs;

    /*! \brief Add a task to be ran with this batch. Note: adding a task while the batch is already running
     * does not mean the newly added task will be ran! You'll need to re-enqueue the batch after the previous one has been completed.
     */
    HYP_FORCE_INLINE void AddTask(TaskThread::Scheduler::Task &&task)
        { tasks.PushBack(std::move(task)); }

    // we can use memory_order_relaxed because we do not add taks once the
    // batch has been enqueued, so once num_completed is equal to num_enqueued,
    // we know it's done
    HYP_FORCE_INLINE bool IsCompleted() const
        { return num_completed.Get(MemoryOrder::RELAXED) >= num_enqueued; }

    /*! \brief Block the current thread until all tasks have been marked as completed. */
    HYP_FORCE_INLINE void AwaitCompletion() const
    {
        while (!IsCompleted()) {
            HYP_WAIT_IDLE();
        }
    }

    /*! \brief Execute each non-enqueued task in serial (not async). */
    void ExecuteBlocking()
    {
        for (auto &task : tasks) {
            task.Execute();
        }

        tasks.Clear();
    }
};

class TaskSystem
{
    struct TaskThreadPool
    {
        AtomicVar<uint>                 cycle { 0u };
        Array<UniquePtr<TaskThread>>    threads;
    };

    struct TaskThreadPoolInfo
    {
        uint                num_task_threads = 0u;
        ThreadPriorityValue priority = ThreadPriorityValue::NORMAL;
    };

    static const FlatMap<TaskThreadPoolName, TaskThreadPoolInfo>    s_thread_pool_infos;

public:
    HYP_API static TaskSystem &GetInstance();

    TaskSystem()
    {
        ThreadMask mask = THREAD_TASK_0;

        for (uint i = 0; i < THREAD_POOL_MAX; i++) {
            const TaskThreadPoolName pool_name { i };

            auto thread_pool_infos_it = s_thread_pool_infos.Find(pool_name);
            AssertThrowMsg(
                thread_pool_infos_it != s_thread_pool_infos.End(),
                "TaskThreadPoolName for %u not found in s_thread_pool_infos",
                i
            );

            const TaskThreadPoolInfo &task_thread_pool_info = thread_pool_infos_it->second;

            TaskThreadPool &pool = m_pools[i];
            pool.threads.Resize(task_thread_pool_info.num_task_threads);

            for (auto &it : pool.threads) {
                AssertThrow(THREAD_TASK & mask);

                it.Reset(new TaskThread(Threads::thread_ids.At(ThreadName(mask)), task_thread_pool_info.priority));
                mask <<= 1;
            }
        }
    }

    TaskSystem(const TaskSystem &other) = delete;
    TaskSystem &operator=(const TaskSystem &other) = delete;

    TaskSystem(TaskSystem &&other) noexcept = delete;
    TaskSystem &operator=(TaskSystem &&other) noexcept = delete;

    ~TaskSystem() = default;

    HYP_FORCE_INLINE
    bool IsRunning() const
        { return m_running.Get(MemoryOrder::RELAXED); }

    HYP_API void Start();
    HYP_API void Stop();

    TaskThreadPool &GetPool(TaskThreadPoolName pool_name)
        { return m_pools[uint(pool_name)]; }

    template <class Task>
    TaskRef ScheduleTask(Task &&task, TaskThreadPoolName pool_name = THREAD_POOL_GENERIC)
    {
        AssertThrowMsg(
            IsRunning(),
            "TaskSystem::Start() must be called before enqueuing tasks"
        );

        TaskThreadPool &pool = GetPool(pool_name);

        const uint cycle = pool.cycle.Get(MemoryOrder::RELAXED);

        TaskThread *task_thread = pool.threads[cycle].Get();
        const TaskID task_id = task_thread->ScheduleTask(std::forward<Task>(task));

        pool.cycle.Set((cycle + 1) % pool.threads.Size(), MemoryOrder::RELAXED);

        return TaskRef {
            task_thread,
            task_id
        };
    }

    /*! \brief Enqueue a batch of multiple Tasks. Each Task will be enqueued to run in parallel.
     * You will need to call AwaitCompletion() before the underlying TaskBatch is destroyed.
     */
    HYP_API TaskBatch *EnqueueBatch(TaskBatch *batch);

    /*! \brief Dequeue each task in a TaskBatch. A potentially expensive operation,
     * as each task will have to individually be dequeued, performing a lock operation.
     * @param batch Pointer to the TaskBatch to dequeue
     * @returns A Array<bool> containing for each Task that has been enqueued, whether or not
     * it was successfully dequeued.
     */
    HYP_API Array<bool> DequeueBatch(TaskBatch *batch);

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class Lambda>
    void ParallelForEach(TaskThreadPoolName pool, uint num_batches, uint num_items, Lambda &&lambda)
    {
        if (num_items == 0) {
            return;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        TaskBatch batch;
        batch.pool = pool;

        const uint items_per_batch = (num_items + num_batches - 1) / num_batches;

        for (uint batch_index = 0; batch_index < num_batches; batch_index++) {
            batch.AddTask([batch_index, items_per_batch, num_items, &lambda](...)
            {
                const uint offset_index = batch_index * items_per_batch;
                const uint max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint i = offset_index; i < max_index; i++) {
                    lambda(i, batch_index);
                }
            });
        }

        if (batch.tasks.Size() > 1) {
            EnqueueBatch(&batch);
            batch.AwaitCompletion();
        } else if (batch.tasks.Size() == 1) {
            // no point in enqueing for just 1 task, execute immediately
            batch.ExecuteBlocking();
        }
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class Lambda>
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPoolName priority, uint num_items, Lambda &&lambda)
    {
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            uint(pool.threads.Size()),
            num_items,
            std::forward<Lambda>(lambda)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \ref{num_items} times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class Lambda>
    HYP_FORCE_INLINE void ParallelForEach(uint num_items, Lambda &&lambda)
    {
        constexpr auto priority = THREAD_POOL_GENERIC;
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            uint(pool.threads.Size()),
            num_items,
            std::forward<Lambda>(lambda)
        );
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into \ref{num_batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class Lambda>
    void ParallelForEach(TaskThreadPoolName pool, uint num_batches, Container &&items, Lambda &&lambda)
    {
        //static_assert(Container::is_contiguous, "Container must be contiguous to use ParallelForEach");

        const uint num_items = uint(items.Size());

        if (num_items == 0) {
            return;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        TaskBatch batch;
        batch.pool = pool;

        const uint items_per_batch = (num_items + num_batches - 1) / num_batches;

        auto *data_ptr = items.Data();

        for (uint batch_index = 0; batch_index < num_batches; batch_index++) {
            batch.AddTask([data_ptr, batch_index, items_per_batch, num_items, &lambda](...)
            {
                const uint offset_index = batch_index * items_per_batch;
                const uint max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (uint i = offset_index; i < max_index; i++) {
                    lambda(*(data_ptr + i), i, batch_index);
                }
            });
        }

        if (batch.tasks.Size() > 1) {
            EnqueueBatch(&batch);
            batch.AwaitCompletion();
        } else if (batch.tasks.Size() == 1) {
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
            uint(pool.threads.Size()),
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
            uint(pool.threads.Size()),
            std::forward<Container>(items),
            std::forward<Lambda>(lambda)
        );
    }

    HYP_FORCE_INLINE bool Unschedule(const TaskRef &task_ref)
        { return task_ref.runner->GetScheduler().Dequeue(task_ref.id); }

private:
    FixedArray<TaskThreadPool, THREAD_POOL_MAX> m_pools;
    Array<TaskBatch *>                          m_running_batches;
    AtomicVar<bool>                             m_running;
};

} // namespace hyperion

#endif

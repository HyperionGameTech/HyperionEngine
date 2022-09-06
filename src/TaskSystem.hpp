#ifndef HYPERION_V2_TASK_SYSTEM_HPP
#define HYPERION_V2_TASK_SYSTEM_HPP

#include <core/lib/FixedArray.hpp>
#include <util/Defines.hpp>

#include "Threads.hpp"
#include "TaskThread.hpp"

#include <Types.hpp>

#include <atomic>

#define HYP_NUM_TASK_THREADS_2
//#define HYP_NUM_TASK_THREADS_4
//#define HYP_NUM_TASK_THREADS_8

namespace hyperion::v2 {

struct TaskRef
{
    TaskThread *runner;
    TaskID id;
};

struct TaskBatch
{
    std::atomic<UInt> num_completed;
    UInt num_enqueued;

    /*! \brief The priority / pool lane for which to place
     * all of the threads in this batch into
     */
    TaskPriority priority = TaskPriority::MEDIUM;

    /* Number of tasks must remain constant from creation of the TaskBatch,
     * to completion. */
    DynArray<TaskThread::Scheduler::Task> tasks;

    /* TaskRefs to be set by the TaskSystem, holding task ids and pointers to the threads
     * each task has been scheduled to. */
    DynArray<TaskRef> task_refs;
    
    /*! \brief Add a task to be ran with this batch. Note: adding a task while the batch is already running
     * does not mean the newly added task will be ran! You'll need to re-enqueue the batch after the previous one has been completed.
     */
    HYP_FORCE_INLINE void AddTask(TaskThread::Scheduler::Task &&task)
        { tasks.PushBack(std::move(task)); }

    HYP_FORCE_INLINE bool IsCompleted() const
        { return num_completed.load(std::memory_order_relaxed) >= num_enqueued; }

    /*! \brief Block the current thread until all tasks have been marked as completed. */
    HYP_FORCE_INLINE void AwaitCompletion() const
    {
        while (!IsCompleted()) {
            HYP_WAIT_IDLE();
        }
    }

    /*! \brief Execute each non-enqueued task in serial (not async). */
    void ForceExecute()
    {
        for (auto &task : tasks) {
            task.Execute();
        }

        tasks.Clear();
    }
};

class TaskSystem
{
    static constexpr UInt target_ticks_per_second = 4096; // For base priority. Second priority is this number << 2, so 65536
    static constexpr UInt num_threads_per_pool = 2;
    
    struct TaskThreadPool
    {
        std::atomic_uint cycle { 0u };
        FixedArray<UniquePtr<TaskThread>, num_threads_per_pool> threads;
    };

public:
    TaskSystem()
    {
        ThreadMask mask = THREAD_TASK_0;
        UInt priority_value = 0;

        for (auto &pool : m_pools) {
            for (auto &it : pool.threads) {
                AssertThrow(THREAD_TASK & mask);
                it.Reset(new TaskThread(Threads::thread_ids.At(static_cast<ThreadName>(mask)), target_ticks_per_second << (2 * priority_value)));
                mask <<= 1;
            }

            ++priority_value;
        }
    }

    TaskSystem(const TaskSystem &other) = delete;
    TaskSystem &operator=(const TaskSystem &other) = delete;

    TaskSystem(TaskSystem &&other) noexcept = delete;
    TaskSystem &operator=(TaskSystem &&other) noexcept = delete;

    ~TaskSystem() = default;

    void Start()
    {
        for (auto &pool : m_pools) {
            for (auto &it : pool.threads) {
                AssertThrow(it != nullptr);
                AssertThrow(it->Start());
            }
        }
    }

    void Stop()
    {
        for (auto &pool : m_pools) {
            for (auto &it : pool.threads) {
                AssertThrow(it != nullptr);
                it->Stop();
                it->Join();
            }
        }
    }

    TaskThreadPool &GetPool(TaskPriority priority)
        { return m_pools[static_cast<UInt>(priority)]; }

    template <class Task>
    TaskRef ScheduleTask(Task &&task, TaskPriority priority = TaskPriority::MEDIUM)
    {
        auto &pool = GetPool(priority);

        const auto cycle = pool.cycle.load(std::memory_order_relaxed);
        auto &task_thread = pool.threads[cycle];

        const auto task_id = task_thread->ScheduleTask(std::forward<Task>(task));

        pool.cycle.store((cycle + 1) % pool.threads.Size(), std::memory_order_relaxed);

        return TaskRef {
            task_thread.Get(),
            task_id
        };
    }

    /*! \brief Enqueue a batch of multiple Tasks. Each Task will be enqueued to run in parallel.
     * You will need to call AwaitCompletion() before the pointer to task batch is destroyed.
     */
    TaskBatch *EnqueueBatch(TaskBatch *batch)
    {
        AssertThrow(batch != nullptr);
        batch->num_completed.store(0u, std::memory_order_relaxed);
        batch->num_enqueued = 0u;

        batch->task_refs.Resize(batch->tasks.Size());

        auto &pool = GetPool(batch->priority);

        for (SizeType i = 0; i < batch->tasks.Size(); i++) {
            auto &task = batch->tasks[i];

            const auto cycle = pool.cycle.load(std::memory_order_relaxed);
            auto &task_thread = pool.threads[cycle];
            
            const auto task_id = task_thread->ScheduleTask(std::move(task), &batch->num_completed);

            ++batch->num_enqueued;

            batch->task_refs[i] = TaskRef {
                task_thread.Get(),
                task_id
            };

            pool.cycle.store((cycle + 1) % pool.threads.Size(), std::memory_order_relaxed);
        }

        // all have been moved
        batch->tasks.Clear();

        return batch;
    }

    /*! \brief Dequeue each task in a TaskBatch. A potentially expensive operation,
     * as each task will have to individually be dequeued, performing a lock operation.
     * @param batch Pointer to the TaskBatch to dequeue
     * @returns A DynArray<bool> containing for each Task that has been enqueued, whether or not
     * it was successfully dequeued.
     */
    DynArray<bool> DequeueBatch(TaskBatch *batch)
    {
        AssertThrow(batch != nullptr);

        DynArray<bool> results;
        results.Resize(batch->task_refs.Size());

        for (SizeType i = 0; i < batch->task_refs.Size(); i++) {
            auto &task_ref = batch->task_refs[i];

            if (task_ref.runner == nullptr) {
                continue;
            }

            results[i] = task_ref.runner->GetScheduler().Dequeue(task_ref.id);
        }

        return results;
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into \ref{num_groups} groups.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class Lambda>
    void ParallelForEach(TaskPriority priority, UInt num_groups, Container &&items, Lambda &&lambda)
    {
        TaskBatch batch;
        batch.priority = priority;

        const auto num_items = items.Size();
        const auto items_per_group = num_items / num_groups;
        
        for (SizeType group_index = 0; group_index < num_groups; group_index++) {
            batch.AddTask([&items, group_index, items_per_group, num_items, lambda](...) {
                const SizeType offset_index = group_index * items_per_group;
                const SizeType max_iter = MathUtil::Min(offset_index + items_per_group, num_items);

                for (SizeType i = offset_index; i < max_iter; ++i) {
                    lambda(items[i], i);
                }
            });
        }

        if (batch.tasks.Size() > 1) {
            EnqueueBatch(&batch);
            batch.AwaitCompletion();
        } else if (batch.tasks.Size() == 1) {
            // no point in enqueing for just 1 task, execute immediately
            batch.ForceExecute();
        }
    }
    
    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class Lambda>
    HYP_FORCE_INLINE void ParallelForEach(TaskPriority priority, Container &&items, Lambda &&lambda)
    {
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            static_cast<UInt>(pool.threads.Size()),
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
        constexpr auto priority = TaskPriority::MEDIUM;
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            static_cast<UInt>(pool.threads.Size()),
            std::forward<Container>(items),
            std::forward<Lambda>(lambda)
        );
    }

    bool Unschedule(const TaskRef &task_ref)
    {
        return task_ref.runner->GetScheduler().Dequeue(task_ref.id);
    }

private:

    FixedArray<TaskThreadPool, 2> m_pools;

    DynArray<TaskBatch *> m_running_batches;
};

} // namespace hyperion::v2

#endif
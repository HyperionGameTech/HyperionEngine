#ifndef HYPERION_V2_TASK_SYSTEM_HPP
#define HYPERION_V2_TASK_SYSTEM_HPP

#include <core/lib/FixedArray.hpp>
#include <core/lib/AtomicVar.hpp>
#include <math/MathUtil.hpp>
#include <util/Defines.hpp>
#include <system/Debug.hpp>

#include "Threads.hpp"
#include "TaskThread.hpp"

#include <Types.hpp>

#include <atomic>

namespace hyperion::v2 {

struct TaskRef
{
    TaskThread  *runner;
    TaskID      id;
};

enum TaskThreadPoolName : UInt
{
    THREAD_POOL_GENERIC        = 0,
    THREAD_POOL_RENDER_COLLECT = 1,
    THREAD_POOL_RENDER         = 2,
    THREAD_POOL_BACKGROUND     = 3,

    THREAD_POOL_MAX
};

struct TaskBatch
{
    AtomicVar<UInt>                     num_completed;
    UInt                                num_enqueued = 0;

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
    static constexpr UInt num_threads_per_pool = 2;
    
    struct TaskThreadPool
    {
        AtomicVar<UInt>                                         cycle { 0u };
        FixedArray<UniquePtr<TaskThread>, num_threads_per_pool> threads;
    };

public:
    static TaskSystem &GetInstance();

    TaskSystem()
    {
        ThreadMask  mask = THREAD_TASK_0;
        UInt        priority_value = 0;

        for (auto &pool : m_pools) {
            for (auto &it : pool.threads) {
                AssertThrow(THREAD_TASK & mask);

                it.Reset(new TaskThread(Threads::thread_ids.At(ThreadName(mask))));
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

    TaskThreadPool &GetPool(TaskThreadPoolName pool_name)
        { return m_pools[UInt(pool_name)]; }

    template <class Task>
    TaskRef ScheduleTask(Task &&task, TaskThreadPoolName pool_name = THREAD_POOL_GENERIC)
    {
        TaskThreadPool &pool = GetPool(pool_name);

        const UInt cycle = pool.cycle.Get(MemoryOrder::RELAXED);

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
    TaskBatch *EnqueueBatch(TaskBatch *batch)
    {
        AssertThrow(batch != nullptr);

        batch->num_completed.Set(0, MemoryOrder::RELAXED);
        batch->num_enqueued = 0u;
        batch->task_refs.Resize(batch->tasks.Size());

        const ThreadID &current_thread_id = Threads::CurrentThreadID();
        const bool in_task_thread = Threads::IsThreadInMask(current_thread_id, THREAD_TASK);

        if (in_task_thread) {
            for (auto &task : batch->tasks) {
                task.Execute();
            }

            // all have been moved
            batch->tasks.Clear();
            return batch;
        }

        TaskThreadPool &pool = GetPool(batch->pool);

        const UInt num_threads_in_pool = UInt(pool.threads.Size());
        const UInt max_spins = num_threads_in_pool;

        for (SizeType i = 0; i < batch->tasks.Size(); i++) {
            auto &task = batch->tasks[i];

            UInt cycle = pool.cycle.Get(MemoryOrder::RELAXED);

            TaskThread *task_thread = nullptr;
            UInt num_spins = 0;
            bool was_busy = false;
            
            // if we are currently on a task thread we need to move to the next task thread in the pool
            // if we selected the current task thread. otherwise we will have a deadlock.
            // this does require that there are > 1 task thread in the pool.
            do {
                if (num_spins >= 1) {
                    DebugLog(LogType::Warn, "Task thread %s: %u spins\n", current_thread_id.name.LookupString().Data(), num_spins);

                    if (num_spins >= max_spins) {
                        DebugLog(
                            LogType::Warn,
                            "On task thread %s: All other task threads busy while enqueing a batch from within another task thread! "
                            "The task will instead be executed inline on the current task thread."
                            "\n\tReduce usage of batching within batches?\n",
                            current_thread_id.name.LookupString().Data()
                        );

                        was_busy = true;

                        break;
                    }
                }

                task_thread = pool.threads[cycle].Get();
                cycle = (cycle + 1) % num_threads_in_pool;

                ++num_spins;
            } while (task_thread->GetID() == current_thread_id
                || (in_task_thread && !task_thread->IsFree()));

            // force execution now. not ideal,
            // but if we are currently on a task thread, and all task threads are busy,
            // we don't want to risk that another task thread is waiting on our current task thread,
            // which would cause a deadlock.
            if (was_busy) {
                task.Execute();

                continue;
            }

            const TaskID task_id = task_thread->ScheduleTask(std::move(task), &batch->num_completed);

            ++batch->num_enqueued;

            batch->task_refs[i] = TaskRef {
                task_thread,
                task_id
            };

            pool.cycle.Set(cycle, MemoryOrder::RELAXED);
        }

        // all have been moved
        batch->tasks.Clear();

        return batch;
    }

    /*! \brief Dequeue each task in a TaskBatch. A potentially expensive operation,
     * as each task will have to individually be dequeued, performing a lock operation.
     * @param batch Pointer to the TaskBatch to dequeue
     * @returns A Array<bool> containing for each Task that has been enqueued, whether or not
     * it was successfully dequeued.
     */
    Array<Bool> DequeueBatch(TaskBatch *batch)
    {
        AssertThrow(batch != nullptr);

        Array<Bool> results;
        results.Resize(batch->task_refs.Size());

        for (SizeType i = 0; i < batch->task_refs.Size(); i++) {
            const TaskRef &task_ref = batch->task_refs[i];

            if (task_ref.runner == nullptr) {
                continue;
            }

            results[i] = task_ref.runner->GetScheduler().Dequeue(task_ref.id);
        }

        return results;
    }

    /*! \brief Creates a TaskBatch which will call the lambda for each and every item in the given container.
     *  The tasks will be split evenly into \ref{batches} batches.
        The lambda will be called with (item, index) for each item. */
    template <class Container, class Lambda>
    void ParallelForEach(TaskThreadPoolName pool, UInt num_batches, Container &&items, Lambda &&lambda)
    {
        // static_assert(Container::is_contiguous, "Container must be contiguous to perform ParallelForEach");

        const UInt num_items = UInt(items.Size());

        if (num_items == 0) {
            return;
        }

        num_batches = MathUtil::Clamp(num_batches, 1u, num_items);

        TaskBatch batch;
        batch.pool = pool;

        const UInt items_per_batch = (num_items + num_batches - 1) / num_batches;

        auto *data_ptr = items.Data();

        for (UInt batch_index = 0; batch_index < num_batches; batch_index++) {
            batch.AddTask([data_ptr, batch_index, items_per_batch, num_items, lambda](...)
            {
                const UInt offset_index = batch_index * items_per_batch;
                const UInt max_index = MathUtil::Min(offset_index + items_per_batch, num_items);

                for (UInt i = offset_index; i < max_index; i++) {
                    lambda(*(data_ptr + i), i, batch_index);
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
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPoolName priority, Container &&items, Lambda &&lambda)
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
        constexpr auto priority = THREAD_POOL_GENERIC;
        const auto &pool = GetPool(priority);

        ParallelForEach(
            priority,
            static_cast<UInt>(pool.threads.Size()),
            std::forward<Container>(items),
            std::forward<Lambda>(lambda)
        );
    }

    HYP_FORCE_INLINE bool Unschedule(const TaskRef &task_ref)
        { return task_ref.runner->GetScheduler().Dequeue(task_ref.id); }



private:
    FixedArray<TaskThreadPool, THREAD_POOL_MAX> m_pools;
    Array<TaskBatch *> m_running_batches;
};

} // namespace hyperion::v2

#endif
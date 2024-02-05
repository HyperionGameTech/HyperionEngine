#include <TaskSystem.hpp>

namespace hyperion::v2 {

TaskSystem &TaskSystem::GetInstance()
{
    static TaskSystem instance;

    return instance;
}

void TaskSystem::Start()
{
    for (auto &pool : m_pools) {
        for (auto &it : pool.threads) {
            AssertThrow(it != nullptr);
            AssertThrow(it->Start());
        }
    }
}

void TaskSystem::Stop()
{
    for (auto &pool : m_pools) {
        for (auto &it : pool.threads) {
            AssertThrow(it != nullptr);

            it->Stop();
            it->Join();
        }
    }
}

TaskBatch *TaskSystem::EnqueueBatch(TaskBatch *batch)
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
                DebugLog(LogType::Warn, "Task thread %s: %u spins\n", current_thread_id.name.LookupString(), num_spins);

                if (num_spins >= max_spins) {
                    DebugLog(
                        LogType::Warn,
                        "On task thread %s: All other task threads busy while enqueing a batch from within another task thread! "
                        "The task will instead be executed inline on the current task thread."
                        "\n\tReduce usage of batching within batches?\n",
                        current_thread_id.name.LookupString()
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

Array<Bool> TaskSystem::DequeueBatch(TaskBatch *batch)
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

} // namespace hyperion::v2
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

namespace threading {

const FlatMap<TaskThreadPoolName, TaskSystem::TaskThreadPoolInfo> TaskSystem::s_thread_pool_infos {
    { TaskThreadPoolName::THREAD_POOL_GENERIC,          { 4u, ThreadPriorityValue::NORMAL } },
    { TaskThreadPoolName::THREAD_POOL_RENDER,           { 4u, ThreadPriorityValue::HIGHEST } },
    { TaskThreadPoolName::THREAD_POOL_RENDER_COLLECT,   { 2u, ThreadPriorityValue::HIGHEST } }
};

TaskSystem &TaskSystem::GetInstance()
{
    static TaskSystem instance;

    return instance;
}

void TaskSystem::Start()
{
    AssertThrowMsg(
        !IsRunning(),
        "TaskSystem::Start() has already been called"
    );

    for (auto &pool : m_pools) {
        for (auto &it : pool.threads) {
            AssertThrow(it != nullptr);
            AssertThrow(it->Start());
        }
    }

    m_running.Set(true, MemoryOrder::RELAXED);
}

void TaskSystem::Stop()
{
    AssertThrowMsg(
        IsRunning(),
        "TaskSystem::Start() must be called before TaskSystem::Stop()"
    );

    m_running.Set(false, MemoryOrder::RELAXED);

    Array<TaskThread *> task_threads;

    for (auto &pool : m_pools) {
        for (auto &it : pool.threads) {
            AssertThrow(it != nullptr);
            it->Stop();

            task_threads.PushBack(it.Get());
        }
    }

    while (task_threads.Any()) {
        TaskThread *top = task_threads.PopBack();
        top->Join();
    }
}

TaskBatch *TaskSystem::EnqueueBatch(TaskBatch *batch)
{
    AssertThrowMsg(
        IsRunning(),
        "TaskSystem::Start() must be called before enqueuing tasks"
    );

    AssertThrow(batch != nullptr);

    batch->num_completed.Set(0, MemoryOrder::RELAXED);
    batch->num_enqueued = 0u;
    batch->task_refs.Resize(batch->executors.Size());

    const ThreadID &current_thread_id = Threads::CurrentThreadID();

    TaskThreadPool &pool = GetPool(batch->pool);

    const uint32 num_threads_in_pool = uint32(pool.threads.Size());
    const uint32 max_spins = num_threads_in_pool;

    uint32 task_index = 0;

    for (auto it = batch->executors.Begin(); it != batch->executors.End(); ++it, ++task_index) {
        TaskExecutor<> &executor = *it;

        uint32 cycle = pool.cycle.Get(MemoryOrder::RELAXED);

        TaskThread *task_thread = nullptr;
        uint num_spins = 0;
        bool was_busy = false;
        
        // if we are currently on a task thread we need to move to the next task thread in the pool
        // if we selected the current task thread. otherwise we will have a deadlock.
        // this does require that there are > 1 task thread in the pool.
        do {
            if (num_spins >= max_spins) {
                was_busy = true;

                break;
            }

            task_thread = pool.threads[cycle].Get();
            cycle = (cycle + 1) % num_threads_in_pool;

            ++num_spins;
        } while (task_thread->GetID() == current_thread_id
            || !task_thread->IsRunning()
            || (Threads::IsThreadInMask(current_thread_id, THREAD_TASK) && !task_thread->IsFree()));

        // force execution now. not ideal,
        // but if we are currently on a task thread, and all task threads are busy,
        // we don't want to risk that another task thread is waiting on our current task thread,
        // which would cause a deadlock.
        if (was_busy) {
            HYP_LOG(Tasks, LogLevel::WARNING, "On task thread {}: All other task threads busy; Executing task inline", current_thread_id.name);

            executor.Execute();

            continue;
        }

        AssertThrow(task_thread != nullptr);

        const TaskID task_id = task_thread->GetScheduler().EnqueueTaskExecutor(&executor, &batch->num_completed);

        ++batch->num_enqueued;

        batch->task_refs[task_index] = TaskRef {
            task_id,
            &task_thread->GetScheduler()
        };

        pool.cycle.Set(cycle, MemoryOrder::RELAXED);
    }

    return batch;
}

Array<bool> TaskSystem::DequeueBatch(TaskBatch *batch)
{
    AssertThrowMsg(
        IsRunning(),
        "TaskSystem::Start() must be called before dequeuing tasks"
    );

    AssertThrow(batch != nullptr);

    Array<bool> results;
    results.Resize(batch->task_refs.Size());

    for (SizeType i = 0; i < batch->task_refs.Size(); i++) {
        const TaskRef &task_ref = batch->task_refs[i];

        if (!task_ref.IsValid()) {
            continue;
        }

        results[i] = task_ref.assigned_scheduler->Dequeue(task_ref.id);
    }

    return results;
}

} // namespace threading
} // namespace hyperion
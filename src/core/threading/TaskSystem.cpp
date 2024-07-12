/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

namespace threading {

struct TaskThreadPoolInfo
{
    uint32              num_task_threads = 0u;
    ThreadPriorityValue priority = ThreadPriorityValue::NORMAL;
};

static const FlatMap<TaskThreadPoolName, TaskThreadPoolInfo> g_thread_pool_info {
    { TaskThreadPoolName::THREAD_POOL_GENERIC,  { 4u, ThreadPriorityValue::NORMAL } },
    { TaskThreadPoolName::THREAD_POOL_RENDER,   { 4u, ThreadPriorityValue::HIGHEST } }
};

TaskSystem &TaskSystem::GetInstance()
{
    static TaskSystem instance;

    return instance;
}

TaskSystem::TaskSystem()
{
    ThreadMask mask = THREAD_TASK_0;

    for (uint32 i = 0; i < THREAD_POOL_MAX; i++) {
        const TaskThreadPoolName pool_name { i };

        auto thread_pool_info_it = g_thread_pool_info.Find(pool_name);

        AssertThrowMsg(
            thread_pool_info_it != g_thread_pool_info.End(),
            "TaskThreadPoolName for %u not found in g_thread_pool_info",
            i
        );

        const TaskThreadPoolInfo &task_thread_pool_info = thread_pool_info_it->second;

        TaskThreadPool &pool = m_pools[i];
        pool.threads.Resize(task_thread_pool_info.num_task_threads);

        for (auto &it : pool.threads) {
            AssertThrow(THREAD_TASK & mask);

            it.Reset(new TaskThread(Threads::thread_ids.At(ThreadName(mask)), task_thread_pool_info.priority));
            mask <<= 1;
        }
    }
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

    int task_index = 0;

    for (auto it = batch->executors.Begin(); it != batch->executors.End(); ++it, ++task_index) {
        TaskExecutor<> &executor = *it;

        TaskThread *task_thread = GetNextTaskThread(pool);
        AssertThrow(task_thread != nullptr);

        const TaskID task_id = task_thread->GetScheduler().EnqueueTaskExecutor(&executor, &batch->num_completed);

        ++batch->num_enqueued;

        batch->task_refs[task_index] = TaskRef {
            task_id,
            &task_thread->GetScheduler()
        };
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

TaskThread *TaskSystem::GetNextTaskThread(TaskThreadPool &pool)
{
    static constexpr uint32 max_spins = 40;

    TaskThread *task_thread = nullptr;

    const ThreadID current_thread_id = Threads::CurrentThreadID();
    const bool is_on_task_thread = current_thread_id & THREAD_TASK;

    const uint32 num_threads_in_pool = uint32(pool.threads.Size());

    uint32 cycle = pool.cycle.Get(MemoryOrder::RELAXED) % num_threads_in_pool;

    uint num_spins = 0;
    
    // if we are currently on a task thread we need to move to the next task thread in the pool
    // if we selected the current task thread. otherwise we will have a deadlock.
    // this does require that there are > 1 task thread in the pool.
    do {
        if (num_spins >= max_spins) {
            AssertThrowMsg(task_thread != nullptr, "All task threads are busy and we have not found a free one");

            HYP_LOG(Tasks, LogLevel::WARNING, "Maximum spins reached in GetNextTaskThread");

            pool.cycle.Increment(1, MemoryOrder::RELAXED);

            return task_thread;
        }

        do {
            task_thread = pool.threads[cycle].Get();

            cycle = (cycle + 1) % num_threads_in_pool;
            pool.cycle.Increment(1, MemoryOrder::RELAXED);

            ++num_spins;
        } while (task_thread->GetID() == current_thread_id || (is_on_task_thread && task_thread->IsWaitingOnTaskFromThread(current_thread_id)));
    } while (!task_thread->IsRunning() && !task_thread->IsFree());

    return task_thread;
}

} // namespace threading
} // namespace hyperion
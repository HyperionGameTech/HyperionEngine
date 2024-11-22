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

            it = MakeUnique<TaskThread>(Threads::GetStaticThreadID(ThreadName(mask)), task_thread_pool_info.priority);
            mask <<= 1;
        }
    }
}

void TaskSystem::Start()
{
    AssertThrowMsg(!IsRunning(), "TaskSystem::Start() has already been called");

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
    AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before TaskSystem::Stop()");

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
    AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");
    AssertThrow(batch != nullptr);

#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
    HYP_MT_CHECK_READ(batch->data_race_detector);
#endif

    // batch->num_completed.Set(0, MemoryOrder::SEQUENTIAL);
    batch->semaphore.SetValue(batch->num_enqueued);

    TaskBatch *next_batch = batch->next_batch;

    if (batch->num_enqueued == 0) {
        // enqueue next batch immediately if it exists and no tasks are added to this batch
        batch->OnComplete();

        if (next_batch != nullptr) {
            EnqueueBatch(next_batch);
        }

        return batch;
    }

    TaskThreadPool &pool = GetPool(batch->pool);

#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
    HYP_MT_CHECK_RW(batch->data_race_detector);
#endif

    for (auto it = batch->executors.Begin(); it != batch->executors.End(); ++it) {
        TaskThread *task_thread = GetNextTaskThread(pool);
        AssertThrow(task_thread != nullptr);

        const TaskID task_id = task_thread->GetScheduler()->EnqueueTaskExecutor(
            &(*it),
            &batch->semaphore,
            next_batch != nullptr
                ? OnTaskCompletedCallback([this, &OnComplete = batch->OnComplete, next_batch]() { OnComplete(); EnqueueBatch(next_batch); })
                : OnTaskCompletedCallback(batch->OnComplete ? &batch->OnComplete : nullptr)
        );

        batch->task_refs.EmplaceBack(task_id, task_thread->GetScheduler());
    }

    return batch;
}

Array<bool> TaskSystem::DequeueBatch(TaskBatch *batch)
{
    AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before dequeuing tasks");

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

    const uint32 num_threads_in_pool = uint32(pool.threads.Size());

    const ThreadID current_thread_id = Threads::CurrentThreadID();
    const bool is_on_task_thread = current_thread_id & THREAD_TASK;

    IThread *current_thread_object = Threads::CurrentThreadObject();

    uint32 cycle = pool.cycle.Get(MemoryOrder::RELAXED) % num_threads_in_pool;
    uint32 num_spins = 0;

    TaskThread *task_thread = nullptr;
    
    // if we are currently on a task thread we need to move to the next task thread in the pool
    // if we selected the current task thread. otherwise we will have a deadlock.
    // this does require that there are > 1 task thread in the pool.
    do {
        do {
            task_thread = pool.threads[cycle].Get();

            cycle = (cycle + 1) % num_threads_in_pool;
            pool.cycle.Increment(1, MemoryOrder::RELAXED);

            ++num_spins;
            
            if (num_spins >= max_spins) {
                if (is_on_task_thread) {
                    return static_cast<TaskThread *>(current_thread_object);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                }

                HYP_LOG(Tasks, LogLevel::WARNING, "Maximum spins reached in GetNextTaskThread -- all task threads busy");

                return task_thread;
            }
        } while (task_thread->GetID() == current_thread_id
            || (current_thread_object != nullptr && current_thread_object->GetScheduler()->HasWorkAssignedFromThread(task_thread->GetID())));
    } while (!task_thread->IsRunning() && !task_thread->IsFree());

    return task_thread;
}

} // namespace threading
} // namespace hyperion
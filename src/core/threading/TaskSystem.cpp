/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskSystem.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

namespace hyperion {

namespace threading {

struct TaskThreadPoolInfo
{
    uint32              num_task_threads = 0u;
    ThreadPriorityValue priority = ThreadPriorityValue::NORMAL;
};

static const FlatMap<TaskThreadPoolName, TaskThreadPoolInfo> g_thread_pool_info {
    { TaskThreadPoolName::THREAD_POOL_GENERIC,      { 4u, ThreadPriorityValue::NORMAL } },
    { TaskThreadPoolName::THREAD_POOL_RENDER,       { 2u, ThreadPriorityValue::HIGHEST } },
    { TaskThreadPoolName::THREAD_POOL_BACKGROUND,   { 2u, ThreadPriorityValue::LOW } }
};

#pragma region TaskBatch

bool TaskBatch::IsCompleted() const
{
    return semaphore.IsInSignalState();
}

void TaskBatch::AwaitCompletion()
{
    if (num_enqueued != 0) {
        // Sanity check - ensure not awaiting from a thread we depend on for processing any of the tasks
        // If we get here, we're probably currently on a task thread, and have a circular dependency chain.
        // Consider breaking up task dependencies.
        const ThreadID current_thread_id = ThreadID::Current();

        for (const TaskRef &task_ref : task_refs) {
            AssertThrow(task_ref.assigned_scheduler != nullptr);
            
            AssertThrowMsg(task_ref.assigned_scheduler->GetOwnerThread() != current_thread_id,
                "Cannot wait on a task that is dependent on the current thread!");
        }
    }

    semaphore.Acquire();
}

#pragma endregion TaskBatch

#pragma region TaskThreadPool

TaskThreadPool::TaskThreadPool()
    : m_thread_mask(0)
{
}

TaskThreadPool::TaskThreadPool(Array<UniquePtr<TaskThread>> &&threads)
    : m_threads(std::move(threads)),
      m_thread_mask(0)
{
    for (const UniquePtr<TaskThread> &thread : threads) {
        AssertThrow(thread != nullptr);

        m_thread_mask |= thread->GetID().GetMask();
    }
}

TaskThreadPool::~TaskThreadPool()
{
    for (auto &it : m_threads) {
        AssertThrow(it != nullptr);
        AssertThrowMsg(!it->IsRunning(), "TaskThreadPool::Stop() must be called before TaskThreadPool is destroyed");
        
        if (it->CanJoin()) {
            it->Join();
        }
    }
}

bool TaskThreadPool::IsRunning() const
{
    if (m_threads.Empty()) {
        return false;
    }

    for (auto &it : m_threads) {
        AssertThrow(it != nullptr);
        
        if (it->IsRunning()) {
            return true;
        }
    }

    return false;
}

void TaskThreadPool::Start()
{
    AssertThrow(m_threads.Any());

    for (auto &it : m_threads) {
        AssertThrow(it != nullptr);
        AssertThrow(it->Start());
    }
}

void TaskThreadPool::Stop()
{
    for (auto &it : m_threads) {
        AssertThrow(it != nullptr);
        it->Stop();
    }
}

void TaskThreadPool::Stop(Array<TaskThread *> &out_task_threads)
{
    for (auto &it : m_threads) {
        AssertThrow(it != nullptr);
        it->Stop();

        out_task_threads.PushBack(it.Get());
    }
}

TaskThread *TaskThreadPool::GetNextTaskThread()
{
    static constexpr uint32 max_spins = 40;

    const uint32 num_threads_in_pool = uint32(m_threads.Size());

    const ThreadID current_thread_id = Threads::CurrentThreadID();
    const bool is_on_task_thread = m_thread_mask & current_thread_id;

    IThread *current_thread_object = Threads::CurrentThreadObject();

    uint32 cycle = m_cycle.Get(MemoryOrder::RELAXED) % num_threads_in_pool;
    uint32 num_spins = 0;

    TaskThread *task_thread = nullptr;
    
    // if we are currently on a task thread we need to move to the next task thread in the pool
    // if we selected the current task thread. otherwise we will have a deadlock.
    // this does require that there are > 1 task thread in the pool.
    do {
        do {
            task_thread = m_threads[cycle].Get();

            cycle = (cycle + 1) % num_threads_in_pool;
            m_cycle.Increment(1, MemoryOrder::RELAXED);

            ++num_spins;
            
            if (num_spins >= max_spins) {
                if (is_on_task_thread) {
                    return static_cast<TaskThread *>(current_thread_object);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                }

                HYP_LOG(Tasks, Warning, "Maximum spins reached in GetNextTaskThread -- all task threads busy");

                return task_thread;
            }
        } while (task_thread->GetID() == current_thread_id
            || (current_thread_object != nullptr && current_thread_object->GetScheduler().HasWorkAssignedFromThread(task_thread->GetID())));
    } while (!task_thread->IsRunning() && !task_thread->IsFree());

    return task_thread;
}

ThreadID TaskThreadPool::CreateTaskThreadID(Name base_name, uint32 thread_index)
{
    return ThreadID::CreateDynamicThreadID(CreateNameFromDynamicString(HYP_FORMAT("{}_{}", base_name, thread_index)));
}

#pragma endregion TaskThreadPool

#pragma region GenericTaskThreadPool

GenericTaskThreadPool::GenericTaskThreadPool(const Array<Pair<ThreadID, ThreadPriorityValue>> &thread_ids)
    : TaskThreadPool()
{
    m_threads.Resize(thread_ids.Size());

    for (SizeType i = 0; i < thread_ids.Size(); i++) {
        const Pair<ThreadID, ThreadPriorityValue> &pair = thread_ids[i];

        m_threads[i] = MakeUnique<TaskThread>(pair.first, pair.second);
        m_thread_mask |= pair.first;
    }
}

GenericTaskThreadPool::~GenericTaskThreadPool() = default;

#pragma endregion GenericTaskThreadPool

#pragma region TaskSystem

TaskSystem &TaskSystem::GetInstance()
{
    static TaskSystem instance;

    return instance;
}

TaskSystem::TaskSystem()
{
    ThreadMask mask = THREAD_TASK_0;

    m_pools.Reserve(THREAD_POOL_MAX);

    for (uint32 i = 0; i < THREAD_POOL_MAX; i++) {
        const TaskThreadPoolName pool_name { i };

        auto thread_pool_info_it = g_thread_pool_info.Find(pool_name);

        AssertThrowMsg(
            thread_pool_info_it != g_thread_pool_info.End(),
            "TaskThreadPoolName for %u not found in g_thread_pool_info",
            i
        );

        const TaskThreadPoolInfo &task_thread_pool_info = thread_pool_info_it->second;

        Array<Pair<ThreadID, ThreadPriorityValue>> thread_ids;
        thread_ids.Resize(task_thread_pool_info.num_task_threads);

        for (auto &pair : thread_ids) {
            AssertThrow(THREAD_TASK & mask);

            pair = { Threads::GetStaticThreadID(ThreadName(mask)), task_thread_pool_info.priority };
            mask <<= 1;
        }

        // Create new GenericTaskThreadPool
        m_pools.PushBack(MakeUnique<GenericTaskThreadPool>(thread_ids));
    }
}

void TaskSystem::Start()
{
    AssertThrowMsg(!IsRunning(), "TaskSystem::Start() has already been called");

    for (const UniquePtr<TaskThreadPool> &pool : m_pools) {
        pool->Start();
    }

    m_running.Set(true, MemoryOrder::RELAXED);
}

void TaskSystem::Stop()
{
    AssertThrowMsg(IsRunning(), "TaskSystem::Start() must be called before TaskSystem::Stop()");

    m_running.Set(false, MemoryOrder::RELAXED);

    Array<TaskThread *> task_threads;

    for (const UniquePtr<TaskThreadPool> &pool : m_pools) {
        pool->Stop(task_threads);
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

    TaskThreadPool *pool = nullptr;

    if (batch->pool != nullptr) {
        AssertThrowMsg(batch->pool->IsRunning(), "Start() must be called on a TaskThreadPool before enqueuing tasks to it");

        pool = batch->pool;
    } else {
        pool = m_pools[uint32(TaskThreadPoolName::THREAD_POOL_GENERIC)].Get();
    }

#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
    HYP_MT_CHECK_RW(batch->data_race_detector);
#endif

    for (auto it = batch->executors.Begin(); it != batch->executors.End(); ++it) {
        TaskThread *task_thread = pool->GetNextTaskThread();
        AssertThrow(task_thread != nullptr);

        const TaskID task_id = task_thread->GetScheduler().EnqueueTaskExecutor(
            &(*it),
            &batch->semaphore,
            next_batch != nullptr
                ? OnTaskCompletedCallback([this, &OnComplete = batch->OnComplete, next_batch]() { OnComplete(); EnqueueBatch(next_batch); })
                : OnTaskCompletedCallback(batch->OnComplete ? &batch->OnComplete : nullptr)
        );

        batch->task_refs.EmplaceBack(task_id, &task_thread->GetScheduler());
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
    return pool.GetNextTaskThread();
}

#pragma endregion TaskSystem

} // namespace threading
} // namespace hyperion
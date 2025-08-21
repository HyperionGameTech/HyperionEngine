/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskSystem.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

namespace hyperion {

namespace threading {

extern const FlatMap<TaskThreadPoolName, UniquePtr<TaskThreadPool> (*)(void)> g_threadPoolFactories;

#pragma region TaskBatch

bool TaskBatch::IsCompleted() const
{
    return notifier.IsInSignalState();
}

void TaskBatch::AwaitCompletion()
{
    if (numEnqueued < executors.Size())
    {
        HYP_FAIL("TaskBatch::AwaitCompletion() called before all tasks were enqueued! Expected %zu tasks, but only %u were enqueued.",
            executors.Size(), numEnqueued);
    }

    if (numEnqueued != 0)
    {
        // Sanity check - ensure not awaiting from a thread we depend on for processing any of the tasks
        // If we get here, we're probably currently on a task thread, and have a circular dependency chain.
        // Consider breaking up task dependencies.
        const ThreadId& currentThreadId = ThreadId::Current();

        for (const TaskRef& taskRef : taskRefs)
        {
            HYP_CORE_ASSERT(taskRef.assignedScheduler != nullptr);

            HYP_CORE_ASSERT(taskRef.assignedScheduler->GetOwnerThread() != currentThreadId,
                "Cannot wait on a task that is dependent on the current thread!");
        }
    }

    notifier.Await();

    // ensure dependent batches are also completed
    if (nextBatch != nullptr)
    {
        nextBatch->AwaitCompletion();
    }
}

#pragma endregion TaskBatch

#pragma region TaskThreadPool

TaskThreadPool::TaskThreadPool()
    : m_threadMask(0)
{
}

TaskThreadPool::TaskThreadPool(Array<UniquePtr<TaskThread>>&& threads)
    : m_threads(std::move(threads)),
      m_threadMask(0)
{
    for (const UniquePtr<TaskThread>& thread : threads)
    {
        HYP_CORE_ASSERT(thread != nullptr);

        m_threadMask |= thread->Id().GetMask();
    }
}

TaskThreadPool::~TaskThreadPool()
{
    for (auto& it : m_threads)
    {
        HYP_CORE_ASSERT(it != nullptr);
        HYP_CORE_ASSERT(!it->IsRunning(), "TaskThreadPool::Stop() must be called before TaskThreadPool is destroyed");

        if (it->CanJoin())
        {
            it->Join();
        }
    }
}

bool TaskThreadPool::IsRunning() const
{
    if (m_threads.Empty())
    {
        return false;
    }

    for (auto& it : m_threads)
    {
        HYP_CORE_ASSERT(it != nullptr);

        if (it->IsRunning())
        {
            return true;
        }
    }

    return false;
}

void TaskThreadPool::Start()
{
    HYP_CORE_ASSERT(m_threads.Any());

    for (auto& it : m_threads)
    {
        HYP_CORE_ASSERT(it != nullptr);
        HYP_CORE_ASSERT(it->Start());

        while (!it->IsRunning())
        {
            HYP_WAIT_IDLE();
        }
    }
}

void TaskThreadPool::Stop()
{
    if (!IsRunning())
    {
        return;
    }

    for (auto& it : m_threads)
    {
        it->Stop();
    }

    for (auto& it : m_threads)
    {
        it->Join();
    }
}

void TaskThreadPool::Stop(Array<TaskThread*>& outTaskThreads)
{
    for (auto& it : m_threads)
    {
        HYP_CORE_ASSERT(it != nullptr);
        it->Stop();

        outTaskThreads.PushBack(it.Get());
    }
}

TaskThread* TaskThreadPool::GetNextTaskThread()
{
    static constexpr uint32 maxSpins = 16;

    const uint32 numThreadsInPool = uint32(m_threads.Size());

    const ThreadId currentThreadId = Threads::CurrentThreadId();
    const bool isOnTaskThread = (m_threadMask & currentThreadId.GetMask()) != 0;

    ThreadBase* currentThreadObject = Threads::CurrentThreadObject();

    uint32 cycle = m_cycle.Get(MemoryOrder::RELAXED) % numThreadsInPool;
    uint32 numSpins = 0;

    TaskThread* taskThread = nullptr;

    // if we are currently on a task thread we need to move to the next task thread in the pool
    // if we selected the current task thread. otherwise we will have a deadlock.
    // this does require that there are > 1 task thread in the pool.
    do
    {
        do
        {
            taskThread = m_threads[cycle].Get();

            cycle = (cycle + 1) % numThreadsInPool;
            m_cycle.Increment(1, MemoryOrder::RELAXED);

            ++numSpins;

            if (numSpins >= maxSpins)
            {
                if (isOnTaskThread)
                {
                    return static_cast<TaskThread*>(currentThreadObject); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                }

                HYP_LOG(Tasks, Warning, "Maximum spins reached in GetNextTaskThread -- all task threads busy");

                return taskThread;
            }
        }
        while (taskThread->Id() == currentThreadId
            || (currentThreadObject != nullptr && currentThreadObject->GetScheduler().HasWorkAssignedFromThread(taskThread->Id())));
    }
    while (!taskThread->IsRunning() && !taskThread->IsFree());

    return taskThread;
}

ThreadId TaskThreadPool::CreateTaskThreadId(ANSIStringView baseName, uint32 threadIndex)
{
    return ThreadId(Name::Unique(HYP_FORMAT("{}{}", baseName, threadIndex).Data()), THREAD_CATEGORY_TASK);
}

#pragma endregion TaskThreadPool

#pragma region GenericTaskThreadPool

class GenericTaskThreadPool final : public TaskThreadPool
{
public:
    GenericTaskThreadPool(uint32 numTaskThreads, ThreadPriorityValue priority)
        : TaskThreadPool(TypeWrapper<TaskThread>(), "GenericTask", numTaskThreads)
    {
    }

    virtual ~GenericTaskThreadPool() override = default;
};

#pragma endregion GenericTaskThreadPool

#pragma region RenderTaskThreadPool

class RenderTaskThreadPool final : public TaskThreadPool
{
public:
    RenderTaskThreadPool(uint32 numTaskThreads, ThreadPriorityValue priority)
        : TaskThreadPool(TypeWrapper<TaskThread>(), "RenderTask", numTaskThreads)
    {
    }

    virtual ~RenderTaskThreadPool() override = default;
};

#pragma endregion RenderTaskThreadPool

#pragma region BackgroundTaskThreadPool

class BackgroundTaskThreadPool final : public TaskThreadPool
{
public:
    BackgroundTaskThreadPool(uint32 numTaskThreads, ThreadPriorityValue priority)
        : TaskThreadPool(TypeWrapper<TaskThread>(), "BackgroundTask", numTaskThreads)
    {
    }

    virtual ~BackgroundTaskThreadPool() override = default;
};

#pragma endregion RenderTaskThreadPool

#pragma region TaskSystem

TaskSystem& TaskSystem::GetInstance()
{
    static TaskSystem instance;

    return instance;
}

TaskSystem::TaskSystem()
{
    m_pools.Reserve(THREAD_POOL_MAX);

    for (uint32 i = 0; i < THREAD_POOL_MAX; i++)
    {
        const TaskThreadPoolName poolName { i };

        auto beginIt = g_threadPoolFactories.Begin();
        auto endIt = g_threadPoolFactories.End();

        auto threadPoolFactoriesIt = g_threadPoolFactories.Find(poolName);

        HYP_CORE_ASSERT(threadPoolFactoriesIt != endIt, "Invalid thread pool index %u", i);

        m_pools.PushBack(threadPoolFactoriesIt->second());
    }
}

void TaskSystem::Start()
{
    HYP_CORE_ASSERT(!IsRunning(), "TaskSystem::Start() has already been called");

    for (const UniquePtr<TaskThreadPool>& pool : m_pools)
    {
        pool->Start();
    }

    m_running.Set(true, MemoryOrder::RELAXED);
}

void TaskSystem::Stop()
{
    HYP_CORE_ASSERT(IsRunning(), "TaskSystem::Start() must be called before TaskSystem::Stop()");

    m_running.Set(false, MemoryOrder::RELAXED);

    Array<TaskThread*> taskThreads;

    for (const UniquePtr<TaskThreadPool>& pool : m_pools)
    {
        pool->Stop(taskThreads);
    }

    while (taskThreads.Any())
    {
        TaskThread* top = taskThreads.PopBack();
        top->Join();
    }
}

TaskBatch* TaskSystem::EnqueueBatch(TaskBatch* batch)
{
    HYP_CORE_ASSERT(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

    HYP_CORE_ASSERT(batch != nullptr);
    HYP_CORE_ASSERT(batch->IsCompleted(), "TaskBatch::ResetState() must be called before enqueuing tasks");

#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
    HYP_MT_CHECK_READ(batch->dataRaceDetector);
#endif

    TaskBatch* nextBatch = batch->nextBatch;

    if (batch->executors.Empty())
    {
        // enqueue next batch immediately if it exists and no tasks are added to this batch
        batch->OnComplete();
        batch->numEnqueued = 0;

        if (nextBatch != nullptr)
        {
            EnqueueBatch(nextBatch);
        }

        return batch;
    }

    batch->numEnqueued = uint32(batch->executors.Size());
    batch->notifier.SetTargetValue(batch->numEnqueued);

    TaskThreadPool* pool = nullptr;

    if (batch->pool != nullptr)
    {
        HYP_CORE_ASSERT(batch->pool->IsRunning(), "Start() must be called on a TaskThreadPool before enqueuing tasks to it");

        pool = batch->pool;
    }
    else
    {
        pool = m_pools[uint32(TaskThreadPoolName::THREAD_POOL_GENERIC)].Get();
    }

#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
    HYP_MT_CHECK_RW(batch->dataRaceDetector);
#endif

    for (TaskExecutorInstance<void>& executor : batch->executors)
    {
        TaskThread* taskThread = pool->GetNextTaskThread();
        HYP_CORE_ASSERT(taskThread != nullptr);

        const TaskID taskId = taskThread->GetScheduler().EnqueueTaskExecutor(
            &executor,
            &batch->notifier,
            nextBatch != nullptr
                ? OnTaskCompletedCallback([this, &onComplete = batch->OnComplete, nextBatch]()
                      {
                          onComplete();

                          EnqueueBatch(nextBatch);
                      })
                : OnTaskCompletedCallback(batch->OnComplete ? &batch->OnComplete : nullptr));

        batch->taskRefs.EmplaceBack(taskId, &taskThread->GetScheduler());
    }

    return batch;
}

Array<bool> TaskSystem::DequeueBatch(TaskBatch* batch)
{
    HYP_CORE_ASSERT(IsRunning(), "TaskSystem::Start() must be called before dequeuing tasks");

    HYP_CORE_ASSERT(batch != nullptr);

    Array<bool> results;
    results.Resize(batch->taskRefs.Size());

    for (SizeType i = 0; i < batch->taskRefs.Size(); i++)
    {
        const TaskRef& taskRef = batch->taskRefs[i];

        if (!taskRef.IsValid())
        {
            continue;
        }

        results[i] = taskRef.assignedScheduler->Dequeue(taskRef.id);
    }

    return results;
}

TaskThread* TaskSystem::GetNextTaskThread(TaskThreadPool& pool)
{
    return pool.GetNextTaskThread();
}

#pragma endregion TaskSystem

const FlatMap<TaskThreadPoolName, UniquePtr<TaskThreadPool> (*)(void)> g_threadPoolFactories {
    { TaskThreadPoolName::THREAD_POOL_GENERIC, +[]() -> UniquePtr<TaskThreadPool>
        {
            // we generally don't have more than 3 concurrent Systems running at once.
            return MakeUnique<GenericTaskThreadPool>(3, ThreadPriorityValue::HIGHEST);
        } },
    { TaskThreadPoolName::THREAD_POOL_RENDER, +[]() -> UniquePtr<TaskThreadPool>
        {
            return MakeUnique<RenderTaskThreadPool>(2, ThreadPriorityValue::HIGHEST);
        } },
    { TaskThreadPoolName::THREAD_POOL_BACKGROUND, +[]() -> UniquePtr<TaskThreadPool>
        {
            return MakeUnique<BackgroundTaskThreadPool>(1, ThreadPriorityValue::LOWEST);
        } }
};

} // namespace threading
} // namespace hyperion

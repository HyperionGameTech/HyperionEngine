/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/TaskSystem.hpp>
#include <core/threading/ThreadPool.hpp>

#include <core/reflection/HypClass.hpp>

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
        HYP_FAIL("TaskBatch::AwaitCompletion() called before all tasks were enqueued! Expected {} tasks, but only {} were enqueued.",
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
            AssertDebug(taskRef.assignedScheduler != nullptr);

            Assert(taskRef.assignedScheduler->GetOwnerThread() != currentThreadId,
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

        AssertDebug(threadPoolFactoriesIt != endIt, "Invalid thread pool index {}", i);

        m_pools.PushBack(threadPoolFactoriesIt->second());
    }
}

void TaskSystem::Start()
{
    AssertDebug(!IsRunning(), "TaskSystem::Start() has already been called");

    for (const UniquePtr<TaskThreadPool>& pool : m_pools)
    {
        pool->Start();
    }

    m_running.Set(true, MemoryOrder::RELAXED);
}

void TaskSystem::Stop()
{
    AssertDebug(IsRunning(), "TaskSystem::Start() must be called before TaskSystem::Stop()");

    m_running.Set(false, MemoryOrder::RELAXED);

    for (const UniquePtr<TaskThreadPool>& pool : m_pools)
    {
        pool->Stop();
    }
}

TaskBatch* TaskSystem::EnqueueBatch(TaskBatch* batch)
{
    AssertDebug(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

    AssertDebug(batch != nullptr);
    AssertDebug(batch->IsCompleted(), "TaskBatch::ResetState() must be called before enqueuing tasks");

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
        AssertDebug(batch->pool->IsRunning(), "Start() must be called on a TaskThreadPool before enqueuing tasks to it");

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
        AssertDebug(taskThread != nullptr);

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
    AssertDebug(IsRunning(), "TaskSystem::Start() must be called before dequeuing tasks");

    AssertDebug(batch != nullptr);

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
            return MakeUnique<BackgroundTaskThreadPool>("BackgroundTask", BackgroundTaskThreadPool::MaxBackgroundThreads);
        } }
};

} // namespace threading
} // namespace hyperion

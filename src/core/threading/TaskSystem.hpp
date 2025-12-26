/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/functional/Proc.hpp>
#include <core/functional/Delegate.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Defines.hpp>
#include <core/utilities/FunctionTraits.hpp>

#include <core/debug/Debug.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/ThreadPool.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Types.hpp>

#include <atomic>

// #define HYP_TASK_BATCH_DATA_RACE_DETECTION

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Tasks);

HYP_API extern Pool* GetTaskPool();

namespace threading {

enum TaskThreadPoolName : uint32
{
    THREAD_POOL_GENERIC,
    THREAD_POOL_RENDER,
    THREAD_POOL_BACKGROUND,
    THREAD_POOL_MAX
};

class ThreadPoolBase;
class TaskThreadPool;
class TaskSystem;

using OnTaskBatchCompletedCallback = Proc<void()>;

class TaskBatch
{
public:
    HYP_DEF_POOL_NEW_DELETE(GetTaskPool());

    TaskCompleteNotifier notifier;
    uint32 numEnqueued = 0;

    /*! \brief The priority / pool lane for which to place
     * all of the threads in this batch into
     */
    TaskThreadPool* pool = nullptr;

    /* Tasks must remain constant from creation of the TaskBatch to completion. */
    Array<TaskExecutorInstance<void>> executors;

    /* TaskRefs to be set by the TaskSystem, holding task ids and pointers to the threads
     * each task has been scheduled to. */
    Array<TaskRef> taskRefs;

    /*! \An optional dependent batch to be ran after this one has been completed.
     *   This TaskBatch does not own nextBatch, and will not delete it.
     *  proper cleanup must be done by the user. */
    TaskBatch* nextBatch = nullptr;

#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
    DataRaceDetector dataRaceDetector;
#endif

    /*! \brief Delegate that is called when the TaskBatch is complete (before nextBatch has completed) */
    Delegate<void> OnComplete;

    TaskBatch() = default;

    TaskBatch(const TaskBatch&) = delete;
    TaskBatch& operator=(const TaskBatch&) = delete;

    TaskBatch(TaskBatch&& other) noexcept = delete;
    TaskBatch& operator=(TaskBatch&& other) noexcept = delete;

    virtual ~TaskBatch()
    {
        HYP_CORE_ASSERT(IsCompleted(), "TaskBatch must be in completed state by the time the destructor is called!");
    }

    /*! \brief Add a task to be ran with this batch
     *  \note Do not call this function after the TaskBatch has been enqueued (before it has been completed). */
    template <class Function>
    HYP_FORCE_INLINE void AddTask(Function&& fn)
    {
#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
        HYP_MT_CHECK_RW(dataRaceDetector);
#endif

        executors.EmplaceBack(std::forward<Function>(fn));
    }

    /*! \brief Check if all tasks in the batch have been completed. */
    HYP_API bool IsCompleted() const;

    /*! \brief Block the current thread until all tasks have been marked as completed. */
    HYP_API void AwaitCompletion();

    /*! \brief Execute each non-enqueued task in serial (not async).
     *  \param executeDependentBatches If true, the nextBatch will also be executed. */
    void ExecuteBlocking(bool executeDependentBatches = false)
    {
#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
        HYP_MT_CHECK_RW(dataRaceDetector);
#endif

        for (auto& it : executors)
        {
            it.Execute();
            notifier.Release(1);
        }

        OnComplete();

        if (executeDependentBatches && nextBatch != nullptr)
        {
            nextBatch->ExecuteBlocking(executeDependentBatches);
        }
    }

    void ResetState()
    {
#ifdef HYP_TASK_BATCH_DATA_RACE_DETECTION
        HYP_MT_CHECK_RW(dataRaceDetector);
#endif

        HYP_CORE_ASSERT(IsCompleted(), "TaskBatch::ResetState() must be called after all tasks have been completed");

        notifier.Reset();
        numEnqueued = 0;
        executors.Clear();
        taskRefs.Clear();
        nextBatch = nullptr;

        OnComplete.RemoveAllDetached();
    }
};

class TaskSystem
{
public:
    HYP_API static TaskSystem& GetInstance();

    TaskSystem();

    TaskSystem(const TaskSystem& other) = delete;
    TaskSystem& operator=(const TaskSystem& other) = delete;

    TaskSystem(TaskSystem&& other) noexcept = delete;
    TaskSystem& operator=(TaskSystem&& other) noexcept = delete;

    ~TaskSystem() = default;

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_running.Get(MemoryOrder::RELAXED);
    }

    HYP_API void Start();
    HYP_API void Stop();

    HYP_FORCE_INLINE TaskThreadPool& GetPool(TaskThreadPoolName poolName) const
    {
        return *m_pools[uint32(poolName)];
    }

    HYP_FORCE_INLINE TaskThread* GetTaskThread(TaskThreadPoolName poolName, ThreadId threadId) const
    {
        return GetPool(poolName).GetTaskThread(threadId);
    }

    HYP_FORCE_INLINE TaskThread* GetTaskThread(ThreadId threadId) const
    {
        for (const UniquePtr<TaskThreadPool>& pool : m_pools)
        {
            if (TaskThread* taskThread = pool->GetTaskThread(threadId))
            {
                return taskThread;
            }
        }

        return nullptr;
    }

    template <class Function>
    auto Enqueue(const StaticMessage& debugName, Function&& fn, TaskThreadPool& pool, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        HYP_CORE_ASSERT(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

        return pool.Enqueue(debugName, std::forward<Function>(fn), flags);
    }

    template <class Function>
    auto Enqueue(Function&& fn, TaskThreadPool& pool, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        HYP_CORE_ASSERT(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

        return pool.Enqueue(std::forward<Function>(fn), flags);
    }

    template <class Function>
    auto Enqueue(const StaticMessage& debugName, Function&& fn, TaskThreadPoolName poolName = THREAD_POOL_GENERIC, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        HYP_CORE_ASSERT(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

        TaskThreadPool& pool = GetPool(poolName);

        return pool.Enqueue(debugName, std::forward<Function>(fn), flags);
    }

    template <class Function>
    auto Enqueue(Function&& fn, TaskThreadPoolName poolName = THREAD_POOL_GENERIC, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        HYP_CORE_ASSERT(IsRunning(), "TaskSystem::Start() must be called before enqueuing tasks");

        TaskThreadPool& pool = GetPool(poolName);

        return pool.Enqueue(std::forward<Function>(fn), flags);
    }

    /*! \brief Enqueue a batch of multiple Tasks. Each task will be enqueued to run in parallel.
     *  You will need to call AwaitCompletion() before the underlying TaskBatch is destroyed.
     *  If enqueuing a batch with dependent tasks via \ref{TaskBatch::nextBatch}, ensure all tasks are added to the next batch (and any proceding next batches in the chain)
     *  before calling this.
     *  \param batch Pointer to the TaskBatch to enqueue
     *  \param callback Optional callback to be called when all tasks in the batch have finished executing.
     */
    HYP_API TaskBatch* EnqueueBatch(TaskBatch* batch);

    /*! \brief Dequeue each task in a TaskBatch. A potentially expensive operation,
     * as each task will have to individually be dequeued, performing a lock operation.
     * @param batch Pointer to the TaskBatch to dequeue
     * @returns A Array<bool> containing for each task that has been enqueued, whether or not
     * it was successfully dequeued.
     */
    HYP_API Array<bool> DequeueBatch(TaskBatch* batch);

    /*! \brief Creates a TaskBatch which will call the lambda for \p numItems times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(uint32 numItems, CallbackFunction&& cb)
    {
        TaskThreadPool& pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            pool,
            pool.GetProcessorAffinity(),
            numItems,
            std::forward<CallbackFunction>(cb));
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \p numItems times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the given priority.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPool& pool, uint32 numItems, CallbackFunction&& cb)
    {
        ParallelForEach(
            pool,
            pool.GetProcessorAffinity(),
            numItems,
            std::forward<CallbackFunction>(cb));
    }

    /*! \brief Creates a TaskBatch which will call the lambda for \p numItems times in parallel.
     *  The tasks will be split evenly into \p numBatches batches.
        The lambda will be called with (item, index) for each item. */
    template <class CallbackFunction>
    void ParallelForEach(TaskThreadPool& pool, uint32 numBatches, uint32 numItems, CallbackFunction&& cb)
    {
        TaskBatch batch;
        batch.pool = &pool;

        if (numItems == 0)
        {
            return;
        }

        numBatches = MathUtil::Clamp(numBatches, 1u, numItems);

        const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

        for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
        {
            batch.AddTask([batchIndex, itemsPerBatch, numItems, &cb](...)
                {
                    const uint32 offsetIndex = batchIndex * itemsPerBatch;
                    const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

                    for (uint32 i = offsetIndex; i < maxIndex; i++)
                    {
                        cb(i, batchIndex);
                    }
                });
        }

        EnqueueBatch(&batch);
        batch.AwaitCompletion();
    }

    template <class Container, class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(TaskThreadPool& pool, Container&& items, CallbackFunction&& cb)
    {
        ParallelForEach(
            pool,
            pool.GetProcessorAffinity(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb));
    }

    template <class Container, class CallbackFunction>
    HYP_FORCE_INLINE void ParallelForEach(Container&& items, CallbackFunction&& cb)
    {
        TaskThreadPool& pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            pool,
            pool.GetProcessorAffinity(),
            std::forward<Container>(items),
            std::forward<CallbackFunction>(cb));
    }

    template <class Container, class CallbackFunction>
    void ParallelForEach(TaskThreadPool& pool, uint32 numBatches, Container&& items, CallbackFunction&& cb)
    {
        TaskBatch batch;
        batch.pool = &pool;
        const uint32 numItems = uint32(items.Size());

        if (numItems == 0)
        {
            return;
        }

        numBatches = MathUtil::Clamp(numBatches, 1u, numItems);

        const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

        auto* dataPtr = items.Data();

        for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
        {
            batch.AddTask([dataPtr, batchIndex, itemsPerBatch, numItems, &cb](...)
                {
                    const uint32 offsetIndex = batchIndex * itemsPerBatch;
                    const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

                    for (uint32 i = offsetIndex; i < maxIndex; i++)
                    {
                        cb(*(dataPtr + i), i, batchIndex);
                    }
                });
        }

        EnqueueBatch(&batch);
        batch.AwaitCompletion();
    }

    template <class Container, class Callback>
    void ParallelForEach_Batch(TaskBatch& batch, uint32 numBatches, Container&& items, Callback&& cb)
    {
        const uint32 numItems = uint32(items.Size());

        if (numItems == 0)
        {
            return;
        }

        numBatches = MathUtil::Clamp(numBatches, 1u, numItems);

        const uint32 itemsPerBatch = (numItems + numBatches - 1) / numBatches;

        auto* dataPtr = items.Data();

        auto procRef = ProcRef(std::forward<Callback>(cb));

        for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
        {
            batch.AddTask([dataPtr, batchIndex, itemsPerBatch, numItems, procRef](...)
                {
                    const uint32 offsetIndex = batchIndex * itemsPerBatch;
                    const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);

                    for (uint32 i = offsetIndex; i < maxIndex; i++)
                    {
                        procRef(*(dataPtr + i), i, batchIndex);
                    }
                });
        }
    }

    HYP_FORCE_INLINE bool CancelTask(const TaskRef& taskRef)
    {
        if (!taskRef.IsValid())
        {
            return false;
        }

        return taskRef.assignedScheduler->Dequeue(taskRef.id);
    }

private:
    HYP_API TaskThread* GetNextTaskThread(TaskThreadPool& pool);

    Array<UniquePtr<TaskThreadPool>> m_pools;
    Array<TaskBatch*> m_runningBatches;
    AtomicVar<bool> m_running;
};

} // namespace threading

using TaskSystem = threading::TaskSystem;
using TaskBatch = threading::TaskBatch;
using TaskRef = threading::TaskRef;
using TaskThreadPool = threading::TaskThreadPool;
using TaskThreadPoolName = threading::TaskThreadPoolName;

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/List.hpp>

#include <Core/Functional/Proc.hpp>
#include <Core/Functional/Delegate.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Core/Defines.hpp>
#include <Core/Utilities/FunctionTraits.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/TaskThread.hpp>
#include <Core/Threading/ThreadPool.hpp>
#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/DataRaceDetector.hpp>
#include <Core/Threading/Semaphore.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Types.hpp>

#include <atomic>

// #define HYP_TASK_BATCH_DATA_RACE_DETECTION

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Threading);

namespace threading {

enum TaskThreadPoolName : uint32
{
    THREAD_POOL_GENERIC,
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
    CORE_API bool IsCompleted() const;

    /*! \brief Block the current thread until all tasks have been marked as completed. */
    CORE_API void AwaitCompletion();

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
            it.Complete(&notifier);
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
    CORE_API static TaskSystem& GetInstance();

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

    CORE_API void Start();
    CORE_API void Stop();

    CORE_API void RegisterPool(TaskThreadPoolName poolName, UniquePtr<TaskThreadPool>&& pool);

    HYP_FORCE_INLINE TaskThreadPool& GetPool(TaskThreadPoolName poolName) const
    {
        Assert(m_pools[uint32(poolName)] != nullptr);

        return *m_pools[uint32(poolName)];
    }

    HYP_FORCE_INLINE ThreadBase* GetTaskThread(TaskThreadPoolName poolName, ThreadId threadId) const
    {
        return GetPool(poolName).GetTaskThread(threadId);
    }

    HYP_FORCE_INLINE ThreadBase* GetTaskThread(ThreadId threadId) const
    {
        for (const UniquePtr<TaskThreadPool>& pool : m_pools)
        {
            if (!pool)
            {
                continue;
            }

            if (ThreadBase* taskThread = pool->GetTaskThread(threadId))
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
    CORE_API TaskBatch* EnqueueBatch(TaskBatch* batch);

    /*! \brief Dequeue each task in a TaskBatch. A potentially expensive operation,
     * as each task will have to individually be dequeued, performing a lock operation.
     * @param batch Pointer to the TaskBatch to dequeue
     * @returns A Array<bool> containing for each task that has been enqueued, whether or not
     * it was successfully dequeued.
     */
    CORE_API Array<bool> DequeueBatch(TaskBatch* batch);

    /*! \brief Creates a TaskBatch which will call the lambda for \p numItems times in parallel.
     *  The tasks will be split evenly into groups, based on the number of threads in the pool for the default priority.
        The lambda will be called with (item, index) for each item. */
    template <class Callback>
    HYP_FORCE_INLINE void ParallelForEach(uint32 numItems, Callback&& cb)
    {
        TaskThreadPool& pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            pool,
            numItems,
            std::forward<Callback>(cb));
    }

    template <class Callback>
    void ParallelForEach(TaskThreadPool& pool, uint32 numItems, Callback&& cb)
    {
        Array<Task<void>> tasks;
        tasks.Reserve(numItems);

        for (uint32 i = 0; i < numItems; i++)
        {
            tasks.EmplaceBack(TaskSystem::GetInstance().Enqueue([&cb, i]() { cb(i); }, pool));
        }

        AwaitAll(tasks.ToSpan());
    }

    template <class Container, class Callback>
    void ParallelForEach(Container&& items, Callback&& cb)
    {
        TaskThreadPool& pool = GetPool(THREAD_POOL_GENERIC);

        ParallelForEach(
            pool,
            std::forward<Container>(items),
            std::forward<Callback>(cb));
    }

    template <class Container, class Callback>
    void ParallelForEach(TaskThreadPool& pool, Container&& items, Callback&& cb)
    {
        Array<Task<void>> tasks;
        tasks.Reserve(items.Size());

        for (size_t i = 0; i < items.Size(); i++)
        {
            tasks.EmplaceBack(TaskSystem::GetInstance().Enqueue([&items, &cb, i]() { cb(items[i], uint32(i)); }, pool));
        }

        AwaitAll(tasks.ToSpan());
    }

    template <class Container, class Callback>
    void ParallelForEach_Batch(TaskBatch& batch, uint8 numBatches, Container&& items, Callback&& cb)
    {
        static_assert(std::is_lvalue_reference_v<decltype(cb)> || std::is_base_of_v<functional::ProcRefBase, NormalizedType<decltype(cb)>>,
            "Callback type must be lvalue reference or already be wrapped by ProcRef, otherwise it will become dangling");

        const uint32 numItems = uint32(items.Size());

        if (numItems == 0)
        {
            return;
        }

        numBatches = uint8(MathUtil::Clamp(uint32(numBatches), 1u, MathUtil::Min(numItems, uint32(UINT8_MAX))));

        const uint16 itemsPerBatch = uint16((numItems + numBatches - 1) / numBatches);

        auto* dataPtr = items.Data();

        auto procRef = ProcRef(cb);

        for (uint8 batchIndex = 0; batchIndex < numBatches; batchIndex++)
        {
            const uint32 offsetIndex = uint32(batchIndex * itemsPerBatch);
            const uint16 maxIndex = uint16(MathUtil::Min(offsetIndex + itemsPerBatch, numItems) - offsetIndex);

            struct Functor
            {
                decltype(procRef) callback;
                decltype(dataPtr) data;
                uint16 maxIndex;
                uint8 batchIndex;

                void operator()()
                {
                    for (uint16 i = 0; i < maxIndex; i++)
                    {
                        callback(*(data + i), i, batchIndex);
                    }
                }
            };

            Functor f;
            f.data = dataPtr + uint32(batchIndex * itemsPerBatch);
            f.callback = procRef;
            f.maxIndex = maxIndex;
            f.batchIndex = batchIndex;

            batch.AddTask(f);
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
    CORE_API TaskThread* GetNextTaskThread(TaskThreadPool& pool);

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

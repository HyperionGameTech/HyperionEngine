/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Threading/ThreadPool.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <chrono>
#include <thread>

namespace Hyperion {
namespace threading {

constexpr bool EnableCleanupIdleBackgroundThreads = false;

#pragma region ThreadPoolBase

ThreadPoolBase::ThreadPoolBase()
    : m_threadMask(0)
{
}

ThreadPoolBase::ThreadPoolBase(Array<UniquePtr<ThreadBase>>&& threads)
    : m_threads(std::move(threads)),
      m_threadMask(0)
{
    for (const UniquePtr<ThreadBase>& thread : m_threads)
    {
        AssertDebug(thread != nullptr);

        m_threadMask |= thread->Id().GetMask();
    }
}

ThreadPoolBase::~ThreadPoolBase()
{
    HYP_LOG(Threading, Verbose, "Destroying thread pool with {} threads", m_threads.Size());

    for (const UniquePtr<ThreadBase>& thread : m_threads)
    {
        AssertDebug(thread != nullptr);

        if (thread->CanJoin())
        {
            thread->GetScheduler().WakeUpOwnerThread();
            thread->Join();
        }
    }
}

bool ThreadPoolBase::IsRunning() const
{
    if (m_threads.Empty())
    {
        return false;
    }

    for (const UniquePtr<ThreadBase>& thread : m_threads)
    {
        HYP_CORE_ASSERT(thread != nullptr);

        if (thread->IsRunning())
        {
            return true;
        }
    }

    return false;
}

void ThreadPoolBase::Stop()
{
    if (!IsRunning())
    {
        return;
    }

    for (const UniquePtr<ThreadBase>& thread : m_threads)
    {
        HYP_CORE_ASSERT(thread != nullptr);

        thread->Stop();
    }
}

ThreadBase* ThreadPoolBase::GetNextThread()
{
    const uint32 numThreadsInPool = uint32(m_threads.Size());

    uint32 cycle = m_cycle.Get(MemoryOrder::RELAXED) % numThreadsInPool;

    ThreadBase* thread = m_threads[cycle].Get();

    cycle = (cycle + 1) % numThreadsInPool;
    m_cycle.Increment(1, MemoryOrder::RELAXED);

    return thread;
}

ThreadId ThreadPoolBase::CreateTaskThreadId(ANSIStringView baseName, uint32 threadIndex)
{
    return ThreadId(NAME_FMT("{}{}", baseName, threadIndex), THREAD_CATEGORY_TASK);
}

#pragma endregion ThreadPoolBase

#pragma region TaskThreadPool

TaskThreadPool::TaskThreadPool()
    : ThreadPoolBase()
{
}

TaskThreadPool::TaskThreadPool(Array<UniquePtr<TaskThread>>&& threads)
{
    m_threads.Reserve(threads.Size());
    m_threadMask = 0;

    uint32 threadIndex = 0;

    for (UniquePtr<TaskThread>& thread : threads)
    {
        thread->SetThreadIndex(threadIndex);
        thread->SetOwnerPool(this);

        m_threadMask |= thread->Id().GetMask();
        m_threads.PushBack(std::move(thread));

        ++threadIndex;
    }
}

void TaskThreadPool::Start()
{
    for (auto& it : m_threads)
    {
        AssertDebug(it != nullptr);

        TaskThread* taskThread = static_cast<TaskThread*>(it.Get());
        taskThread->Start();

        while (!taskThread->IsRunning())
        {
            HYP_WAIT_IDLE();
        }
    }
}

TaskThread* TaskThreadPool::GetNextTaskThread()
{
    static constexpr uint32 MaxSpins = 16;

    const uint32 numThreadsInPool = uint32(m_threads.Size());

    const ThreadId currentThreadId = CurrentThreadId();
    const bool isOnTaskThread = (m_threadMask & currentThreadId.GetMask()) != 0;

    ThreadBase* currentThreadObject = CurrentThreadObject();

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
            taskThread = static_cast<TaskThread*>(m_threads[cycle].Get());

            cycle = (cycle + 1) % numThreadsInPool;
            m_cycle.Increment(1, MemoryOrder::RELAXED);

            ++numSpins;

            if (numSpins >= MaxSpins)
            {
                if (isOnTaskThread)
                {
                    return static_cast<TaskThread*>(currentThreadObject); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                }

                HYP_LOG(Threading, Warning, "Maximum spins reached in GetNextTaskThread -- all task threads busy");

                return taskThread;
            }
        }
        while (taskThread->Id() == currentThreadId
               || (currentThreadObject != nullptr && currentThreadObject->GetScheduler().HasWorkAssignedFromThread(taskThread->Id())));
    }
    while (!taskThread->IsRunning() && !taskThread->IsFree());

    return taskThread;
}

bool TaskThreadPool::TryStealTask(TaskThread* thief, Scheduler::ScheduledTask& outTask)
{
    const uint32 numThreads = uint32(m_threads.Size());

    if (numThreads <= 1)
    {
        return false;
    }

    // Start stealing from a random offset to reduce contention
    // Using cycle counter as a pseudo-random offset
    const uint32 startIndex = m_cycle.Get(MemoryOrder::RELAXED) % numThreads;

    for (uint32 i = 0; i < numThreads; i++)
    {
        const uint32 index = (startIndex + i) % numThreads;
        TaskThread* victim = static_cast<TaskThread*>(m_threads[index].Get());

        if (victim == thief)
        {
            continue;
        }

        if (victim->GetScheduler().TryStealFrom(outTask))
        {
            return true;
        }
    }

    return false;
}

#pragma endregion TaskThreadPool

#pragma region BackgroundWorkerPool

BackgroundWorkerPool::BackgroundWorkerPool(ANSIStringView baseName, uint32 maxThreads)
    : TaskThreadPool(),
      m_baseName(baseName),
      m_maxThreads(maxThreads),
      m_overseerThread(nullptr)
{
    AssertDebug(maxThreads > 0, "BackgroundWorkerPool must have at least one thread");
}

BackgroundWorkerPool::~BackgroundWorkerPool()
{
    Stop();
}

bool BackgroundWorkerPool::IsRunning() const
{
    // consider the background worker pool to be 'running' as long as Start() is been called
    // we may not have any threads yet, therefore we need to override the default behaviour which
    // returns false when there are no threads in a pool.
    return (m_overseerThread != nullptr);
}

void BackgroundWorkerPool::Start()
{
    if (m_overseerThread != nullptr)
    {
        return; // Already started
    }

    m_overseerShouldStop.Set(false, MemoryOrder::RELEASE);

    class OverseerThread final : public TaskThread
    {
    public:
        OverseerThread(BackgroundWorkerPool* pool)
            : TaskThread(ThreadId(Name::Unique(HYP_FORMAT("{}_Overseer", pool->m_baseName).Data()), THREAD_CATEGORY_TASK), ThreadPriorityValue::LOWEST),
              m_pool(pool)
        {
        }

        virtual void operator()() override
        {
            while (!m_pool->m_overseerShouldStop.Get(MemoryOrder::ACQUIRE))
            {
                {
                    Mutex::Guard lock(m_pool->m_overseerMutex);
                    m_pool->m_overseerCV.WaitFor(m_pool->m_overseerMutex, 1000);
                }

                if (m_pool->m_overseerShouldStop.Get(MemoryOrder::ACQUIRE))
                {
                    break;
                }

                if constexpr (EnableCleanupIdleBackgroundThreads)
                {
                    m_pool->CleanupIdleThreads();
                }
            }
        }

    private:
        BackgroundWorkerPool* m_pool;
    };

    OverseerThread* overseerThread = new OverseerThread(this);
    overseerThread->Start();

    m_overseerThread = overseerThread;
}

void BackgroundWorkerPool::Stop()
{
    if (m_overseerThread == nullptr)
    {
        return;
    }

    m_overseerShouldStop.Set(true, MemoryOrder::RELEASE);

    WakeOverseer();

    m_overseerThread->Stop();
    m_overseerThread->Join();

    delete m_overseerThread;
    m_overseerThread = nullptr;

    Mutex::Guard guard(m_threadCreationMutex);

    for (auto& thread : m_threads)
    {
        if (thread != nullptr)
        {
            thread->Stop();
        }
    }

    for (auto& thread : m_threads)
    {
        if (thread != nullptr && thread->CanJoin())
        {
            thread->Join();
        }
    }

    m_threads.Clear();
    m_workerIndexAllocator.Reset();
    m_threadMask = 0;
    m_activeThreadCount.Set(0, MemoryOrder::RELEASE);
}

TaskThread* BackgroundWorkerPool::GetNextTaskThread()
{
    Mutex::Guard guard(m_threadCreationMutex);

    // look for a free running thread
    for (auto& thread : m_threads)
    {
        if (thread != nullptr)
        {
            TaskThread* taskThread = static_cast<TaskThread*>(thread.Get());

            if (!taskThread->IsStopping() && taskThread->IsFree())
            {
                return taskThread;
            }
        }
    }

    // all threads are busy

    const uint32 activeCount = m_activeThreadCount.Get(MemoryOrder::ACQUIRE);

    // can create a thread
    if (activeCount < m_maxThreads)
    {
        return CreateThread();
    }

    TaskThread* taskThread = nullptr;

    uint32 seed = m_cycle.Increment(1, MemoryOrder::RELAXED);

    while (!taskThread)
    {
        uint32 index = MathUtil::Floor(MathUtil::RandomFloat(seed) * float(activeCount - 1));
        taskThread = static_cast<TaskThread*>(m_threads[index].Get());
    }

    return taskThread;
}

TaskThread* BackgroundWorkerPool::CreateThread()
{
    const uint32 threadIndex = m_workerIndexAllocator.Allocate();

    ThreadId threadId = CreateTaskThreadId(m_baseName, threadIndex);

    UniquePtr<ThreadBase> newThread = MakeUnique<TaskThread>(threadId, ThreadPriorityValue::LOW);
    TaskThread* taskThread = static_cast<TaskThread*>(newThread.Get());

    m_threadMask |= threadId.GetMask();

    // Start the thread immediately
    taskThread->Start();

    // wait for ready state
    while (!taskThread->IsRunning())
    {
        ThreadYield();
    }

    if (m_threads.Size() <= threadIndex)
    {
        m_threads.Resize(threadIndex + 1);
    }

    // expected to be null (nothing should exist in the new slot we allocated)
    Assert(m_threads[threadIndex] == nullptr);

    m_threads[threadIndex] = std::move(newThread);

    m_activeThreadCount.Increment(1, MemoryOrder::RELEASE);

    return taskThread;
}

void BackgroundWorkerPool::CleanupIdleThreads()
{
    Mutex::Guard guard(m_threadCreationMutex);

    Array<size_t> threadsToRemove;

    for (size_t i = 0; i < m_threads.Size(); ++i)
    {
        if (m_threads[i] == nullptr)
        {
            continue;
        }

        TaskThread* taskThread = static_cast<TaskThread*>(m_threads[i].Get());

        // If the thread is not running or is free (no pending tasks), mark it for cleanup
        if (!taskThread->IsRunning() || taskThread->IsFree())
        {
            taskThread->Stop();

            threadsToRemove.PushBack(i);
        }
    }

    // Remove threads in reverse order to maintain array indices
    for (size_t i = threadsToRemove.Size(); i > 0; --i)
    {
        const size_t index = threadsToRemove[i - 1];

        ThreadBase* thread = m_threads[index].Get();
        Assert(thread != nullptr);

        if (thread != nullptr)
        {
            if (thread->IsRunning() || thread->GetScheduler().NumEnqueued() > 0)
            {
                // skip for now, stuff still running
                continue;
            }

            if (thread->CanJoin())
            {
                thread->Join();
            }

            HYP_LOG(Threading, Verbose, "BackgroundWorkerPool cleaned up idle thread: {}", thread->Id().GetName());
        }

        // release the thread at that index;
        m_threads[index].Reset();

        m_activeThreadCount.Decrement(1, MemoryOrder::RELEASE);
        m_workerIndexAllocator.Free(index);
    }
}

void BackgroundWorkerPool::WakeOverseer()
{
    Mutex::Guard lock(m_overseerMutex);

    m_overseerCV.NotifyOne();
}

#pragma endregion BackgroundWorkerPool

} // namespace threading
} // namespace Hyperion

/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/threading/ThreadPool.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/Format.hpp>

#include <core/math/MathUtil.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <chrono>
#include <thread>

namespace hyperion {
namespace threading {

constexpr bool EnableCleanupIdleBackgroundThreads = false; // tmp debugging

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
    for (auto& it : m_threads)
    {
        AssertDebug(it != nullptr);
        AssertDebug(!it->IsRunning(), "ThreadPoolBase::Stop() must be called before ThreadPoolBase is destroyed");

        if (it->CanJoin())
        {
            it->Join();
        }
    }
}

bool ThreadPoolBase::IsRunning() const
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

void ThreadPoolBase::Stop()
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
    return ThreadId(Name::Unique(HYP_FORMAT("{}{}", baseName, threadIndex).Data()), THREAD_CATEGORY_TASK);
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

    for (UniquePtr<TaskThread>& thread : threads)
    {
        thread->SetOwnerPool(this);

        m_threadMask |= thread->Id().GetMask();
        m_threads.PushBack(std::move(thread));
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
    static constexpr uint32 maxSpins = 16;

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

#pragma region BackgroundTaskThreadPool

BackgroundTaskThreadPool::BackgroundTaskThreadPool(ANSIStringView baseName, uint32 maxThreads)
    : TaskThreadPool(),
      m_baseName(baseName),
      m_maxThreads(maxThreads),
      m_overseerThread(nullptr)
{
    AssertDebug(maxThreads > 0, "BackgroundTaskThreadPool must have at least one thread");
}

BackgroundTaskThreadPool::~BackgroundTaskThreadPool()
{
    Stop();
}

void BackgroundTaskThreadPool::Start()
{
    if (m_overseerThread != nullptr)
    {
        return; // Already started
    }

    m_overseerShouldStop.Set(false, MemoryOrder::RELEASE);

    class OverseerThread final : public TaskThread
    {
    public:
        OverseerThread(BackgroundTaskThreadPool* pool)
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

                    m_pool->m_overseerCV.WaitFor(m_pool->m_overseerMutex, 30000);
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
        BackgroundTaskThreadPool* m_pool;
    };

    OverseerThread* overseerThread = new OverseerThread(this);
    overseerThread->Start();

    m_overseerThread = overseerThread;
}

void BackgroundTaskThreadPool::Stop()
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
    m_threadMask = 0;
    m_activeThreadCount.Set(0, MemoryOrder::RELEASE);
}

TaskThread* BackgroundTaskThreadPool::GetNextTaskThread()
{
    Mutex::Guard guard(m_threadCreationMutex);

    // look for a free running thread
    for (auto& thread : m_threads)
    {
        if (thread != nullptr)
        {
            TaskThread* taskThread = static_cast<TaskThread*>(thread.Get());

            if (taskThread->IsRunning() && taskThread->IsFree())
            {
                return taskThread;
            }
        }
    }

    const uint32 activeCount = m_activeThreadCount.Get(MemoryOrder::ACQUIRE);

    if (activeCount < m_maxThreads)
    {
        return CreateThread();
    }

    // all threads are busy

    if (m_threads.Empty())
    {
        return CreateThread();
    }

    uint32 cycle = m_cycle.Get(MemoryOrder::RELAXED) % uint32(m_threads.Size());
    TaskThread* taskThread = static_cast<TaskThread*>(m_threads[cycle].Get());

    m_cycle.Increment(1, MemoryOrder::RELAXED);

    return taskThread;
}

TaskThread* BackgroundTaskThreadPool::CreateThread()
{
    const uint32 threadIndex = m_nextThreadIndex.Increment(1, MemoryOrder::ACQUIRE_RELEASE);

    ThreadId threadId = CreateTaskThreadId(m_baseName, threadIndex);

    UniquePtr<ThreadBase> newThread = MakeUnique<TaskThread>(threadId, ThreadPriorityValue::LOW);
    TaskThread* taskThread = static_cast<TaskThread*>(newThread.Get());

    m_threadMask |= threadId.GetMask();

    // Start the thread immediately
    taskThread->Start();

    while (!taskThread->IsRunning())
    {
        HYP_WAIT_IDLE();
    }

    m_threads.PushBack(std::move(newThread));
    m_activeThreadCount.Increment(1, MemoryOrder::RELEASE);

    return taskThread;
}

void BackgroundTaskThreadPool::CleanupIdleThreads()
{
    Mutex::Guard guard(m_threadCreationMutex);

    Array<SizeType> threadsToRemove;

    for (SizeType i = 0; i < m_threads.Size(); ++i)
    {
        if (m_threads[i] == nullptr)
        {
            continue;
        }

        TaskThread* taskThread = static_cast<TaskThread*>(m_threads[i].Get());

        // If the thread is not running or is free (no pending tasks), mark it for cleanup
        if (!taskThread->IsRunning() || taskThread->IsFree())
        {
            threadsToRemove.PushBack(i);
        }
    }

    // Remove threads in reverse order to maintain array indices
    for (SizeType i = threadsToRemove.Size(); i > 0; --i)
    {
        const SizeType index = threadsToRemove[i - 1];

        ThreadBase* thread = m_threads[index].Get();

        if (thread != nullptr)
        {
            thread->Stop();

            if (thread->CanJoin())
            {
                thread->Join();
            }

            HYP_LOG(Tasks, Debug, "BackgroundTaskThreadPool cleaned up idle thread: {}", thread->Id().GetName());
        }

        m_threads.EraseAt(index);
        m_activeThreadCount.Decrement(1, MemoryOrder::RELEASE);
    }
}

void BackgroundTaskThreadPool::WakeOverseer()
{
    Mutex::Guard lock(m_overseerMutex);

    m_overseerCV.NotifyOne();
}

#pragma endregion BackgroundTaskThreadPool

} // namespace threading
} // namespace hyperion

/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/threading/Thread.hpp>
#include <Core/threading/TaskThread.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Mutex.hpp>
#include <Core/threading/ConditionVariable.hpp>

#include <Core/utilities/Format.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/Defines.hpp>
#include <Core/utilities/FunctionTraits.hpp>
#include <Core/Types.hpp>

#include <thread>

namespace Hyperion {
namespace threading {

class ThreadId;
class ThreadBase;
class TaskThread;

/*! \brief Base class for thread pools managing worker threads.
 *  Provides core thread pool functionality including thread management,
 *  lifecycle control, and task distribution. */
class HYP_API ThreadPoolBase
{
public:
    ThreadPoolBase();
    ThreadPoolBase(Array<UniquePtr<ThreadBase>>&& threads);

    ThreadPoolBase(const ThreadPoolBase&) = delete;
    ThreadPoolBase& operator=(const ThreadPoolBase&) = delete;

    ThreadPoolBase(ThreadPoolBase&&) noexcept = delete;
    ThreadPoolBase& operator=(ThreadPoolBase&&) noexcept = delete;

    virtual ~ThreadPoolBase();

    /*! \brief Check if any thread in the pool is currently running. */
    bool IsRunning() const;

    /*! \brief Start all threads in the pool. */
    virtual void Start() = 0;

    /*! \brief Stop all threads in the pool and wait for them to complete. */
    virtual void Stop();

    HYP_FORCE_INLINE uint32 NumThreads() const
    {
        return uint32(m_threads.Size());
    }

    HYP_FORCE_INLINE const Array<UniquePtr<ThreadBase>>& GetThreads() const
    {
        return m_threads;
    }

    HYP_FORCE_INLINE uint32 GetProcessorAffinity() const
    {
        return MathUtil::Min(NumThreads(), MathUtil::Max(1u, NumCores()) - 1);
    }

    HYP_FORCE_INLINE ThreadMask GetThreadMask() const
    {
        return m_threadMask;
    }

protected:
    /*! \brief Get the next available thread from the pool using round-robin scheduling. */
    ThreadBase* GetNextThread();

    /*! \brief Create a unique ThreadId for a task thread.
     *  \param baseName Base name for the thread (e.g., "GenericTask")
     *  \param threadIndex Index of the thread in the pool */
    static ThreadId CreateTaskThreadId(ANSIStringView baseName, uint32 threadIndex);

    AtomicVar<uint32> m_cycle { 0u };
    Array<UniquePtr<ThreadBase>> m_threads;
    ThreadMask m_threadMask;
};

/*! \brief Thread pool specialized for task execution with enqueue support.
 *  Extends ThreadPoolBase with task enqueueing capabilities. */
class HYP_API TaskThreadPool : public ThreadPoolBase
{
public:
    TaskThreadPool();
    TaskThreadPool(Array<UniquePtr<TaskThread>>&& threads);

    TaskThreadPool(ANSIStringView baseName, uint32 numThreads)
        : TaskThreadPool(TypeWrapper<TaskThread>(), baseName, numThreads)
    {
    }

    template <class TaskThreadType>
    TaskThreadPool(TypeWrapper<TaskThreadType>, ANSIStringView baseName, uint32 numThreads)
    {
        static_assert(std::is_base_of_v<TaskThread, TaskThreadType>, "TaskThreadType must be a subclass of TaskThread");

        m_threadMask = 0;

        m_threads.Reserve(numThreads);

        for (uint32 threadIndex = 0; threadIndex < numThreads; threadIndex++)
        {
            UniquePtr<ThreadBase>& thread = m_threads.PushBack(MakeUnique<TaskThreadType>(CreateTaskThreadId(baseName, threadIndex)));

            TaskThread* taskThread = static_cast<TaskThread*>(thread.Get());

            taskThread->SetThreadIndex(threadIndex);
            taskThread->SetOwnerPool(this);

            m_threadMask |= thread->Id().GetMask();
        }
    }

    TaskThreadPool(const TaskThreadPool&) = delete;
    TaskThreadPool& operator=(const TaskThreadPool&) = delete;

    TaskThreadPool(TaskThreadPool&&) noexcept = delete;
    TaskThreadPool& operator=(TaskThreadPool&&) noexcept = delete;

    virtual ~TaskThreadPool() override = default;

    virtual void Start() override;

    HYP_FORCE_INLINE TaskThread* GetTaskThread(ThreadId threadId) const
    {
        const auto it = m_threads.FindIf([threadId](const UniquePtr<ThreadBase>& thread)
            {
                return thread->Id() == threadId;
            });

        if (it != m_threads.End())
        {
            return static_cast<TaskThread*>(it->Get());
        }

        return nullptr;
    }

    /*! \brief Get the next available task thread from the pool using round-robin scheduling.
     *  Avoids deadlocks by not returning the current thread if called from a task thread. */
    virtual TaskThread* GetNextTaskThread();

    /*! \brief Attempt to steal a task from any thread in the pool, excluding the thief. */
    bool TryStealTask(TaskThread* thief, Scheduler::ScheduledTask& outTask);

    /*! \brief Enqueue a task to the pool with optional debug name and flags.
     *  \param debugName Debug name for profiling/logging
     *  \param fn Function to execute
     *  \param flags Task enqueue flags
     *  \return Task handle for tracking completion */
    template <class Function>
    auto Enqueue(const StaticMessage& debugName, Function&& fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        TaskThread* taskThread = GetNextTaskThread();

        return taskThread->GetScheduler().Enqueue(debugName, std::forward<Function>(fn), flags);
    }

    /*! \brief Enqueue a task to the pool with optional flags.
     *  \param fn Function to execute
     *  \param flags Task enqueue flags
     *  \return Task handle for tracking completion */
    template <class Function>
    auto Enqueue(Function&& fn, EnumFlags<TaskEnqueueFlags> flags = TaskEnqueueFlags::NONE) -> Task<typename FunctionTraits<Function>::ReturnType>
    {
        TaskThread* taskThread = GetNextTaskThread();

        return taskThread->GetScheduler().Enqueue(std::forward<Function>(fn), flags);
    }
};

/*! \brief Background thread pool that lazily creates threads on-demand and manages thread lifecycle.
 *  Threads are created only when needed and automatically cleaned up when idle for too long.
 *  Useful for background tasks that don't require constant worker threads. */
class HYP_API BackgroundTaskThreadPool : public TaskThreadPool
{
public:
    static constexpr uint32 MaxBackgroundThreads = 6;
    static constexpr uint32 IdleTimeout = 30000; // 30 seconds

    BackgroundTaskThreadPool(ANSIStringView baseName, uint32 maxThreads = MaxBackgroundThreads);

    BackgroundTaskThreadPool(const BackgroundTaskThreadPool&) = delete;
    BackgroundTaskThreadPool& operator=(const BackgroundTaskThreadPool&) = delete;

    BackgroundTaskThreadPool(BackgroundTaskThreadPool&&) noexcept = delete;
    BackgroundTaskThreadPool& operator=(BackgroundTaskThreadPool&&) noexcept = delete;

    virtual ~BackgroundTaskThreadPool() override;

    virtual void Start() override;
    virtual void Stop() override;

    /*! \brief Get the maximum number of threads that can be created. */
    HYP_FORCE_INLINE uint32 GetMaxThreads() const
    {
        return m_maxThreads;
    }

    /*! \brief Get the number of currently active threads. */
    HYP_FORCE_INLINE uint32 GetActiveThreadCount() const
    {
        return m_activeThreadCount.Get(MemoryOrder::ACQUIRE);
    }

    TaskThread* GetNextTaskThread() override;

private:
    /*! \brief Create a new thread and add it to the pool. */
    TaskThread* CreateThread();

    /*! \brief Clean up idle threads that have been unused for too long. */
    void CleanupIdleThreads();

    /*! \brief Wake up the overseer thread to perform cleanup or stop. */
    void WakeOverseer();

    ANSIString m_baseName;
    uint32 m_maxThreads;

    AtomicVar<uint32> m_nextThreadIndex { 0u };
    AtomicVar<uint32> m_activeThreadCount { 0u };

    Mutex m_threadCreationMutex;

    ThreadBase* m_overseerThread;
    Mutex m_overseerMutex;
    ConditionVariable m_overseerCV;
    AtomicVar<bool> m_overseerShouldStop { false };
};

} // namespace threading

using threading::BackgroundTaskThreadPool;
using threading::TaskThreadPool;
using threading::ThreadPoolBase;

} // namespace Hyperion

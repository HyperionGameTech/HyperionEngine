/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/Scheduler.hpp>
#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/AtomicFlag.hpp>

#include <utility>

namespace Hyperion {
namespace threading {

class FastScheduler final : public SchedulerBase
{
    friend class TaskThreadPool;

public:
    using ScheduledTask = Scheduler::ScheduledTask;

    static constexpr uint64 RingBufferSize = 4096;
    static_assert((RingBufferSize & (RingBufferSize - 1)) == 0,
        "RingBufferSize must be a power of two so indices can be masked instead of modulo-divided");

    FastScheduler(ThreadId ownerThreadId = CurrentThreadId())
        : SchedulerBase(ownerThreadId)
    {
        for (uint64 i = 0; i < RingBufferSize; ++i)
        {
            m_ringBuffer[i].seq.Set(i, MemoryOrder::RELAXED);
        }
    }

    FastScheduler(const FastScheduler& other) = delete;
    FastScheduler& operator=(const FastScheduler& other) = delete;

    FastScheduler(FastScheduler&& other) noexcept = delete;
    FastScheduler& operator=(FastScheduler&& other) noexcept = delete;

    ~FastScheduler() override
    {
        ScheduledTask task;

        while (TryPop(task))
        {
        }
    }

    virtual TaskID EnqueueTaskExecutor(
        TaskExecutorBase* executorPtr,
        TaskCompleteNotifier* notifier,
        OnTaskCompletedCallback&& callback = nullptr,
        const StaticMessage& debugName = StaticMessage(),
        bool ownsExecutor = false) override
    {
        ScheduledTask scheduledTask;
        scheduledTask.executor = executorPtr;
        scheduledTask.ownsExecutor = ownsExecutor;
        scheduledTask.notifier = notifier;
        scheduledTask.pTaskExecutedCV = &m_taskExecutedCV;
        scheduledTask.callback = std::move(callback);
        scheduledTask.debugName = debugName;


        return Enqueue_Internal(std::move(scheduledTask));
    }

    virtual bool Dequeue(TaskID id) override
    {
        HYP_CORE_ASSERT(!id, "FastScheduler does not support random-access Dequeue");

        return false;
    }

    virtual bool TakeOwnershipOfTask(TaskID id, TaskExecutorBase* executor) override
    {
        (void)id;
        (void)executor;

        HYP_CORE_ASSERT(false, "FastScheduler does not support TakeOwnershipOfTask");

        return false;
    }

    virtual bool HasWorkAssignedFromThread(ThreadId threadId) const override
    {
        if (m_numEnqueued.Get(MemoryOrder::ACQUIRE) == 0)
        {
            return false;
        }

        for (uint64 i = 0; i < RingBufferSize; ++i)
        {
            const RingSlot& slot = m_ringBuffer[i];
            const uint64 seq = slot.seq.Get(MemoryOrder::ACQUIRE);

            if (((seq - 1u) & (RingBufferSize - 1)) != i)
            {
                continue;
            }

            const TaskExecutorBase* executor = slot.task.executor;

            if (executor != nullptr && executor->GetInitiatorThreadId() == threadId)
            {
                return true;
            }
        }

        return false;
    }

    bool TryPop(ScheduledTask& outTask)
    {
        uint64 pos = m_head.Get(MemoryOrder::RELAXED);

        for (;;)
        {
            RingSlot& slot = m_ringBuffer[pos & (RingBufferSize - 1)];
            const uint64 seq = slot.seq.Get(MemoryOrder::ACQUIRE);
            const int64 diff = int64(seq) - int64(pos + 1);

            if (diff == 0)
            {
                if (m_head.CompareExchangeWeak(pos, pos + 1, MemoryOrder::RELAXED))
                {
                    outTask = std::move(slot.task);
                    slot.seq.Set(pos + RingBufferSize, MemoryOrder::RELEASE);

                    m_numEnqueued.Decrement(1, MemoryOrder::RELEASE);

                    return true;
                }
            }
            else if (diff < 0)
            {
                return false; // queue empty
            }
            else
            {
                pos = m_head.Get(MemoryOrder::RELAXED);
            }
        }
    }

    virtual void WakeUpOwnerThread() override
    {
        m_wakeEpoch.Increment(1, MemoryOrder::RELEASE);
        m_wakeEpoch.NotifyAll();
    }

    void WaitForTasks(bool* outStopRequested)
    {
        HYP_CORE_ASSERT(IsOnThread(m_ownerThread));

        static constexpr uint32 spinCount = 4000;

        for (uint32 i = 0; i < spinCount; i++)
        {
            if (NumEnqueued() > 0 || m_stopRequested.Load())
            {
                if (outStopRequested)
                {
                    *outStopRequested = m_stopRequested.Load();
                }

                return;
            }

            HYP_WAIT_IDLE();
        }

        uint32 lastEpoch = m_wakeEpoch.Get(MemoryOrder::ACQUIRE);

        while (NumEnqueued() == 0 && !m_stopRequested.Load())
        {
            m_wakeEpoch.Wait(lastEpoch, MemoryOrder::ACQUIRE);

            lastEpoch = m_wakeEpoch.Get(MemoryOrder::ACQUIRE);
        }

        if (outStopRequested)
        {
            *outStopRequested = m_stopRequested.Load();
        }
    }

    template <class Lambda>
    void Flush(Lambda&& lambda)
    {
        HYP_CORE_ASSERT(IsOnThread(m_ownerThread));

        ScheduledTask task;

        while (TryPop(task))
        {
            task.ExecuteWithLambda(lambda);
        }

        WakeUpOwnerThread();
    }

    template <class Container>
    void AcceptAll(Container& outContainer)
    {
        HYP_CORE_ASSERT(IsOnThread(m_ownerThread));

        ScheduledTask task;

        while (TryPop(task))
        {
            outContainer.Add(std::move(task));
        }

        WakeUpOwnerThread();
    }

private:
    struct RingSlot
    {
        ScheduledTask task;
        AtomicVar<uint64> seq { 0 };
    };

    bool TryEnqueue_LockFree(ScheduledTask&& task)
    {
        uint64 pos = m_tail.Get(MemoryOrder::RELAXED);

        for (;;)
        {
            RingSlot& slot = m_ringBuffer[pos & (RingBufferSize - 1)];
            const uint64 seq = slot.seq.Get(MemoryOrder::ACQUIRE);
            const int64 diff = int64(seq) - int64(pos);

            if (diff == 0)
            {
                if (m_tail.CompareExchangeWeak(pos, pos + 1, MemoryOrder::RELAXED))
                {
                    slot.task = std::move(task);
                    slot.seq.Set(pos + 1, MemoryOrder::RELEASE);

                    return true;
                }
            }
            else if (diff < 0)
            {
                return false; // queue full
            }
            else
            {
                pos = m_tail.Get(MemoryOrder::RELAXED);
            }
        }
    }

    TaskID Enqueue_Internal(ScheduledTask&& scheduledTask)
    {
        const TaskID taskId { m_fastIdCounter.Increment(1, MemoryOrder::RELAXED) + 1 };

        scheduledTask.executor->SetTaskID(taskId);
        scheduledTask.executor->SetInitiatorThreadId(CurrentThreadId());
        scheduledTask.executor->SetAssignedScheduler(this);

        while (!TryEnqueue_LockFree(std::move(scheduledTask)))
        {
            HYP_WAIT_IDLE();
        }

        m_numEnqueued.Increment(1, MemoryOrder::RELEASE);

        WakeUpOwnerThread();

        return taskId;
    }

    bool TryStealFrom(ScheduledTask& outTask)
    {
        return TryPop(outTask);
    }

    RingSlot m_ringBuffer[RingBufferSize];

    template <class T, size_t Padding = 64>
    struct Padded : T
    {
        static constexpr size_t NumPaddingBytes = (Padding > sizeof(T) ? Padding - sizeof(T) : 1);

        uint8 padding[NumPaddingBytes];
    };

    // padded out to prevent false sharing.
    Padded<AtomicVar<uint64>> m_head { 0 }; // dequeue position (consumer / owner thread)
    Padded<AtomicVar<uint64>> m_tail { 0 }; // enqueue position (producers)
    Padded<AtomicVar<uint32>> m_fastIdCounter { 0 };
    Padded<AtomicVar<uint32>> m_wakeEpoch { 0 };
};

} // namespace threading

using threading::FastScheduler;

} // namespace Hyperion

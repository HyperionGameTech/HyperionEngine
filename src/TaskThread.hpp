#ifndef HYPERION_V2_TASK_THREAD_H
#define HYPERION_V2_TASK_THREAD_H

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>
#include <core/lib/Queue.hpp>
#include <math/MathUtil.hpp>
#include <GameCounter.hpp>
#include <util/Defines.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

struct ThreadID;

class TaskThread : public Thread<Scheduler<Task<void>>>
{
public:
    TaskThread(const ThreadID &thread_id)
        : Thread(thread_id),
          m_is_running(false),
          m_is_free(false)
    {
    }

    virtual ~TaskThread() = default;

    HYP_FORCE_INLINE bool IsRunning() const
        { return m_is_running.Get(MemoryOrder::RELAXED); }

    HYP_FORCE_INLINE void Stop()
        { m_is_running.Set(false, MemoryOrder::RELAXED); }

    HYP_FORCE_INLINE bool IsFree() const
        { return m_is_free.Get(MemoryOrder::RELAXED); }

protected:
    virtual void operator()() override
    {
        m_is_running.Set(true, MemoryOrder::RELAXED);
        m_is_free.Set(true, MemoryOrder::RELAXED);

        while (!IsRunning()) {
            HYP_WAIT_IDLE();
        }

        while (IsRunning()) {
            m_scheduler.WaitForTasks(m_task_queue);

            const bool was_free = m_task_queue.Empty();
            m_is_free.Set(was_free, MemoryOrder::RELAXED);
            
            // do not execute within lock
            while (m_task_queue.Any()) {
                m_task_queue.Pop().Execute();
            }
            
            if (!was_free) {
                m_is_free.Set(true, MemoryOrder::RELAXED);
            }
        }
    }

    AtomicVar<Bool> m_is_running;
    AtomicVar<Bool> m_is_free;

    Queue<Scheduler::ScheduledTask> m_task_queue;
};

} // namespace hyperion::v2

#endif
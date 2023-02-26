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
        { return m_is_running.load(std::memory_order_relaxed); }

    HYP_FORCE_INLINE void Stop()
        { m_is_running.store(false, std::memory_order_relaxed); }

    HYP_FORCE_INLINE bool IsFree() const
        { return m_is_free.load(); }

protected:
    virtual void operator()() override
    {
        m_is_running.store(true);
        m_is_free.store(true);

        while (!IsRunning()) {
            HYP_WAIT_IDLE();
        }

        while (IsRunning()) {
            m_scheduler.WaitForTasks(m_task_queue);

            const bool was_free = m_task_queue.Empty();
            m_is_free.store(was_free);
            
            // do not execute within lock
            while (m_task_queue.Any()) {
                m_task_queue.Pop().Execute();
            }

            const bool is_free = m_task_queue.Empty();
            
            if (is_free != was_free) {
                m_is_free.store(true);
            }
        }
    }

    std::atomic_bool m_is_running;
    std::atomic_bool m_is_free;

    Queue<Scheduler::ScheduledTask> m_task_queue;
};

} // namespace hyperion::v2

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_THREAD_HPP
#define HYPERION_THREAD_HPP

#include <core/threading/ThreadID.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/StringView.hpp>

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

#include <thread>
#include <type_traits>

namespace hyperion {
namespace threading {

class SchedulerBase;
class Scheduler;
class ThreadLocalStorage;

enum class ThreadPriorityValue
{
    LOWEST,
    LOW,
    NORMAL,
    HIGH,
    HIGHEST
};

class HYP_API ThreadBase
{
public:
    virtual ~ThreadBase();

    /*! \brief Get the ID of this thread. This ID is unique to this thread and is used to identify it. */
    HYP_FORCE_INLINE const ThreadID& GetID() const
    {
        return m_id;
    }

    /*! \brief Get the thread-local storage for this thread. This is used to store thread-local data that is unique to this thread. */
    HYP_FORCE_INLINE ThreadLocalStorage* GetTLS() const
    {
        return m_tls;
    }

    /*! \brief Get the priority of this thread. */
    HYP_FORCE_INLINE ThreadPriorityValue GetPriority() const
    {
        return m_priority;
    }

    /*! \brief Get the scheduler that this thread is associated with. */
    virtual Scheduler& GetScheduler() = 0;

protected:
    ThreadBase(const ThreadID& id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    const ThreadID m_id;
    ThreadPriorityValue m_priority;
    ThreadLocalStorage* m_tls;
};

extern HYP_API void SetCurrentThreadObject(ThreadBase*);
extern HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority);

template <class Scheduler, class... Args>
class Thread : public ThreadBase
{
public:
    Thread(const ThreadID& id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    Thread(const Thread& other) = delete;
    Thread& operator=(const Thread& other) = delete;
    Thread(Thread&& other) noexcept = delete;
    Thread& operator=(Thread&& other) noexcept = delete;
    virtual ~Thread() override;

    virtual Scheduler& GetScheduler() override final
    {
        return m_scheduler;
    }

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_is_running.Get(MemoryOrder::RELAXED);
    }

    /*! \brief Start the thread with the given arguments and run the thread function with them */
    bool Start(Args... args);

    /*! \brief Request the thread to stop running. This does not immediately stop the thread, but sets a flag that the thread should stop.
     *  The thread should check this flag periodically and exit when it is set. */
    virtual void Stop();

    /*! \brief Detach the thread from the current thread and let it run in the background until it finishes execution */
    void Detach();

    /*!\brief Join the thread and wait for it to finish execution before continuing execution of the current thread */
    bool Join();

    /*! \brief Check if the thread can be joined (i.e. it is not detached) and is joinable (i.e. it is not already joined) */
    bool CanJoin() const;

protected:
    virtual void operator()(Args... args) = 0;

    Scheduler m_scheduler;

    AtomicVar<bool> m_stop_requested;

private:
    std::thread* m_thread;
    AtomicVar<bool> m_is_running;
};

template <class Scheduler, class... Args>
Thread<Scheduler, Args...>::Thread(const ThreadID& id, ThreadPriorityValue priority)
    : ThreadBase(id, priority),
      m_is_running(false),
      m_stop_requested(false),
      m_thread(nullptr)
{
}

template <class Scheduler, class... Args>
Thread<Scheduler, Args...>::~Thread()
{
    if (m_thread != nullptr)
    {
        if (m_thread->joinable())
        {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;
    }
}

template <class Scheduler, class... Args>
bool Thread<Scheduler, Args...>::Start(Args... args)
{
    if (m_thread != nullptr)
    {
        return false;
    }

    AssertThrowMsg(!m_is_running.Get(MemoryOrder::RELAXED), "Thread is already running");

    m_is_running.Set(true, MemoryOrder::RELAXED);

    m_thread = new std::thread([this, tuple_args = MakeTuple(args...)](...) -> void
        {
            SetCurrentThreadObject(this);

            m_scheduler.SetOwnerThread(GetID());

            (*this)((tuple_args.template GetElement<Args>())...);

            m_is_running.Set(false, MemoryOrder::RELAXED);
        });

    return true;
}

template <class Scheduler, class... Args>
void Thread<Scheduler, Args...>::Stop()
{
    m_stop_requested.Set(true, MemoryOrder::RELAXED);

    m_scheduler.RequestStop();
}

template <class Scheduler, class... Args>
void Thread<Scheduler, Args...>::Detach()
{
    if (m_thread == nullptr)
    {
        return;
    }

    m_thread->detach();
}

template <class Scheduler, class... Args>
bool Thread<Scheduler, Args...>::Join()
{
    if (!CanJoin())
    {
        return false;
    }

    m_thread->join();

    return true;
}

template <class Scheduler, class... Args>
bool Thread<Scheduler, Args...>::CanJoin() const
{
    if (m_thread == nullptr)
    {
        return false;
    }

    return m_thread->joinable();
}
} // namespace threading

using threading::Thread;
using threading::ThreadBase;
using threading::ThreadPriorityValue;

} // namespace hyperion

#endif
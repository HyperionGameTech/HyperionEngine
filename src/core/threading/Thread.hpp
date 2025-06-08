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

enum class ThreadPriorityValue
{
    LOWEST,
    LOW,
    NORMAL,
    HIGH,
    HIGHEST
};

class IThread
{
public:
    virtual ~IThread() = default;

    /*! \brief Get the ID of this thread. This ID is unique to this thread and is used to identify it. */
    virtual const ThreadID& GetID() const = 0;

    /*! \brief Get the scheduler that this thread is associated with. */
    virtual Scheduler& GetScheduler() = 0;

    /*! \brief Get the priority of this thread. */
    virtual ThreadPriorityValue GetPriority() const = 0;
};

extern HYP_API void RegisterThread(const ThreadID& id, IThread* thread);
extern HYP_API void UnregisterThread(const ThreadID& id);

extern HYP_API void SetCurrentThreadObject(IThread*);
extern HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority);

template <class Scheduler, class... Args>
class Thread : public IThread
{
public:
    Thread(const ThreadID& id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    Thread(const Thread& other) = delete;
    Thread& operator=(const Thread& other) = delete;
    Thread(Thread&& other) noexcept = delete;
    Thread& operator=(Thread&& other) noexcept = delete;
    virtual ~Thread() override;

    virtual const ThreadID& GetID() const override final
    {
        return m_id;
    }

    virtual ThreadPriorityValue GetPriority() const override final
    {
        return m_priority;
    }

    virtual Scheduler& GetScheduler() override final
    {
        return m_scheduler;
    }

    /*! \brief Start the thread with the given arguments and run the thread function with them */
    bool Start(Args... args);

    /*! \brief Detach the thread from the current thread and let it run in the background until it finishes execution */
    void Detach();

    /*!\brief Join the thread and wait for it to finish execution before continuing execution of the current thread */
    bool Join();

    /*! \brief Check if the thread can be joined (i.e. it is not detached) and is joinable (i.e. it is not already joined) */
    bool CanJoin() const;

protected:
    virtual void operator()(Args... args) = 0;

    const ThreadID m_id;

    ThreadPriorityValue m_priority;

    Scheduler m_scheduler;

private:
    std::thread* m_thread;
};

template <class Scheduler, class... Args>
Thread<Scheduler, Args...>::Thread(const ThreadID& id, ThreadPriorityValue priority)
    : m_id(id),
      m_priority(priority),
      m_thread(nullptr)
{
    RegisterThread(m_id, this);
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

    UnregisterThread(m_id);
}

template <class Scheduler, class... Args>
bool Thread<Scheduler, Args...>::Start(Args... args)
{
    if (m_thread != nullptr)
    {
        return false;
    }

    m_thread = new std::thread([this, tuple_args = MakeTuple(args...)](...) -> void
        {
            SetCurrentThreadObject(this);

            m_scheduler.SetOwnerThread(GetID());

            (*this)((tuple_args.template GetElement<Args>())...);
        });

    return true;
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

using threading::IThread;
using threading::Thread;
using threading::ThreadPriorityValue;

} // namespace hyperion

#endif
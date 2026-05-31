/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Threading/Util/ThreadId.hpp>

#include <Core/Threading/AtomicFlag.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Utilities/Tuple.hpp>
#include <Core/Utilities/StringView.hpp>

#include <Core/Memory/Pimpl.hpp>

#include <thread>
#include <type_traits>

namespace Hyperion {

namespace functional {

template <class FunctionSignature>
class Proc;

} // namespace functional

using functional::Proc;

namespace threading {

class SchedulerBase;
class Scheduler;
class ThreadLocalStorage;

enum class ThreadPriorityValue : uint32
{
    LOWEST,
    LOW,
    NORMAL,
    HIGH,
    HIGHEST
};

class CORE_API ThreadBase
{
public:
    virtual ~ThreadBase();

    /*! \brief Get the Id of this thread. This Id is unique to this thread and is used to identify it. */
    HYP_FORCE_INLINE const ThreadId& Id() const
    {
        return m_id;
    }

    /*! \brief Get the thread-local storage for this thread. This is used to store thread-local data that is unique to this thread.
     *  Must only be called from THIS thread */
    ThreadLocalStorage& GetTLS() const;

    /*! \brief Get the priority of this thread. */
    HYP_FORCE_INLINE ThreadPriorityValue GetPriority() const
    {
        return m_priority;
    }

    /*! \brief Get the scheduler that this thread is associated with. */
    virtual Scheduler& GetScheduler() = 0;

    /*! \brief Check if the thread is currently running. */
    virtual bool IsRunning() const = 0;

    /*! \brief Request the thread to stop running. This does not immediately stop the thread, but sets a flag that the thread should stop.
     *  The thread should check this flag periodically and exit when it is set. */
    virtual void Stop() = 0;

    /*! \brief Detach the thread from the current thread and let it run in the background until it finishes execution */
    virtual void Detach() = 0;

    /*! \brief Join the thread and wait for it to finish execution before continuing execution of the current thread */
    virtual bool Join() = 0;

    /*! \brief Check if the thread can be joined (i.e. it is not detached) and is joinable (i.e. it is not already joined) */
    virtual bool CanJoin() const = 0;

    void AddOnExitCallback(void (*callback)(void));

protected:
    ThreadBase(const ThreadId& id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    void OnExit();

    const ThreadId m_id;

    ThreadPriorityValue m_priority;
    mutable ThreadLocalStorage* m_tls;

    mutable Mutex m_onExitMutex;
    Array<void (*)(void), DynamicAllocator> m_onExitCallbacks;
};

CORE_API extern void SetCurrentThreadObject(ThreadBase*);
CORE_API extern void SetCurrentThreadPriority(ThreadPriorityValue priority);

template <class TScheduler, class... TArgs>
class Thread : public ThreadBase
{
public:
    explicit Thread(const ThreadId& id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    Thread(const Thread& other) = delete;
    Thread& operator=(const Thread& other) = delete;

    Thread(Thread&& other) noexcept = delete;
    Thread& operator=(Thread&& other) noexcept = delete;

    virtual ~Thread() override;

    virtual TScheduler& GetScheduler() override final
    {
        return *m_scheduler;
    }

    bool IsRunning() const override
    {
        return m_isRunning.Load();
    }

    bool IsStopping() const
    {
        return m_stopRequested.Load();
    }

    /*! \brief Start the thread with the given arguments and run the thread function with them */
    bool Start(TArgs... args);

    /*! \brief Request the thread to stop running. This does not immediately stop the thread, but sets a flag that the thread should stop.
     *  The thread should check this flag periodically and exit when it is set. */
    virtual void Stop() override;

    /*! \brief Detach the thread from the current thread and let it run in the background until it finishes execution */
    void Detach() override;

    /*!\brief Join the thread and wait for it to finish execution before continuing execution of the current thread */
    bool Join() override;

    /*! \brief Check if the thread can be joined (i.e. it is not detached) and is joinable (i.e. it is not already joined) */
    bool CanJoin() const override;

protected:
    virtual void operator()(TArgs... args) = 0;

    Pimpl<TScheduler> m_scheduler;

    AtomicFlag m_stopRequested;
    AtomicFlag m_isRunning;

    std::thread* m_thread;
};

template <class TScheduler, class... TArgs>
Thread<TScheduler, TArgs...>::Thread(const ThreadId& id, ThreadPriorityValue priority)
    : ThreadBase(id, priority),
      m_thread(nullptr),
      m_scheduler(MakePimpl<TScheduler>())
{
    m_scheduler->SetOwnerThread(m_id);
}

template <class TScheduler, class... TArgs>
Thread<TScheduler, TArgs...>::~Thread()
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

template <class TScheduler, class... TArgs>
bool Thread<TScheduler, TArgs...>::Start(TArgs... args)
{
    if (m_thread != nullptr)
    {
        return false;
    }

    HYP_CORE_ASSERT(!m_isRunning.Load(), "Thread is already running");

    m_isRunning.Store(true);

    m_thread = new std::thread([this, tupleArgs = MakeTuple(args...)](...) -> void
        {
            SetCurrentThreadObject(this);

            (*this)((tupleArgs.template GetElement<TArgs>())...);

            m_isRunning.Store(false);

            OnExit();
        });

    return true;
}

template <class TScheduler, class... TArgs>
void Thread<TScheduler, TArgs...>::Stop()
{
    m_stopRequested.Store(true);

    m_scheduler->RequestStop();
}

template <class TScheduler, class... TArgs>
void Thread<TScheduler, TArgs...>::Detach()
{
    if (m_thread == nullptr)
    {
        return;
    }

    m_thread->detach();
}

template <class TScheduler, class... TArgs>
bool Thread<TScheduler, TArgs...>::Join()
{
    if (!CanJoin())
    {
        return false;
    }

    m_thread->join();

    return true;
}

template <class TScheduler, class... TArgs>
bool Thread<TScheduler, TArgs...>::CanJoin() const
{
    if (m_thread == nullptr)
    {
        return false;
    }

    return m_thread->joinable();
}

#ifndef HYP_BUILD_CORE
#ifdef HYP_MSVC
extern template class HYP_IMPORT Thread<Scheduler>;
#else
extern template class Thread<Scheduler>;
#endif
#endif

} // namespace threading

using threading::Thread;
using threading::ThreadBase;
using threading::ThreadPriorityValue;

} // namespace Hyperion

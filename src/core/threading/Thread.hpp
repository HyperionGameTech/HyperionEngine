/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_CORE_THREAD_HPP
#define HYPERION_CORE_THREAD_HPP

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

using ThreadMask = uint32;

enum class ThreadPriorityValue
{
    LOWEST,
    LOW,
    NORMAL,
    HIGH,
    HIGHEST
};

struct ThreadID
{
    static const ThreadID invalid;

    HYP_API static ThreadID Current();
    HYP_API static ThreadID CreateDynamicThreadID(Name name);
    HYP_API static ThreadID Invalid();

    uint32  value;
    Name    name;

    HYP_FORCE_INLINE bool operator==(const ThreadID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const ThreadID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE bool operator<(const ThreadID &other) const
        { return value < other.value; }

    /*! \brief Get the inverted value of this thread ID, for use as a mask.
     * This is useful for selecting all threads except the one with this ID.
     * \warning This is not valid for dynamic thread IDs. */
    HYP_FORCE_INLINE uint32 operator~() const
        { return ~value; }

    HYP_FORCE_INLINE operator uint32() const
        { return value; }
    
    /*! \brief Check if this thread ID is a dynamic thread ID.
     *  \returns True if this is a dynamic thread ID, false otherwise. */
    HYP_API bool IsDynamic() const;

    /*! \brief Get the mask for this thread ID. For static thread IDs, this is the same as the value.
     *  For dynamic thread IDs, this is the THREAD_DYNAMIC mask.
     *  \returns The relevant mask for this thread ID. */ 
    HYP_API ThreadMask GetMask() const;

    HYP_API bool IsValid() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(value);
    }
};

static_assert(std::is_trivially_destructible_v<ThreadID>,
    "ThreadID must be trivially destructible! Otherwise thread_local current_thread_id var may  be generated using a wrapper function.");

class ThreadBase
{
public:
    virtual ~ThreadBase() = default;

    /*! \brief Get the ID of this thread. This ID is unique to this thread and is used to identify it. */
    virtual const ThreadID &GetID() const = 0;

    /*! \brief Get the scheduler that this thread is associated with. */
    virtual SchedulerBase *GetScheduler() = 0;

    /*! \brief Get the priority of this thread. */
    virtual ThreadPriorityValue GetPriority() const = 0;
};

extern HYP_API void SetCurrentThreadObject(ThreadBase *);
extern HYP_API void SetCurrentThreadID(const ThreadID &thread_id);
extern HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority);

template <class SchedulerType, class ...Args>
class Thread : public ThreadBase
{
public:
    using Scheduler = SchedulerType;

    // Dynamic thread
    Thread(Name dynamic_thread_name, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    Thread(const ThreadID &id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    Thread(const Thread &other)                 = delete;
    Thread &operator=(const Thread &other)      = delete;
    Thread(Thread &&other) noexcept             = delete;
    Thread &operator=(Thread &&other) noexcept  = delete;
    virtual ~Thread() override;

    virtual const ThreadID &GetID() const override
        { return m_id; }

    virtual ThreadPriorityValue GetPriority() const override
        { return m_priority; }

    virtual SchedulerBase *GetScheduler() override
        { return &m_scheduler; }

    HYP_FORCE_INLINE Scheduler *GetSchedulerInstance()
        { return &m_scheduler; }

    /*! \brief Start the thread with the given arguments and run the thread function with them */
    bool Start(Args... args);

    /*! \brief Detach the thread from the current thread and let it run in the background until it finishes execution */
    void Detach();

    /*!\brief Join the thread and wait for it to finish execution before continuing execution of the current thread */
    bool Join();

    /*! \brief Check if the thread can be joined (i.e. it is not detached) and is joinable (i.e. it is not already joined) */
    bool CanJoin() const;

protected:
    virtual void operator()(Args ...args) = 0;

    const ThreadID              m_id;
    const ThreadPriorityValue   m_priority;

    Scheduler                   m_scheduler;

private:
    std::thread                 *m_thread;
};

template <class SchedulerType, class ...Args>
Thread<SchedulerType, Args...>::Thread(Name dynamic_thread_name, ThreadPriorityValue priority)
    : m_id(ThreadID::CreateDynamicThreadID(dynamic_thread_name)),
      m_priority(priority),
      m_thread(nullptr)
{
}

template <class SchedulerType, class ...Args>
Thread<SchedulerType, Args...>::Thread(const ThreadID &id, ThreadPriorityValue priority)
    : m_id(id),
      m_priority(priority),
      m_thread(nullptr)
{
}

template <class SchedulerType, class ...Args>
Thread<SchedulerType, Args...>::~Thread()
{
    if (m_thread != nullptr) {
        if (m_thread->joinable()) {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;
    }
}

template <class SchedulerType, class... Args>
bool Thread<SchedulerType, Args...>::Start(Args ... args)
{
    if (m_thread != nullptr) {
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

template <class SchedulerType, class ...Args>
void Thread<SchedulerType, Args...>::Detach()
{
    if (m_thread == nullptr) {
        return;
    }

    m_thread->detach();
}

template <class SchedulerType, class ...Args>
bool Thread<SchedulerType, Args...>::Join()
{
    if (!CanJoin()) {
        return false;
    }
    
    m_thread->join();

    return true;
}

template <class SchedulerType, class ...Args>
bool Thread<SchedulerType, Args...>::CanJoin() const
{
    if (m_thread == nullptr) {
        return false;
    }

    return m_thread->joinable();
}
} // namespace threading

using threading::Thread;
using threading::ThreadID;
using threading::ThreadPriorityValue;

} // namespace hyperion

#endif
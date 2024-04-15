/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_CORE_THREAD_H
#define HYPERION_V2_CORE_THREAD_H

#include <core/lib/StringView.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/Name.hpp>
#include <core/Task.hpp>
#include <core/Defines.hpp>
#include <Types.hpp>

#include <thread>
#include <type_traits>

namespace hyperion {

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
    HYP_API static const ThreadID invalid;

    HYP_API static ThreadID Current();
    HYP_API static ThreadID CreateDynamicThreadID(Name name);

    uint32  value;
    Name    name;

    HYP_FORCE_INLINE
    bool operator==(const ThreadID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE
    bool operator!=(const ThreadID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE
    bool operator<(const ThreadID &other) const
        { return value < other.value; }

    // Not valid for DYNAMIC thread IDs.
    HYP_FORCE_INLINE
    uint operator~() const
        { return ~value; }

    HYP_FORCE_INLINE
    operator uint32() const
        { return value; }
    
    HYP_API bool IsDynamic() const;
};

static_assert(std::is_trivially_destructible_v<ThreadID>,
    "ThreadID must be trivially destructible! Otherwise thread_local current_thread_id var may  be generated using a wrapper function.");

extern HYP_API void SetCurrentThreadID(const ThreadID &thread_id);
extern HYP_API void SetCurrentThreadPriority(ThreadPriorityValue priority);

template <class SchedulerType, class ...Args>
class Thread
{
public:
    using Scheduler = SchedulerType;
    using Task      = typename Scheduler::Task;

    // Dynamic thread
    Thread(Name dynamic_thread_name, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    Thread(const ThreadID &id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    Thread(const Thread &other)                 = delete;
    Thread &operator=(const Thread &other)      = delete;
    Thread(Thread &&other) noexcept             = delete;
    Thread &operator=(Thread &&other) noexcept  = delete;
    virtual ~Thread();

    /**
     * \brief Get the ID of this thread. This ID is unique to this thread and is used to identify it. 
     */
    const ThreadID &GetID() const
        { return m_id; }

    /**
     * \brief Get the priority of this thread.
     */
    ThreadPriorityValue GetPriority() const
        { return m_priority; }

    Scheduler &GetScheduler()
        { return m_scheduler; }

    const Scheduler &GetScheduler() const
        { return m_scheduler; }

    /*! \brief Enqueue a task to be executed on this thread
     * @param task The task to be executed
     * @param atomic_counter An optionally provided pointer to atomic uint which will be incremented
     *      upon completion
     */
    TaskID ScheduleTask(Task &&task, AtomicVar<uint> *atomic_counter = nullptr)
    {
        return m_scheduler.Enqueue(std::forward<Task>(task), atomic_counter);
    }

    /**
     * \brief Start the thread with the given arguments and run the thread function with them
     */
    bool Start(Args ...args);

    /**
     * \brief Detach the thread from the current thread and let it run in the background until it finishes execution
     */
    bool Detach();

    /**
     * \brief Join the thread and wait for it to finish execution before continuing execution of the current thread
     */
    bool Join();

    /**
     * \brief Check if the thread can be joined (i.e. it is not detached) and is joinable (i.e. it is not already joined)
     */
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

template <class SchedulerType, class ...Args>
bool Thread<SchedulerType, Args...>::Start(Args ...args)
{
    if (m_thread != nullptr) {
        return false;
    }

    std::tuple<Args...> tuple_args(std::forward<Args>(args)...);

    m_thread = new std::thread([&self = *this, tuple_args](...) -> void
    {
        SetCurrentThreadID(self.GetID());
        SetCurrentThreadPriority(self.GetPriority());

        self.m_scheduler.SetOwnerThread(self.GetID());

        self(std::get<Args>(tuple_args)...);
    });

    return true;
}

template <class SchedulerType, class ...Args>
bool Thread<SchedulerType, Args...>::Detach()
{
    if (m_thread == nullptr) {
        return false;
    }

    try {
        m_thread->detach();
    } catch (...) {
        return false;
    }

    return true;
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

} // namespace hyperion

#endif
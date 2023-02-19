#ifndef HYPERION_V2_CORE_THREAD_H
#define HYPERION_V2_CORE_THREAD_H

#include <core/lib/FixedString.hpp>
#include <core/lib/AtomicVar.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <thread>
#include <type_traits>
#include <tuple>

namespace hyperion::v2 {

struct ThreadID
{
    static const ThreadID invalid;

    UInt value;
    StringView name;

    HYP_FORCE_INLINE bool operator==(const ThreadID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const ThreadID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE bool operator<(const ThreadID &other) const
        { return std::tie(value, name) < std::tie(other.value, other.name); }

    HYP_FORCE_INLINE UInt operator~() const
        { return ~value; }
};

static_assert(std::is_trivially_destructible_v<ThreadID>,
    "ThreadID must be trivially destructible! Otherwise thread_local current_thread_id var may  be generated using a wrapper function.");

void SetCurrentThreadID(const ThreadID &thread_id);

template <class SchedulerType, class ...Args>
class Thread
{
public:
    using Scheduler = SchedulerType;
    using Task = typename Scheduler::Task;
    using TaskID = typename Scheduler::TaskID;

    Thread(const ThreadID &id);
    Thread(const Thread &other) = delete;
    Thread &operator=(const Thread &other) = delete;
    Thread(Thread &&other) noexcept;
    Thread &operator=(Thread &&other) noexcept;
    virtual ~Thread();

    const ThreadID &GetID() const { return m_id; }

    Scheduler &GetScheduler() { return m_scheduler; }
    const Scheduler &GetScheduler() const { return m_scheduler; }

    /*! \brief Enqueue a task to be executed on this thread
     * @param task The task to be executed
     * @param atomic_counter An optionally provided pointer to atomic UInt which will be incremented
     *      upon completion
     */
    TaskID ScheduleTask(Task &&task, AtomicVar<UInt> *atomic_counter = nullptr)
    {
        return m_scheduler.Enqueue(std::forward<Task>(task), atomic_counter);
    }

    bool Start(Args ...args);
    bool Detach();
    bool Join();
    bool CanJoin() const;

protected:
    virtual void operator()(Args ...args) = 0;

    const ThreadID m_id;
    Scheduler m_scheduler;

private:
    std::thread *m_thread;
};

template <class SchedulerType, class ...Args>
Thread<SchedulerType, Args...>::Thread(const ThreadID &id)
    : m_id(id),
      m_thread(nullptr)
{
}

template <class SchedulerType, class ...Args>
Thread<SchedulerType, Args...>::Thread(Thread &&other) noexcept
    : m_id(std::move(other.m_id)),
      m_thread(other.m_thread),
      m_scheduler(std::move(other.m_scheduler))
{
    other.m_thread = nullptr;
}

template <class SchedulerType, class ...Args>
Thread<SchedulerType, Args...> &Thread<SchedulerType, Args...>::operator=(Thread &&other) noexcept
{
    m_id = std::move(other.m_id);
    m_thread = other.m_thread;
    m_scheduler = std::move(other.m_scheduler);

    other.m_thread = nullptr;

    return *this;
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

    m_thread = new std::thread([&self = *this, tuple_args] {
        SetCurrentThreadID(self.GetID());
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

} // namespace hyperion::v2

#endif
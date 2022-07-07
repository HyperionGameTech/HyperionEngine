#ifndef HYPERION_V2_CORE_THREAD_H
#define HYPERION_V2_CORE_THREAD_H

#include "lib/FixedString.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>

#include <thread>
#include <tuple>

namespace hyperion::v2 {

struct ThreadId {
    UInt        value;
    FixedString name;

    bool operator==(const ThreadId &other) const { return value == other.value; }
    bool operator!=(const ThreadId &other) const { return value != other.value; }
    bool operator<(const ThreadId &other) const { return std::tie(value, name) < std::tie(other.value, other.name); }
};

#if HYP_ENABLE_THREAD_ASSERTION
extern thread_local ThreadId current_thread_id;
#endif

void SetThreadId(const ThreadId &id);

template <class Scheduler, class ...Args>
class Thread {
public:
    Thread(const ThreadId &id);
    Thread(const Thread &other) = delete;
    Thread &operator=(const Thread &other) = delete;
    Thread(Thread &&other) noexcept;
    Thread &operator=(Thread &&other) noexcept;
    virtual ~Thread();

    const ThreadId &GetId() const         { return m_id; }

    Scheduler *GetScheduler()             { return m_scheduler; }
    const Scheduler *GetScheduler() const { return m_scheduler; }

    template <class Task>
    void ScheduleTask(Task &&task)
    {
        AssertThrow(m_scheduler != nullptr);
        m_scheduler->Enqueue(std::forward<Task>(task));
    }

    bool Start(Args ...args);
    bool Detach();
    bool Join();
    bool CanJoin() const;

protected:
    virtual void operator()(Args ...args) = 0;

    const ThreadId m_id;
    Scheduler     *m_scheduler;

private:
    std::thread   *m_thread;
};

template <class Scheduler, class ...Args>
Thread<Scheduler, Args...>::Thread(const ThreadId &id)
    : m_id(id),
      m_thread(nullptr),
      m_scheduler(nullptr)
{
}

template <class Scheduler, class ...Args>
Thread<Scheduler, Args...>::Thread(Thread &&other) noexcept
    : m_id(std::move(other.m_id)),
      m_thread(other.m_thread),
      m_scheduler(other.m_scheduler)
{
    other.m_thread = nullptr;
    other.m_scheduler = nullptr;
}

template <class Scheduler, class ...Args>
Thread<Scheduler, Args...> &Thread<Scheduler, Args...>::operator=(Thread &&other) noexcept
{
    m_id = std::move(other.m_id);
    m_thread = other.m_thread;
    m_scheduler = other.m_scheduler;

    other.m_thread = nullptr;
    other.m_scheduler = nullptr;

    return *this;
}

template <class Scheduler, class ...Args>
Thread<Scheduler, Args...>::~Thread()
{
    if (m_scheduler != nullptr) {
        delete m_scheduler;
        m_scheduler = nullptr;
    }

    if (m_thread != nullptr) {
        if (m_thread->joinable()) {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;
    }
}

template <class Scheduler, class ...Args>
bool Thread<Scheduler, Args...>::Start(Args ...args)
{
    if (m_thread != nullptr) {
        return false;
    }

    std::tuple<Args...> tuple_args(std::forward<Args>(args)...);

    m_thread = new std::thread([&self = *this, tuple_args] {
        SetThreadId(self.GetId());

        AssertThrow(self.m_scheduler == nullptr);
        self.m_scheduler = new Scheduler();

        self(std::get<Args>(tuple_args)...);
    });

    return true;
}

template <class Scheduler, class ...Args>
bool Thread<Scheduler, Args...>::Detach()
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

template <class Scheduler, class ...Args>
bool Thread<Scheduler, Args...>::Join()
{
    if (m_thread == nullptr) {
        return false;
    }
    
    try {
        m_thread->join();
    } catch (...) {
        return false;
    }

    return true;
}

template <class Scheduler, class ...Args>
bool Thread<Scheduler, Args...>::CanJoin() const
{
    if (m_thread == nullptr) {
        return false;
    }

    return m_thread->joinable();
}

} // namespace hyperion::v2

#endif
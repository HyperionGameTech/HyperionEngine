#ifndef HYPERION_V2_CORE_THREAD_H
#define HYPERION_V2_CORE_THREAD_H

#include "lib/fixed_string.h"
#include <util/defines.h>
#include <types.h>

#include <thread>
#include <tuple>

namespace hyperion::v2 {

struct ThreadId {
    UInt        value;
    FixedString name;

    bool operator==(const ThreadId &other) const { return value == other.value; }
    bool operator!=(const ThreadId &other) const { return value != other.value; }
};

#if HYP_ENABLE_THREAD_ASSERTION
extern thread_local ThreadId current_thread_id;
#endif

void SetThreadId(const ThreadId &id);

template <class ...Args>
class Thread {
public:
    Thread(const ThreadId &id);
    Thread(const Thread &other) = delete;
    Thread &operator=(const Thread &other) = delete;
    Thread(Thread &&other) noexcept;
    Thread &operator=(Thread &&other) noexcept;
    virtual ~Thread();

    const ThreadId &GetId() const { return m_id; }

    bool Start(Args ...args);
    bool Detach();
    bool Join();
    bool CanJoin() const;

protected:
    virtual void operator()(Args ...args) = 0;

    const ThreadId m_id;

private:
    std::thread *m_thread;
};

template <class ...Args>
Thread<Args...>::Thread(const ThreadId &id)
    : m_id(id),
      m_thread(nullptr)
{
}

template <class ...Args>
Thread<Args...>::Thread(Thread &&other) noexcept
    : m_id(std::move(other.m_id)),
      m_thread(other.m_thread)
{
    other.m_thread = nullptr;
}

template <class ...Args>
Thread<Args...> &Thread<Args...>::operator=(Thread &&other) noexcept
{
    m_id = std::move(other.m_id);
    m_thread = other.m_thread;
    other.m_thread = nullptr;

    return *this;
}

template <class ...Args>
Thread<Args...>::~Thread()
{
    if (m_thread != nullptr) {
        if (m_thread->joinable()) {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;
    }
}

template <class ...Args>
bool Thread<Args...>::Start(Args ...args)
{
    if (m_thread != nullptr) {
        return false;
    }

    std::tuple<Args...> tuple_args(std::forward<Args>(args)...);

    m_thread = new std::thread([&self = *this, tuple_args] {
        SetThreadId(self.GetId());

        self(std::get<Args>(tuple_args)...);
    });

    return true;
}

template <class ...Args>
bool Thread<Args...>::Detach()
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

template <class ...Args>
bool Thread<Args...>::Join()
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

template <class ...Args>
bool Thread<Args...>::CanJoin() const
{
    if (m_thread == nullptr) {
        return false;
    }

    return m_thread->joinable();
}

} // namespace hyperion::v2

#endif
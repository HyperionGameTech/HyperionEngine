#pragma once

#include <Core/threading/Mutex.hpp>
#include <Core/threading/ConditionVariable.hpp>

namespace Hyperion {
namespace threading {

class ThreadSignal
{
public:
    explicit ThreadSignal(bool value = 1)
        : m_value(int32(value))
    {
    }

    ThreadSignal(const ThreadSignal&) = delete;
    ThreadSignal& operator=(const ThreadSignal&) = delete;

    ThreadSignal(ThreadSignal&&) noexcept = delete;
    ThreadSignal& operator=(ThreadSignal&&) noexcept = delete;

    ~ThreadSignal() = default;

    bool IsSignalled() const
    {
        return AtomicAdd(&m_value, 0) > 0;
    }

    void Signal()
    {
        Mutex::Guard guard(m_mutex);

        AtomicIncrement(&m_value);

        m_conditionVariable.NotifyAll();
    }

    void Wait()
    {
        while (!IsSignalled())
        {   
            Mutex::Guard guard(m_mutex);
            m_conditionVariable.Wait(m_mutex);
        }
    }

    void WaitAndReset()
    {
        while (!IsSignalled())
        {
            Mutex::Guard guard(m_mutex);
            m_conditionVariable.Wait(m_mutex);

            Reset();
        }
    }

    void Reset()
    {
        Mutex::Guard guard(m_mutex);
        AtomicExchange(&m_value, 0);
    }

private:
    mutable Mutex m_mutex;
    ConditionVariable m_conditionVariable;

    mutable volatile int32 m_value;
};

} // namespace threading

using threading::ThreadSignal;

} // namespace Hyperion

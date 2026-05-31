#pragma once

#include <Core/threading/Mutex.hpp>
#include <Core/threading/ConditionVariable.hpp>

namespace Hyperion {
namespace threading {

class ThreadSignal
{
public:
    explicit ThreadSignal(int32 value = 1)
        : m_value(int32(value))
    {
    }

    ThreadSignal(const ThreadSignal&) = delete;
    ThreadSignal& operator=(const ThreadSignal&) = delete;

    ThreadSignal(ThreadSignal&&) noexcept = delete;
    ThreadSignal& operator=(ThreadSignal&&) noexcept = delete;

    ~ThreadSignal() = default;

    bool IsSignalled(int32 minValue = 1) const
    {
        return AtomicAdd(&m_value, 0) >= minValue;
    }

    void Signal(int32 count = 1)
    {
        Mutex::Guard guard(m_mutex);

        AtomicAdd(&m_value, count);

        m_conditionVariable.NotifyAll();
    }

    void Wait(int32 waitForValue = 1)
    {
        while (AtomicAdd(&m_value, 0) < waitForValue)
        {
            Mutex::Guard guard(m_mutex);
            m_conditionVariable.Wait(m_mutex);
        }
    }

    void WaitAndReset(int32 minValue = 1)
    {
        while (true)
        {
            int32 currentValue = AtomicAdd(&m_value, 0);
            if (currentValue >= minValue)
            {
                if (AtomicCompareExchange(&m_value, currentValue, 0))
                {
                    return;
                }
            }

            Mutex::Guard guard(m_mutex);
            m_conditionVariable.Wait(m_mutex);
        }
    }

    void Reset(int32 newValue = 0)
    {
        Mutex::Guard guard(m_mutex);
        AtomicExchange(&m_value, newValue);
    }

private:
    mutable Mutex m_mutex;
    ConditionVariable m_conditionVariable;

    mutable volatile int32 m_value;
};

} // namespace threading

using threading::ThreadSignal;

} // namespace Hyperion

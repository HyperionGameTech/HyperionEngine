#pragma once

#include <core/threading/Mutex.hpp>
#include <core/threading/ConditionVariable.hpp>

namespace Hyperion {
namespace threading {

class ThreadSignal
{
public:
    ThreadSignal() = default;

    ThreadSignal(const ThreadSignal&) = delete;
    ThreadSignal& operator=(const ThreadSignal&) = delete;

    ThreadSignal(ThreadSignal&&) noexcept = delete;
    ThreadSignal& operator=(ThreadSignal&&) noexcept = delete;

    ~ThreadSignal() = default;

    void Wait()
    {
        Mutex::Guard guard(m_mutex);
        while (m_signalCount <= 0)
        {
            m_conditionVariable.Wait(m_mutex);
        }

        m_signalCount = 0;
    }

    bool IsSignalled() const
    {
        Mutex::Guard guard(m_mutex);
        return m_signalCount > 0;
    }

    void Signal()
    {
        Mutex::Guard guard(m_mutex);
        ++m_signalCount;
        m_conditionVariable.NotifyOne();
    }

private:
    mutable Mutex m_mutex;
    ConditionVariable m_conditionVariable;
    int m_signalCount = 0;
};

} // namespace threading

using threading::ThreadSignal;

} // namespace Hyperion

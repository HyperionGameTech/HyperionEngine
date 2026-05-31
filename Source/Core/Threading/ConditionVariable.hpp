/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/Mutex.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/Types.hpp>

#if defined(HYP_UNIX)
#include <pthread.h>
#include <time.h>
#elif defined(HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace Hyperion {
namespace threading {

class ConditionVariable
{
public:
    ConditionVariable()
    {
#if defined(HYP_UNIX)
        if (pthread_cond_init(&m_conditionVariable, nullptr) != 0)
        {
            HYP_CORE_ASSERT(false, "Failed to create condition variable");
        }
#elif defined(HYP_WINDOWS)
        InitializeConditionVariable(&m_conditionVariable);
#endif
    }

    ConditionVariable(const ConditionVariable& other) = delete;
    ConditionVariable& operator=(const ConditionVariable& other) = delete;
    ConditionVariable(ConditionVariable&& other) noexcept = delete;
    ConditionVariable& operator=(ConditionVariable&& other) noexcept = delete;

#if defined(HYP_UNIX)
    ~ConditionVariable()
    {
        if (pthread_cond_destroy(&m_conditionVariable) != 0)
        {
            HYP_CORE_ASSERT(false, "Failed to destroy condition variable");
        }
    }
#elif defined(HYP_WINDOWS)
    ~ConditionVariable() = default;
#endif

    void Wait(Mutex& mutex) const
    {
#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(mutex.m_locked, "Mutex must be locked before waiting on condition variable");
        mutex.m_locked = false;
#endif

#if defined(HYP_UNIX)
        pthread_cond_wait(&m_conditionVariable, &mutex.m_mutex);
#elif defined(HYP_WINDOWS)
        SleepConditionVariableCS(&m_conditionVariable, &mutex.m_criticalSection, INFINITE);
#endif

#if HYP_DEBUG_MODE
        mutex.m_locked = true;
#endif
    }

    /*! \brief Wait with timeout in milliseconds. Returns true if notified, false if timeout occurred. */
    bool WaitFor(Mutex& mutex, uint32 timeoutMs) const
    {
#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(mutex.m_locked, "Mutex must be locked before waiting on condition variable");
        mutex.m_locked = false;
#endif

        bool result = true;

#if defined(HYP_UNIX)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        ts.tv_sec += timeoutMs / 1000;
        ts.tv_nsec += (timeoutMs % 1000) * 1000000;

        if (ts.tv_nsec >= 1000000000)
        {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }

        int waitResult = pthread_cond_timedwait(&m_conditionVariable, &mutex.m_mutex, &ts);
        result = (waitResult == 0);
#elif defined(HYP_WINDOWS)
        BOOL waitResult = SleepConditionVariableCS(&m_conditionVariable, &mutex.m_criticalSection, timeoutMs);
        result = (waitResult != 0);
#endif

#if HYP_DEBUG_MODE
        mutex.m_locked = true;
#endif

        return result;
    }

    void NotifyOne() const
    {
#if defined(HYP_UNIX)
        pthread_cond_signal(&m_conditionVariable);
#elif defined(HYP_WINDOWS)
        WakeConditionVariable(&m_conditionVariable);
#endif
    }

    void NotifyAll() const
    {
#if defined(HYP_UNIX)
        pthread_cond_broadcast(&m_conditionVariable);
#elif defined(HYP_WINDOWS)
        WakeAllConditionVariable(&m_conditionVariable);
#endif
    }

private:
#if defined(HYP_UNIX)
    mutable pthread_cond_t m_conditionVariable = PTHREAD_COND_INITIALIZER;
#elif defined(HYP_WINDOWS)
    mutable CONDITION_VARIABLE m_conditionVariable;
#endif
};

} // namespace threading

using threading::ConditionVariable;

} // namespace Hyperion

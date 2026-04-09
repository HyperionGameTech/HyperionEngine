/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#if defined(HYP_UNIX)
#include <pthread.h>
#elif defined(HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <Core/threading/LockGuard.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {
namespace threading {

class ConditionVariable;

class Mutex
{
    friend class ConditionVariable;

public:
    using Guard = TLockGuard<Mutex>;

    Mutex()
    {
#if defined(HYP_UNIX)
        if (pthread_mutex_init(&m_mutex, nullptr) != 0)
        {
            HYP_CORE_ASSERT(false, "Failed to create mutex");
        }
#elif defined(HYP_WINDOWS)
        InitializeCriticalSection(&m_criticalSection);
#endif

#if HYP_DEBUG_MODE
        m_locked = false;
        m_destroyed = false;
#endif
    }

    Mutex(const Mutex& other) = delete;
    Mutex& operator=(const Mutex& other) = delete;
    
    Mutex(Mutex&& other) noexcept = delete;
    Mutex& operator=(Mutex&& other) noexcept = delete;

#if defined(HYP_UNIX)
    ~Mutex()
    {
#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(!m_destroyed, "Mutex is already destroyed");
#endif

        if (pthread_mutex_destroy(&m_mutex) != 0)
        {
            HYP_CORE_ASSERT(false, "Failed to destroy mutex");
        }

#if HYP_DEBUG_MODE
        m_destroyed = true;
#endif
    }
#elif defined(HYP_WINDOWS)
    ~Mutex()
    {
#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(!m_destroyed, "Mutex is already destroyed");
#endif

        DeleteCriticalSection(&m_criticalSection);

#if HYP_DEBUG_MODE
        m_destroyed = true;
#endif
    }
#endif

    void Lock()
    {
#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(!m_destroyed, "Mutex is already destroyed");
#endif

#if defined(HYP_UNIX)
        pthread_mutex_lock(&m_mutex);
#elif defined(HYP_WINDOWS)
        EnterCriticalSection(&m_criticalSection);
#endif

#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(!m_locked, "Mutex is already locked");
        m_locked = true;
#endif
    }

    void Unlock()
    {
#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(!m_destroyed, "Mutex is already destroyed");
#endif

#if HYP_DEBUG_MODE
        HYP_CORE_ASSERT(m_locked, "Mutex is not locked");
        m_locked = false;
#endif
#if defined(HYP_UNIX)
        pthread_mutex_unlock(&m_mutex);
#elif defined(HYP_WINDOWS)
        LeaveCriticalSection(&m_criticalSection);
#endif
    }

private:
#if defined(HYP_UNIX)
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
#elif defined(HYP_WINDOWS)
    CRITICAL_SECTION m_criticalSection;
#endif

#if HYP_DEBUG_MODE
    bool m_locked : 1;
    bool m_destroyed : 1;
#endif
};

} // namespace threading

using threading::Mutex;

} // namespace Hyperion

/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#if defined(HYP_UNIX)
#include <pthread.h>
#elif defined(HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace hyperion {
namespace threading {

class Mutex
{
public:
    struct Guard
    {
        Guard(Mutex& mutex)
            : mutex(mutex)
        {
            mutex.Lock();
        }

        Guard(const Guard& other) = delete;
        Guard& operator=(const Guard& other) = delete;
        Guard(Guard&& other) noexcept = delete;
        Guard& operator=(Guard&& other) noexcept = delete;

        ~Guard()
        {
            mutex.Unlock();
        }

        Mutex& mutex;
    };

    Mutex()
    {
#if defined(HYP_UNIX)
        if (pthread_mutex_init(&m_mutex, nullptr) != 0)
        {
            HYP_CORE_ASSERT(false, "Failed to create mutex");
        }
#elif defined(HYP_WINDOWS)
        InitializeCriticalSection(&m_criticalSection);
        m_locked = false;
#endif
    }

    Mutex(const Mutex& other) = delete;
    Mutex& operator=(const Mutex& other) = delete;
    Mutex(Mutex&& other) noexcept = delete;
    Mutex& operator=(Mutex&& other) noexcept = delete;

#if defined(HYP_UNIX)
    ~Mutex()
    {
        if (pthread_mutex_destroy(&m_mutex) != 0)
        {
            HYP_CORE_ASSERT(false, "Failed to destroy mutex");
        }
    }
#elif defined(HYP_WINDOWS)
    ~Mutex()
    {
        DeleteCriticalSection(&m_criticalSection);
    }
#endif

    void Lock()
    {
#if defined(HYP_UNIX)
        pthread_mutex_lock(&m_mutex);
#elif defined(HYP_WINDOWS)
        EnterCriticalSection(&m_criticalSection);
        HYP_CORE_ASSERT(!m_locked, "Mutex is already locked");
        m_locked = true;
#endif
    }

    void Unlock()
    {
#if defined(HYP_UNIX)
        pthread_mutex_unlock(&m_mutex);
#elif defined(HYP_WINDOWS)
        m_locked = false;
        LeaveCriticalSection(&m_criticalSection);
#endif
    }

private:
#if defined(HYP_UNIX)
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
#elif defined(HYP_WINDOWS)
    CRITICAL_SECTION m_criticalSection;
    bool m_locked : 1;
#endif
};

} // namespace threading

using threading::Mutex;

} // namespace hyperion

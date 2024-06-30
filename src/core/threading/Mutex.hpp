/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MUTEX_HPP
#define HYPERION_MUTEX_HPP

#ifdef HYP_UNIX
#include <pthread.h>
#endif

#include <mutex>

namespace hyperion {
namespace threading {

// @TODO: Implement without using STL mutex, so we can avoid ABI breakage

class Mutex
{
public:
    struct Guard
    {
        Guard(Mutex &mutex)
            : mutex(mutex)
        {
            mutex.Lock();
        }

        Guard(const Guard &other)                   = delete;
        Guard &operator=(const Guard &other)        = delete;
        Guard(Guard &&other) noexcept               = delete;
        Guard &operator=(Guard &&other) noexcept    = delete;

        ~Guard()
        {
            mutex.Unlock();
        }

        Mutex &mutex;
    };

    Mutex()                                     = default;
    Mutex(const Mutex &other)                   = delete;
    Mutex &operator=(const Mutex &other)        = delete;
    Mutex(Mutex &&other) noexcept               = delete;
    Mutex &operator=(Mutex &&other) noexcept    = delete;
    ~Mutex()                                    = default;

    void Lock()
    {
#ifdef HYP_UNIX
        pthread_mutex_lock(&m_mutex);
#else
        m_mutex.lock();
#endif
    }

    void Unlock()
    {
#ifdef HYP_UNIX
        pthread_mutex_unlock(&m_mutex);
#else
        m_mutex.unlock();
#endif
    }

private:
#ifdef HYP_UNIX
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
    std::mutex m_mutex;
#endif
};

} // namespace threading

using threading::Mutex;

} // namespace hyperion

#endif
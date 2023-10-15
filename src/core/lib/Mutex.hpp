#ifndef HYPERION_V2_LIB_MUTEX_HPP
#define HYPERION_V2_LIB_MUTEX_HPP

#include <mutex>

namespace hyperion {

class Mutex;

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
    ~Mutex() = default;

    void Lock()
        { m_mutex.lock(); }

    void Unlock()
        { m_mutex.unlock(); }

private:
    std::mutex m_mutex;
};

} // namespace hyperion

#endif
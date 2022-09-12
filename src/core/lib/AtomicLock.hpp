#ifndef HYPERION_V2_LIB_ATOMIC_LOCK_H
#define HYPERION_V2_LIB_ATOMIC_LOCK_H

#include <atomic>

#include "AtomicSemaphore.hpp"

#include <system/Debug.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>

namespace hyperion::v2 {

class AtomicLock {
public:
    AtomicLock() = default;
    AtomicLock(const AtomicLock &other) = delete;
    AtomicLock &operator=(const AtomicLock &other) = delete;
    AtomicLock(AtomicLock &&other) = delete;
    AtomicLock &operator=(AtomicLock &&other) = delete;
    ~AtomicLock() = default;

    HYP_FORCE_INLINE void Lock() { m_semaphore.Wait(); }
    HYP_FORCE_INLINE void Unlock() { m_semaphore.Signal(); }

private:
    BinarySemaphore m_semaphore;
};

class AtomicLocker {
public:
    AtomicLocker(AtomicLock &lock)
        : m_lock(lock)
    {
        m_lock.Lock();
    }

    ~AtomicLocker()
    {
        m_lock.Unlock();
    }

private:
    AtomicLock &m_lock;
};

} // namespace hyperion::v2

#endif
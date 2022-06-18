#ifndef HYPERION_V2_LIB_ATOMIC_LOCK_H
#define HYPERION_V2_LIB_ATOMIC_LOCK_H

#include <atomic>

#include "atomic_semaphore.h"

#include <system/debug.h>
#include <types.h>
#include <util/defines.h>

namespace hyperion::v2 {

class AtomicLock {
public:
    AtomicLock() = default;
    AtomicLock(const AtomicLock &other) = delete;
    AtomicLock &operator=(const AtomicLock &other) = delete;
    AtomicLock(AtomicLock &&other) = delete;
    AtomicLock &operator=(AtomicLock &&other) = delete;
    ~AtomicLock() = default;

    HYP_FORCE_INLINE bool IsLocked() const { return m_semaphore.Count() != 0; }

    HYP_FORCE_INLINE void Lock()           { AssertThrow(!IsLocked()); m_semaphore.Inc(); }
    HYP_FORCE_INLINE void Unlock()         { AssertThrow(IsLocked());  m_semaphore.Dec(); }

    HYP_FORCE_INLINE void Wait() const     { m_semaphore.BlockUntilZero(); }

private:
    AtomicSemaphore<uint> m_semaphore;
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

class AtomicWaiter {
public:
    AtomicWaiter(AtomicLock &lock)
        : m_lock(lock)
    {
        m_lock.Wait();
    }

private:
    AtomicLock &m_lock;
};

} // namespace hyperion::v2

#endif
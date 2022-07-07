#ifndef HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H
#define HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H

#include <core/Memory.hpp>

#include <atomic>

namespace hyperion::v2 {

/* waits for zero to be accessing */
template <class T = int>
class AtomicSemaphore {
public:
    AtomicSemaphore() = default;
    AtomicSemaphore(const AtomicSemaphore &other) = delete;
    AtomicSemaphore &operator=(const AtomicSemaphore &other) = delete;
    AtomicSemaphore(AtomicSemaphore &&other) = delete;
    AtomicSemaphore &operator=(AtomicSemaphore &&other) = delete;
    ~AtomicSemaphore() = default;

    void Inc()      { ++m_count; }
    void Dec()      { --m_count; }
    T Count() const { return m_count.load(); }

    HYP_FORCE_INLINE void BlockUntil(T value) const { HYP_MEMORY_BARRIER_COUNTER(m_count, value); }
    HYP_FORCE_INLINE void BlockUntilZero() const    { BlockUntil(0); }

private:
    std::atomic<T> m_count{0};
};

} // namespace hyperion::v2

#endif

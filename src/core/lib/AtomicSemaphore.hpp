#ifndef HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H
#define HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H

#include <core/Memory.hpp>
#include <Types.hpp>

#include <atomic>

namespace hyperion::v2 {

// counting semaphore
template <class T = int>
class AtomicSemaphore {
public:
    AtomicSemaphore() = default;
    AtomicSemaphore(const AtomicSemaphore &other) = delete;
    AtomicSemaphore &operator=(const AtomicSemaphore &other) = delete;
    AtomicSemaphore(AtomicSemaphore &&other) = delete;
    AtomicSemaphore &operator=(AtomicSemaphore &&other) = delete;
    ~AtomicSemaphore() = default;

    void Inc() { ++m_count; }
    void Dec() { --m_count; }
    T Count() const { return m_count.load(); }

    HYP_FORCE_INLINE void BlockUntil(T value) const { HYP_MEMORY_BARRIER_COUNTER(m_count, value); }
    HYP_FORCE_INLINE void BlockUntilZero() const { BlockUntil(0); }

private:
    std::atomic<T> m_count { 0 };
};

// binary semaphore
class BinarySemaphore {
public:
    BinarySemaphore() = default;
    BinarySemaphore(const BinarySemaphore &other) = delete;
    BinarySemaphore &operator=(const BinarySemaphore &other) = delete;
    BinarySemaphore(BinarySemaphore &&other) = delete;
    BinarySemaphore &operator=(BinarySemaphore &&other) = delete;
    ~BinarySemaphore() = default;

    void Signal()
    {
        m_value.fetch_add(1u, std::memory_order_release);
    }

    void Wait()
    {
        auto value = m_value.load(std::memory_order_relaxed);

        // wait for value be non-zero.
        while (value == 0u || !m_value.compare_exchange_strong(value, value - 1u, std::memory_order_acquire)) {
            value = m_value.load();
        }
    }

private:
    std::atomic<UInt8> m_value { 1u };
};

} // namespace hyperion::v2

#endif

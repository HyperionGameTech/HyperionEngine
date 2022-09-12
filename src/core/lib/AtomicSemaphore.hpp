#ifndef HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H
#define HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H

#include <core/Memory.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>

#include <atomic>

namespace hyperion::v2
{

// binary semaphore
class BinarySemaphore
{
public:
    BinarySemaphore() = default;
    BinarySemaphore(const BinarySemaphore &other) = delete;
    BinarySemaphore &operator=(const BinarySemaphore &other) = delete;
    BinarySemaphore(BinarySemaphore &&other) = delete;
    BinarySemaphore &operator=(BinarySemaphore &&other) = delete;
    ~BinarySemaphore() = default;

    HYP_FORCE_INLINE void Signal()
    {
        m_value.fetch_add(1u, std::memory_order_release);
    }

    HYP_FORCE_INLINE void Wait()
    {
        auto value = m_value.load(std::memory_order_relaxed);

        // wait for value be non-zero.
        while (value == 0u || !m_value.compare_exchange_strong(value, value - 1u, std::memory_order_acquire, std::memory_order_relaxed)) {
            value = m_value.load(std::memory_order_relaxed);
        }
    }

private:
    std::atomic<UInt8> m_value { 1u };
};

} // namespace hyperion::v2

#endif

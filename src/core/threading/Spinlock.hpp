/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SPINLOCK_HPP
#define HYPERION_SPINLOCK_HPP

#include <core/Defines.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/debug/Debug.hpp>

#include <Types.hpp>

namespace hyperion {
namespace threading {

class Spinlock
{
public:
    struct Guard
    {
        Spinlock    &m_spinlock;

        Guard(Spinlock &spinlock)
            : m_spinlock(spinlock)
        {
            m_spinlock.Lock();
        }

        ~Guard()
        {
            m_spinlock.Unlock();
        }
    };

    Spinlock()
        : m_value(0)
    {
    }

    Spinlock(const Spinlock &)                   = delete;
    Spinlock &operator=(const Spinlock &)        = delete;

    Spinlock(Spinlock &&other) noexcept
        : m_value(other.m_value.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    Spinlock &operator=(Spinlock &&other) noexcept
    {
        m_value.Set(other.m_value.Exchange(0, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

        return *this;
    }

    ~Spinlock()
    {
        AssertThrow(m_value.Get(MemoryOrder::ACQUIRE) == 0);
    }

    void Lock()
    {
        while (true) {
            if (TryLock()) {
                return;
            }

            while (m_value.Get(MemoryOrder::ACQUIRE) != 0) {
                // Spin
                HYP_WAIT_IDLE();
            }
        }
    }

    bool TryLock()
    {
        uint32 expected = 0;

        return m_value.CompareExchangeStrong(expected, 1, MemoryOrder::ACQUIRE);
    }

    void Unlock()
    {
        m_value.Set(0, MemoryOrder::RELEASE);
    }

private:
    AtomicVar<uint32>   m_value;
};

} // namespace threading

using threading::Spinlock;

} // namespace hyperion

#endif

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SPINLOCK_HPP
#define HYPERION_SPINLOCK_HPP

#include <core/Defines.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Threads.hpp>

#include <core/debug/Debug.hpp>

#include <Types.hpp>

namespace hyperion {
namespace threading {

class Spinlock
{
public:
    Spinlock(AtomicVar<uint64>& value)
        : m_value(value)
    {
    }

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    Spinlock(Spinlock&& other) noexcept = delete;
    Spinlock& operator=(Spinlock&& other) noexcept = delete;

    ~Spinlock() = default;

    void LockWriter()
    {
        uint64 state = m_value.BitOr(0x1, MemoryOrder::ACQUIRE_RELEASE);
        while (state & (~0u << 1))
        {
            state = m_value.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }
    }

    void UnlockWriter()
    {
        m_value.BitAnd(~uint64(0x1), MemoryOrder::RELEASE);
    }

    void LockReader()
    {
        uint64 state = m_value.Increment(0x2, MemoryOrder::ACQUIRE_RELEASE);
        while (state & 0x1)
        {
            state = m_value.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }
    }

    void UnlockReader()
    {
        m_value.Decrement(0x2, MemoryOrder::RELEASE);
    }

private:
    AtomicVar<uint64>& m_value;
};

} // namespace threading

using threading::Spinlock;

} // namespace hyperion

#endif

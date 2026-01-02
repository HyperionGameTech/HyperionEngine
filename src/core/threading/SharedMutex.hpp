/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/threading/AtomicVar.hpp>

namespace Hyperion {
namespace threading {

HYP_API extern void ThreadSleep(uint32 milliseconds);

/*! \brief Single producer, multiple consumer spinlock.
 *  Writer has exclusive access, readers have shared access.
 *
 *  Intrusive, meaning the user must provide a pointer to volatile int64  to use as the lock state.
 */
class SharedMutex final
{
    static constexpr uint32 MaxSpins = 1024; // before we yield

public:
    SharedMutex()
        : m_value(0)
    {
    }

    SharedMutex(const SharedMutex&) = delete;
    SharedMutex& operator=(const SharedMutex&) = delete;

    SharedMutex(SharedMutex&& other) noexcept = delete;
    SharedMutex& operator=(SharedMutex&& other) noexcept = delete;

    ~SharedMutex() = default;

    void LockWriter() const
    {
        uint32 numSpins = 0;

        union
        {
            int64 state;
            uint64 ustate;
        };

        state = AtomicBitOr(&m_value, 0x1);

        while (ustate & (~0ull << 1))
        {
            for (int i = 0; i < 32; i++)
            {
                HYP_WAIT_IDLE();
            }

            if (numSpins++ >= MaxSpins)
            {
                // yield to other threads
                ThreadSleep(0);
                numSpins = 0;
            }

            // read and try again
            state = AtomicBitOr(&m_value, 0);
        }
    }

    void UnlockWriter() const
    {
        AtomicBitAnd(&m_value, ~0x1);
    }

    void LockReader() const
    {
        uint32 numSpins = 0;

        union
        {
            int64 state;
            uint64 ustate;
        };

        state = AtomicAdd(&m_value, 2);

        while (ustate & 0x1)
        {
            for (int i = 0; i < 32; i++)
            {
                HYP_WAIT_IDLE();
            }

            if (numSpins++ >= MaxSpins)
            {
                ThreadSleep(0);
                numSpins = 0;
            }

            // read and try again
            state = AtomicAdd(&m_value, 0);
        }
    }

    void UnlockReader() const
    {
        AtomicSub(&m_value, 2);
    }

private:
    mutable volatile int64 m_value;
};

} // namespace threading

using threading::SharedMutex;

} // namespace Hyperion

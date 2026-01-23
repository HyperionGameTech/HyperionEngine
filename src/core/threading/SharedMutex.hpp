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
 */
class SharedMutex final
{
    static constexpr uint32 MaxSpinsBeforeYield = 16; // spin until we yield

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

        int64 expected = 0;
        while (!AtomicCompareExchange(&m_value, expected, 1))
        {
            // volatile read
            while (m_value != 0)
            {
                if (numSpins++ < MaxSpinsBeforeYield)
                {
                    HYP_WAIT_IDLE();
                }
                else
                {
                    // yield to other threads
                    ThreadSleep(0);
                }
            }
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
        
        // first pass: optimistic read
        if ((m_value & 0x1) == 0)
        {
            state = AtomicAdd(&m_value, 2);

            if ((state & 0x1) == 0)
            {
                // successfully acquired read lock
                return;
            }

            AtomicSub(&m_value, 2);
        }

        while (true)
        {
            // failed, wait for writer to release
            if (m_value & 0x1)
            {
                if (numSpins++ < MaxSpinsBeforeYield)
                {
                    HYP_WAIT_IDLE();
                }
                else
                {
                    ThreadSleep(0);
                }

                continue;
            }

            state = AtomicAdd(&m_value, 2);

            if ((state & 0x1) == 0)
            {
                // successfully acquired read lock
                return;
            }

            AtomicSub(&m_value, 2);
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

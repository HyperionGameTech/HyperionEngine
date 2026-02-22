/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/LockGuard.hpp>

namespace Hyperion {
namespace threading {

HYP_API extern void ThreadSleep(uint32 milliseconds);

class AtomicFlag final
{
    static constexpr uint32 MaxSpinsBeforeYield = 16; // spin until we yield

    friend class TLockGuard<AtomicFlag>;

public:
    using Guard = TLockGuard<AtomicFlag>;

    AtomicFlag()
        : m_value(0)
    {
    }

    AtomicFlag(const AtomicFlag&) = delete;
    AtomicFlag& operator=(const AtomicFlag&) = delete;

    AtomicFlag(AtomicFlag&& other) noexcept = delete;
    AtomicFlag& operator=(AtomicFlag&& other) noexcept = delete;

    ~AtomicFlag() = default;

    bool Load() const
    {
        return AtomicAdd(&m_value, 0) != 0;
    }

    void Store(bool value)
    {
        AtomicExchange(&m_value, value ? 1 : 0);
    }

    void Acquire() const
    {
        uint32 numSpins = 0;

        int64 expected = 0;
        while (!AtomicCompareExchange(&m_value, expected, 1))
        {
            expected = 0;
            
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

    void Release() const
    {
        AtomicExchange(&m_value, 0);
    }

private:
    // To be used with TLockGuard
    HYP_FORCE_INLINE void Lock() const
    {
        Acquire();
    }

    HYP_FORCE_INLINE void Unlock() const
    {
        Release();
    }

    mutable volatile int64 m_value;
};

} // namespace threading

using threading::AtomicFlag;

} // namespace Hyperion

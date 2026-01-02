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
    static constexpr uint32 MaxSpins = 1024; // before we yield

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

    void Acquire() const
    {
        uint32 numSpins = 0;
        int64 expected = 0;

        while (!AtomicCompareExchange(&m_value, expected, 1))
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

            expected = 0;
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

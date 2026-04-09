/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/LockGuard.hpp>

namespace Hyperion {
namespace threading {

HYP_API extern void ThreadSleep(uint32 milliseconds);

class AtomicFlag final
{
    static constexpr uint32 MaxSpinsBeforeYield = 16; // spin until we yield

    friend class TLockGuard<AtomicFlag>;

public:
    using Guard = TLockGuard<AtomicFlag>;

    HYP_FORCE_INLINE AtomicFlag()
        : m_value(0)
    {
    }

    HYP_FORCE_INLINE explicit AtomicFlag(bool initialValue)
        : m_value(initialValue ? 1 : 0)
    {
    }

    AtomicFlag(const AtomicFlag&) = delete;
    AtomicFlag& operator=(const AtomicFlag&) = delete;

    AtomicFlag(AtomicFlag&& other) noexcept = delete;
    AtomicFlag& operator=(AtomicFlag&& other) noexcept = delete;

    ~AtomicFlag() = default;

    HYP_FORCE_INLINE bool Load() const
    {
        return AtomicAdd(&m_value, 0) != 0;
    }

    HYP_FORCE_INLINE void Store(bool value)
    {
        AtomicExchange(&m_value, value ? 1 : 0);
    }

    HYP_FORCE_INLINE bool LoadVolatile() const
    {
        return m_value != 0;
    }

    HYP_FORCE_INLINE void StoreVolatile(bool value)
    {
        m_value = value ? 1 : 0;
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

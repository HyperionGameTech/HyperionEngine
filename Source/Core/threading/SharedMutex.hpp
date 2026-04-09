/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/threading/AtomicVar.hpp>

#include <Core/utilities/ByteUtil.hpp>

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
    
    HYP_NODISCARD bool TryLockWriter() const
    {
        int64 expected = 0;
        if (AtomicCompareExchange(&m_value, expected, 1))
        {
            return true;
        }

        return false;
    }

    /*! \brief Remove the write lock */
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
    
    HYP_NODISCARD bool TryLockReader() const
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
                return true;
            }

            AtomicSub(&m_value, 2);
        }

        return false;
    }

    /*! \brief Unlock a single reader. Returns the number of readers remaining at the time of release */
    uint32 UnlockReader() const
    {
        return (uint32)ByteUtil::BitCount(AtomicSub(&m_value, 2) - 2);
    }

    volatile int64* GetInternalValuePtr() const
    {
        return &m_value;
    }

private:
    mutable volatile int64 m_value;
};

} // namespace threading

using threading::SharedMutex;

} // namespace Hyperion

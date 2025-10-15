/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Threads.hpp>

#include <core/debug/Debug.hpp>

#include <core/Types.hpp>

namespace hyperion {
namespace threading {

enum SpinlockType : int
{
    SPMC,
    MPMC
};

template <SpinlockType Type>
class Spinlock;

/*! \brief Single producer, multiple consumer spinlock.
 *  Writer has exclusive access, readers have shared access.
 *
 *  Intrusive, meaning the user must provide a pointer to volatile int64  to use as the lock state.
 */
template <>
class Spinlock<SPMC>
{
    static constexpr uint32 MaxSpins = 1024; // before we yield
public:
    Spinlock(volatile int64* value)
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
        uint32 numSpins = 0;

        union
        {
            int64 state;
            uint64 ustate;
        };

        state = AtomicBitOr(m_value, 0x1);

        while (ustate & (~0ull << 1))
        {
            for (int i = 0; i < 32; i++)
            {
                HYP_WAIT_IDLE();
            }

            if (numSpins++ >= MaxSpins)
            {
                // yield to other threads
                Threads::Sleep(0);
                numSpins = 0;
            }

            // read and try again
            state = AtomicBitOr(m_value, 0);
        }
    }

    void UnlockWriter()
    {
        AtomicBitAnd(m_value, ~0x1);
    }

    void LockReader()
    {
        uint32 numSpins = 0;

        union
        {
            int64 state;
            uint64 ustate;
        };

        state = AtomicAdd(m_value, 2);

        while (ustate & 0x1)
        {
            for (int i = 0; i < 32; i++)
            {
                HYP_WAIT_IDLE();
            }

            if (numSpins++ >= MaxSpins)
            {
                Threads::Sleep(0);
                numSpins = 0;
            }

            // read and try again
            state = AtomicAdd(m_value, 0);
        }
    }

    void UnlockReader()
    {
        AtomicSub(m_value, 2);
    }

private:
    volatile int64* m_value;
};

template <>
class Spinlock<MPMC>
{
    static constexpr uint32 MaxSpins = 1024; // before we yield

public:
    Spinlock(volatile int64* value)
        : m_value(value)
    {
    }

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    Spinlock(Spinlock&& other) noexcept = delete;
    Spinlock& operator=(Spinlock&& other) noexcept = delete;

    ~Spinlock() = default;

    void Lock()
    {
        uint32 numSpins = 0;
        int64 expected = 0;
        
        while (!AtomicCompareExchange(m_value, expected, 1))
        {
            for (int i = 0; i < 32; i++)
            {
                HYP_WAIT_IDLE();
            }
            
            if (numSpins++ >= MaxSpins)
            {
                Threads::Sleep(0);
                numSpins = 0;
            }
            
            expected = 0;
        }
    }

    void Unlock()
    {
        AtomicExchange(m_value, 0);
    }

private:
    volatile int64* m_value;
};

template <class T>
struct RWLock;

template <class T>
struct ReadLock;

template <class T>
struct WriteLock;

template <>
struct RWLock<Spinlock<MPMC>>
{
    Spinlock<MPMC>& spinlock;

    RWLock(Spinlock<MPMC>& spinlock)
        : spinlock(spinlock)
    {
        spinlock.Lock();
    }

    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;

    RWLock(RWLock&& other) noexcept = delete;
    RWLock& operator=(RWLock&& other) noexcept = delete;

    ~RWLock()
    {
        spinlock.Unlock();
    }
};

template <>
struct ReadLock<Spinlock<SPMC>>
{
    Spinlock<SPMC>& spinlock;

    ReadLock(Spinlock<SPMC>& spinlock)
        : spinlock(spinlock)
    {
        spinlock.LockReader();
    }

    ReadLock(const ReadLock&) = delete;
    ReadLock& operator=(const ReadLock&) = delete;

    ReadLock(ReadLock&& other) noexcept = delete;
    ReadLock& operator=(ReadLock&& other) noexcept = delete;

    ~ReadLock()
    {
        spinlock.UnlockReader();
    }
};

template <>
struct WriteLock<Spinlock<SPMC>>
{
    Spinlock<SPMC>& spinlock;

    WriteLock(Spinlock<SPMC>& spinlock)
        : spinlock(spinlock)
    {
        spinlock.LockWriter();
    }

    WriteLock(const WriteLock&) = delete;
    WriteLock& operator=(const WriteLock&) = delete;

    WriteLock(WriteLock&& other) noexcept = delete;
    WriteLock& operator=(WriteLock&& other) noexcept = delete;

    ~WriteLock()
    {
        spinlock.UnlockWriter();
    }
};

} // namespace threading

using threading::Spinlock;
using threading::SPMC;
using threading::MPMC;
using threading::RWLock;
using threading::ReadLock;
using threading::WriteLock;

} // namespace hyperion

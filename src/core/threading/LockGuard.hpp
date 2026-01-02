/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/debug/Debug.hpp>

namespace Hyperion {
namespace threading {

template <class TMutex>
class TLockGuard final
{
public:
    TLockGuard()
        : mutex(nullptr)
    {
    }

    TLockGuard(TMutex& mutex)
        : mutex(&mutex)
    {
        mutex.Lock();
    }

    TLockGuard(const TLockGuard& other) = delete;
    TLockGuard& operator=(const TLockGuard& other) = delete;

    TLockGuard(TLockGuard&& other) noexcept = delete;
    TLockGuard& operator=(TLockGuard&& other) noexcept = delete;

    ~TLockGuard()
    {
        if (mutex)
            mutex->Unlock();
    }

    void Reset()
    {
        if (mutex)
        {
            mutex->Unlock();
        }

        mutex = nullptr;
    }

    void Reset(TMutex& newMutex)
    {
        if (mutex)
        {
            mutex->Unlock();
        }

        mutex = &newMutex;
        mutex->Lock();
    }

private:
    TMutex* mutex;
};

template <class TMutex>
class TSharedLock final
{
public:
    TSharedLock(TMutex& mutex)
        : mutex(&mutex)
    {
        mutex.LockReader();
    }

    TSharedLock(const TSharedLock& other) = delete;
    TSharedLock& operator=(const TSharedLock& other) = delete;

    TSharedLock(TSharedLock&& other) noexcept = delete;
    TSharedLock& operator=(TSharedLock&& other) noexcept = delete;

    ~TSharedLock()
    {
        if (mutex)
            mutex->UnlockReader();
    }

    void Reset()
    {
        if (mutex)
        {
            mutex->UnlockReader();
        }

        mutex = nullptr;
    }

    void Reset(TMutex& newMutex)
    {
        if (mutex)
        {
            mutex->UnlockReader();
        }

        mutex = &newMutex;
        mutex->LockReader();
    }
private:
    TMutex* mutex;
};

template <class TMutex>
class TUniqueLock final
{
public:
    TUniqueLock(TMutex& mutex)
        : mutex(&mutex)
    {
        mutex.LockWriter();
    }

    TUniqueLock(const TUniqueLock& other) = delete;
    TUniqueLock& operator=(const TUniqueLock& other) = delete;

    TUniqueLock(TUniqueLock&& other) noexcept = delete;
    TUniqueLock& operator=(TUniqueLock&& other) noexcept = delete;

    ~TUniqueLock()
    {
        if (mutex)
            mutex->UnlockWriter();
    }

    void Reset()
    {
        if (mutex)
        {
            mutex->UnlockWriter();
        }

        mutex = nullptr;
    }

    void Reset(TMutex& newMutex)
    {
        if (mutex)
        {
            mutex->UnlockWriter();
        }

        mutex = &newMutex;
        mutex->LockWriter();
    }

private:
    TMutex* mutex;
};

} // namespace threading

using threading::TLockGuard;
using threading::TSharedLock;
using threading::TUniqueLock;

} // namespace Hyperion

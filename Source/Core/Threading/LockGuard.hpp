/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Debug/Debug.hpp>

namespace Hyperion {
namespace threading {

template <class TLockObject>
class TLockGuard final
{
public:
    TLockGuard()
        : obj(nullptr)
    {
    }

    TLockGuard(TLockObject& obj)
        : obj(&obj)
    {
        obj.Lock();
    }

    TLockGuard(const TLockGuard& other) = delete;
    TLockGuard& operator=(const TLockGuard& other) = delete;

    TLockGuard(TLockGuard&& other) noexcept
        : obj(other.obj)
    {
        other.obj = nullptr;
    }

    TLockGuard& operator=(TLockGuard&& other) noexcept
    {
        if (obj)
            obj->Unlock();

        obj = other.obj;
        other.obj = nullptr;

        return *this;
    }

    ~TLockGuard()
    {
        if (obj)
            obj->Unlock();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return obj != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return obj == nullptr;
    }

    void Reset()
    {
        if (obj)
        {
            obj->Unlock();
        }

        obj = nullptr;
    }

    void Reset(TLockObject& newMutex)
    {
        if (obj)
        {
            obj->Unlock();
        }

        obj = &newMutex;
        obj->Lock();
    }

private:
    TLockObject* obj;
};

template <class TLockObject>
class TSharedLock final
{
public:
    TSharedLock()
        : obj(nullptr)
    {
    }

    TSharedLock(TLockObject& obj)
        : obj(&obj)
    {
        obj.LockReader();
    }

    TSharedLock(const TSharedLock& other)
        : obj(other.obj)
    {
        if (obj)
            obj->LockReader();
    }

    TSharedLock& operator=(const TSharedLock& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (obj)
            obj->UnlockReader();

        obj = other.obj;

        if (obj)
            obj->LockReader();

        return *this;
    }

    TSharedLock(TSharedLock&& other) noexcept
        : obj(other.obj)
    {
        other.obj = nullptr;
    }

    TSharedLock& operator=(TSharedLock&& other) noexcept
    {
        if (obj)
            obj->UnlockReader();

        obj = other.obj;
        other.obj = nullptr;

        return *this;
    }

    ~TSharedLock()
    {
        if (obj)
            obj->UnlockReader();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return obj != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return obj == nullptr;
    }

    void Reset()
    {
        if (obj)
        {
            obj->UnlockReader();
        }

        obj = nullptr;
    }

    void Reset(TLockObject& newMutex)
    {
        if (obj == &newMutex)
        {
            return;
        }

        if (obj)
        {
            obj->UnlockReader();
        }

        obj = &newMutex;
        obj->LockReader();
    }
    
private:
    TLockObject* obj;
};

template <class TLockObject>
class TUniqueLock final
{
public:
    TUniqueLock()
        : obj(nullptr)
    {
    }

    TUniqueLock(TLockObject& obj)
        : obj(&obj)
    {
        obj.LockWriter();
    }

    TUniqueLock(const TUniqueLock& other) = delete;
    TUniqueLock& operator=(const TUniqueLock& other) = delete;

    TUniqueLock(TUniqueLock&& other) noexcept = delete;
    TUniqueLock& operator=(TUniqueLock&& other) noexcept = delete;

    ~TUniqueLock()
    {
        if (obj)
            obj->UnlockWriter();
    }
    
    HYP_FORCE_INLINE explicit operator bool() const
    {
        return obj != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return obj == nullptr;
    }

    void Reset()
    {
        if (obj)
        {
            obj->UnlockWriter();
        }

        obj = nullptr;
    }

    void Reset(TLockObject& newMutex)
    {
        if (obj == &newMutex)
        {
            return;
        }

        if (obj)
        {
            obj->UnlockWriter();
        }

        obj = &newMutex;
        obj->LockWriter();
    }

private:
    TLockObject* obj;
};

} // namespace threading

using threading::TLockGuard;
using threading::TSharedLock;
using threading::TUniqueLock;

} // namespace Hyperion

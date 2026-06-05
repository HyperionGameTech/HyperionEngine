/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Debug/Debug.hpp>

#include <Core/Threading/LockGuard.hpp>

namespace Hyperion {

template <class TLockObject>
class TSharedResLock : public TSharedLock<TLockObject>
{
public:
    TSharedResLock() = default;
    TSharedResLock(TLockObject& obj) : TSharedLock<TLockObject>(obj) {}
};

template <class TLockObject>
class TUniqueResLock : public TUniqueLock<TLockObject>
{
public:
    TUniqueResLock() = default;
    TUniqueResLock(TLockObject& obj) : TUniqueLock<TLockObject>(obj) {}
};

} // namespace Hyperion

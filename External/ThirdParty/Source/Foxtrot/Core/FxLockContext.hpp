#pragma once

#include "FxPanic.hpp"

#include <mutex>

template <typename TObjectType>
class FxLockContext
{
public:
    FxLockContext(std::mutex& mutex, TObjectType& object) : mMutex(mutex), mObject(object)
    {
        mMutex.lock();
        mbIsLocked = true;
    }

    TObjectType& Get()
    {
        FxAssert(mbIsLocked == true);
        return mObject;
    }

    TObjectType* operator->()
    {
        FxAssert(mbIsLocked == true);
        return &mObject;
    }

    void Unlock()
    {
        mbIsLocked = false;
        mMutex.unlock();
    }

    ~FxLockContext() { Unlock(); }


private:
    std::mutex& mMutex;
    TObjectType& mObject;
    bool mbIsLocked = false;
};

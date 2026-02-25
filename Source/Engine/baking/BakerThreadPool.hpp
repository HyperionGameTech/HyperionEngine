/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/threading/TaskSystem.hpp>

namespace Hyperion {
namespace Baking {

class BakerWorkerThread : public TaskThread
{
public:
    BakerWorkerThread(ThreadId id)
        : TaskThread(id)
    {
    }

    virtual ~BakerWorkerThread() override = default;
};

class BakerThreadPool : public TaskThreadPool
{
public:
    BakerThreadPool(const TypeInfo& typeInfo, uint32 numThreads)
        : TaskThreadPool(TypeWrapper<BakerWorkerThread>(), *TypeInfo_GetName(typeInfo), numThreads)
    {
    }

    virtual ~BakerThreadPool() override = default;
};

} // namespace Baking
} // namespace Hyperion
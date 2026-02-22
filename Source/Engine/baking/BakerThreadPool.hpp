/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/TaskSystem.hpp>

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
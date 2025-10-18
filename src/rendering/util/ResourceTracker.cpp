/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/util/ResourceTracker.hpp>

#include <core/reflection/HypClass.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/pool/Pool.hpp>

#include <rendering/RenderMemory.hpp>

namespace hyperion {

HYP_API const TypeInfo& HypClass_GetTypeInfo(const HypClass& hypClass)
{
    return *hypClass.GetTypeInfo();
}

void ResourceTrackerBase::FreeRenderProxyEx(RenderProxyEx* ext)
{
    if (!ext)
    {
        return;
    }

    // Clean up render side only data
    Threads::AssertOnThread(g_renderThread);

    g_renderPool->Free(ext);
    HYP_BREAKPOINT;
}

} // namespace hyperion
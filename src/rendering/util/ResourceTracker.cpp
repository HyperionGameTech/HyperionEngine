/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/util/ResourceTracker.hpp>

#include <core/threading/Threads.hpp>

#include <core/memory/pool/Pool.hpp>

#include <rendering/RenderMemory.hpp>

namespace hyperion {

void ResourceTrackerBase::FreeRenderProxyEx(RenderProxyEx* ext)
{
    if (!ext)
    {
        return;
    }

    // Clean up render side only data
    Threads::AssertOnThread(g_renderThread);

    g_renderPool->Free(ext);
}

} // namespace hyperion
/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Constants.hpp>

#include <core/memory/allocator/Allocator.hpp>

namespace hyperion {

namespace memory {
class Pool;
} // namespace memory

using memory::Pool;

HYP_API extern Pool* g_renderPool;
HYP_API extern Pool* g_framePools[NumMultiBuffers];

HYP_API extern uint32 RenderApi_GetFrameIndex();

static inline Pool* GetRenderPool()
{
    return g_renderPool;
}

static inline Pool* GetFramePool()
{
    return g_framePools[RenderApi_GetFrameIndex()];
}

template <int FrameIndex>
static inline Pool* GetFramePoolT()
{
    static_assert(FrameIndex < NumMultiBuffers, "FrameIndex must be < NumMultiBuffers!");
    return g_framePools[FrameIndex];
}

using RenderAllocator = TAllocator<Pool, &GetRenderPool>;
using FrameAllocator = TAllocator<Pool, &GetFramePool>;

} // namespace hyperion

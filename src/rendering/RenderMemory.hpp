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

using RenderAllocator = TAllocator<Pool, &g_renderPool>;

HYP_API extern TAllocator<Pool>& GetCurrentFrameAllocator();
HYP_API extern TAllocator<Pool>& GetFrameAllocator(uint32 frameIndex);

} // namespace hyperion

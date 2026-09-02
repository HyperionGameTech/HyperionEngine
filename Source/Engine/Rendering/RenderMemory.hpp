/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/Allocator/AllocatorFwd.hpp>

namespace Hyperion {

static constexpr size_t RenderPoolBlockSize = 64 * 1024 * 1024; // 64 MiB -- needs some more space for structured buffers cpu-side data
static constexpr size_t RenderArenaSize = 64 * 1024;            // 64 KiB

static constexpr size_t RHIPoolBlockSize = 16 * 1024 * 1024;     // 16 MiB

ENGINE_API extern Pool* g_renderPool;
ENGINE_API extern Arena* g_renderArena;

using RenderAllocator = AllocatorInstance<Pool, &g_renderPool>;
using RenderTempAllocator = AllocatorInstance<Arena, &g_renderArena>;

#if defined(HYP_VULKAN)

ENGINE_API extern Pool* g_vulkanPool;
using VulkanAllocator = AllocatorInstance<Pool, &g_vulkanPool>;

using RHIAllocator = VulkanAllocator;
//constexpr Pool*& g_rhiPool = g_vulkanPool;
#define g_rhiPool g_vulkanPool

#elif defined(HYP_DX12)

ENGINE_API extern Pool* g_dx12Pool;
using DX12Allocator = AllocatorInstance<Pool, &g_dx12Pool>;

using RHIAllocator = DX12Allocator;
//constexpr Pool*& g_rhiPool = g_dx12Pool;

#define g_rhiPool g_dx12Pool

#endif

} // namespace Hyperion

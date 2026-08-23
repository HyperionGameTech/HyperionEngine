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

static constexpr size_t RHIPoolBlockSize = 1 * 1024 * 1024;     // 1 MiB
static constexpr size_t RHIArenaSize = 64 * 1024;               // 64 KiB

ENGINE_API extern Pool* g_renderPool;
ENGINE_API extern Arena* g_renderArena;

using RenderAllocator = AllocatorInstance<Pool, &g_renderPool>;
using RenderTempAllocator = AllocatorInstance<Arena, &g_renderArena>;

#if HYP_VULKAN

ENGINE_API extern Pool* g_vulkanPool;
using VulkanAllocator = AllocatorInstance<Pool, &g_vulkanPool>;

ENGINE_API extern Arena* g_vulkanArena;
using VulkanTempAllocator = AllocatorInstance<Arena, &g_vulkanArena>;

using RHIAllocator = VulkanAllocator;
using RHITempAllocator = VulkanTempAllocator;

#define g_rhiPool g_vulkanPool
#define g_rhiArena g_vulkanArena

#elif HYP_DX12

ENGINE_API extern Pool* g_dx12Pool;
using DX12Allocator = AllocatorInstance<Pool, &g_dx12Pool>;

ENGINE_API extern Arena* g_dx12Arena;
using DX12TempAllocator = AllocatorInstance<Arena, &g_dx12Arena>;

using RHIAllocator = DX12Allocator;
using RHITempAllocator = DX12TempAllocator;

#define g_rhiPool g_dx12Pool
#define g_rhiArena g_dx12Arena

#endif

} // namespace Hyperion

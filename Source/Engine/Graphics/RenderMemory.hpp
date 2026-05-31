/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

namespace Hyperion {

#pragma region Fwd declarations
namespace memory {

class Pool;

template <class AllocatorType> class TArena;

using Arena = TArena<DynamicAllocator>;

} // namespace memory

using memory::Pool;
using memory::Arena;

#pragma endregion Fwd declarations

static constexpr size_t RenderPoolBlockSize = 16 * 1024 * 1024;    // 16 MiB
static constexpr size_t RenderArenaSize = 4 * 1024 * 1024;

static constexpr size_t RHIPoolBlockSize = 8 * 1024 * 1024;
static constexpr size_t RHIArenaSize = 1 * 1024 * 1024;

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

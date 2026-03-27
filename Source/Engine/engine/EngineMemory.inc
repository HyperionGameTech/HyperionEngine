
#if HYP_ENGINE_MEMORY_IMPLEMENTATION
#define HYP_ENGINE_MEMORY_DECL HYP_EXPORT

HYP_ENGINE_MEMORY_DECL Pool* const* g_enginePools[EPN_MAX] = {
    &g_objectPool, // EPN_CORE
    &g_renderPool, // EPN_RENDER
    &g_scenePool   // EPN_SCENE
};

#else

#ifdef HYP_ENGINE_MEMORY_DECL
#undef HYP_ENGINE_MEMORY_DECL
#endif // HYP_ENGINE_MEMORY_DECL

#define HYP_ENGINE_MEMORY_DECL HYP_API extern

static constexpr size_t ObjectPoolBlockSize = 16 * 1024 * 1024;    // 16 MiB
static constexpr size_t RenderPoolBlockSize = 16 * 1024 * 1024;    // 16 MiB
static constexpr size_t ScenePoolBlockSize = 16 * 1024 * 1024;    // 16 MiB
static constexpr size_t TaskPoolBlockSize = 4 * 1024 * 1024;     // 4 MiB
static constexpr size_t ResourcePoolBlockSize = 1 * 1024 * 1024;  // 1 MiB
static constexpr size_t AssetPoolBlockSize = 64 * 1024 * 1024;    // 64 MiB
static constexpr size_t StreamingPoolBlockSize = 1 * 1024 * 1024; // 1 MiB

static constexpr size_t SceneArenaSize = 64 * 1024;               // 64 KiB
static constexpr size_t StreamingArenaSize = 64 * 1024;           // 64 KiB
static constexpr size_t RenderArenaSize = 16 * 1024 * 1024;              // 64 MiB

#if HYP_VULKAN
static constexpr size_t VulkanPoolBlockSize = 8 * 1024 * 1024;
static constexpr size_t VulkanArenaSize = 1 * 1024 * 1024;
#elif HYP_DX12
static constexpr size_t DX12PoolBlockSize = 8 * 1024 * 1024;
static constexpr size_t DX12ArenaSize = 1 * 1024 * 1024;
#endif

HYP_ENGINE_MEMORY_DECL Pool* const* g_enginePools[EPN_MAX];
#endif

HYP_ENGINE_MEMORY_DECL Pool* g_objectPool;                  // Pool for object allocations, thread safe
HYP_ENGINE_MEMORY_DECL Pool* g_renderPool;                  // Pool for rendering general allocations
HYP_ENGINE_MEMORY_DECL Pool* g_scenePool;                   // Pool for scene-related allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_taskPool;                    // Pool for task system allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_assetPool;                   // Pool for asset system allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_streamingPool;               // Pool for streaming system allocations (thread safe)

#if HYP_VULKAN

HYP_ENGINE_MEMORY_DECL Pool* g_vulkanPool;
using VulkanAllocator = AllocatorInstance<Pool, &g_vulkanPool>;

HYP_ENGINE_MEMORY_DECL Arena* g_vulkanArena;
using VulkanTempAllocator = AllocatorInstance<Arena, &g_vulkanArena>;

using RHIAllocator = VulkanAllocator;
using RHITempAllocator = VulkanTempAllocator;

#define g_rhiPool g_vulkanPool
#define g_rhiArena g_vulkanArena

#elif HYP_DX12

HYP_ENGINE_MEMORY_DECL Pool* g_dx12Pool;
using DX12Allocator = AllocatorInstance<Pool, &g_dx12Pool>;

HYP_ENGINE_MEMORY_DECL Arena* g_dx12Arena;
using DX12TempAllocator = AllocatorInstance<Arena, &g_dx12Arena>;

using RHIAllocator = DX12Allocator;
using RHITempAllocator = DX12TempAllocator;

#define g_rhiPool g_dx12Pool
#define g_rhiArena g_dx12Arena

#endif

using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;
using RenderAllocator = AllocatorInstance<Pool, &g_renderPool>;
using StreamingAllocator = AllocatorInstance<Pool, &g_streamingPool>;

HYP_ENGINE_MEMORY_DECL Arena* g_renderArena; // Arena for the render thread to use for temporary allocations during the frame
HYP_ENGINE_MEMORY_DECL Arena* g_sceneArena;   // Arena for scene-related temporary allocations during the frame
HYP_ENGINE_MEMORY_DECL Arena* g_streamingArena;

using RenderTempAllocator = AllocatorInstance<Arena, &g_renderArena>;
using SceneTempAllocator = AllocatorInstance<Arena, &g_sceneArena>;
using StreamingTempAllocator = AllocatorInstance<Arena, &g_streamingArena>;

#if HYP_ENGINE_MEMORY_IMPLEMENTATION

static void InitMemoryPools()
{
    g_objectPool = new Pool(ObjectPoolBlockSize);
    g_renderPool = new Pool(RenderPoolBlockSize);
    g_scenePool = new Pool(ScenePoolBlockSize);
    g_taskPool = new Pool(TaskPoolBlockSize);
    g_assetPool = new Pool(AssetPoolBlockSize);
    g_streamingPool = new Pool(StreamingPoolBlockSize);

    g_sceneArena = new Arena(SceneArenaSize);
    g_streamingArena = new Arena(StreamingArenaSize);

#if HYP_VULKAN
    g_vulkanPool = new Pool(VulkanPoolBlockSize);
    g_vulkanArena = new Arena(VulkanArenaSize);
#elif HYP_DX12
    g_dx12Pool = new Pool(DX12PoolBlockSize);
    g_dx12Arena = new Arena(DX12ArenaSize);
#endif
}

#endif
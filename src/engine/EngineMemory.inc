
#if HYP_ENGINE_MEMORY_IMPLEMENTATION
#define HYP_ENGINE_MEMORY_DECL HYP_EXPORT

HYP_ENGINE_MEMORY_DECL Pool* const* g_enginePools[EPN_MAX] = {
    &g_objectPool, // EPN_CORE
    &g_renderPool, // EPN_RENDER
    &g_scenePool   // EPN_SCENE
};

#else
#define HYP_ENGINE_MEMORY_DECL HYP_API extern
static constexpr SizeType ObjectPoolBlockSize = 1 * 1024 * 1024;    // 1 MiB
static constexpr SizeType RenderPoolBlockSize = 2 * 1024 * 1024;    // 2 MiB
static constexpr SizeType FramePoolBlockSize = 16 * 1024;           // 16 KiB
static constexpr SizeType ScenePoolBlockSize = 16 * 1024 * 1024;    // 16 KiB
static constexpr SizeType TaskPoolBlockSize = 16 * 1024 * 1024;     // 16 KiB
static constexpr SizeType ResourcePoolBlockSize = 1 * 1024 * 1024;  // 1 MiB
static constexpr SizeType AssetPoolBlockSize = 1 * 1024 * 1024;     // 1 MiB
static constexpr SizeType StreamingPoolBlockSize = 1 * 1024 * 1024; // 1 MiB
static constexpr SizeType ScriptPoolBlockSize = 1 * 1024 * 1024;    // 1 MiB

static constexpr SizeType SceneArenaSize = 64 * 1024;               // 64 KiB
static constexpr SizeType StreamingArenaSize = 64 * 1024;           // 64 KiB
static constexpr SizeType RenderArenaSize = 64 * 1024;              // 64 KiB

#if HYP_VULKAN
static constexpr SizeType VulkanPoolBlockSize = 8 * 1024 * 1024;
static constexpr SizeType VulkanArenaSize = 1 * 1024 * 1024;
#endif

HYP_ENGINE_MEMORY_DECL Pool* const* g_enginePools[EPN_MAX];
#endif

HYP_ENGINE_MEMORY_DECL Pool* g_objectPool;                  // Pool for object allocations, thread safe
HYP_ENGINE_MEMORY_DECL Pool* g_renderPool;                  // Pool for render thread general allocations
HYP_ENGINE_MEMORY_DECL Pool* g_scenePool;                   // Pool for scene-related allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_taskPool;                    // Pool for task system allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_assetPool;                   // Pool for asset system allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_resourcePool;                // Pool for paged data allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_streamingPool;               // Pool for streaming system allocations (thread safe)

#if HYP_VULKAN
HYP_ENGINE_MEMORY_DECL Pool* g_vulkanPool;
using VulkanAllocator = AllocatorInstance<Pool, &g_vulkanPool>;

HYP_ENGINE_MEMORY_DECL Arena* g_vulkanArena;
using VulkanTempAllocator = AllocatorInstance<Arena, &g_vulkanArena>;
#endif

#if HYP_SCRIPT
HYP_ENGINE_MEMORY_DECL Pool* g_scriptPool; // Pool for script-related allocations (sim thread only)
#endif

using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;
using RenderAllocator = AllocatorInstance<Pool, &g_renderPool>;
using StreamingAllocator = AllocatorInstance<Pool, &g_streamingPool>;

#if HYP_SCRIPT
using ScriptAllocator = AllocatorInstance<Pool, &g_scriptPool>;
#endif

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
    g_renderPool = new Pool(RenderPoolBlockSize, PF_NONE, g_renderThread);
    g_scenePool = new Pool(ScenePoolBlockSize);
    g_taskPool = new Pool(TaskPoolBlockSize);
    g_resourcePool = new Pool(ResourcePoolBlockSize);
    g_assetPool = new Pool(AssetPoolBlockSize);
    g_streamingPool = new Pool(StreamingPoolBlockSize);
    g_scriptPool = new Pool(ScriptPoolBlockSize, PF_NONE, g_simThread);

    g_sceneArena = new Arena(SceneArenaSize);
    g_streamingArena = new Arena(StreamingArenaSize);

#if HYP_VULKAN
    g_vulkanPool = new Pool(VulkanPoolBlockSize);
    g_vulkanArena = new Arena(VulkanArenaSize);
#endif
}

#endif
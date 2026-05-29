
#if HYP_ENGINE_MEMORY_IMPLEMENTATION
#define HYP_ENGINE_MEMORY_DECL HYP_EXPORT

#else

#ifdef HYP_ENGINE_MEMORY_DECL
#undef HYP_ENGINE_MEMORY_DECL
#endif // HYP_ENGINE_MEMORY_DECL

#define HYP_ENGINE_MEMORY_DECL ENGINE_API extern

static constexpr size_t ObjectPoolBlockSize = 16 * 1024 * 1024;    // 16 MiB
static constexpr size_t ScenePoolBlockSize = 16 * 1024 * 1024;    // 16 MiB
static constexpr size_t TaskPoolBlockSize = 4 * 1024 * 1024;     // 4 MiB
static constexpr size_t ResourcePoolBlockSize = 1 * 1024 * 1024;  // 1 MiB
static constexpr size_t AssetPoolBlockSize = 64 * 1024 * 1024;    // 64 MiB
static constexpr size_t StreamingPoolBlockSize = 1 * 1024 * 1024; // 1 MiB

static constexpr size_t SceneArenaSize = 64 * 1024;               // 64 KiB
static constexpr size_t StreamingArenaSize = 64 * 1024;           // 64 KiB

#endif

HYP_ENGINE_MEMORY_DECL Pool* g_scenePool;                   // Pool for scene-related allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_assetPool;                   // Pool for asset system allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_streamingPool;               // Pool for streaming system allocations (thread safe)

using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;
using StreamingAllocator = AllocatorInstance<Pool, &g_streamingPool>;

HYP_ENGINE_MEMORY_DECL Arena* g_sceneArena;   // Arena for scene-related temporary allocations during the frame
HYP_ENGINE_MEMORY_DECL Arena* g_streamingArena;

using SceneTempAllocator = AllocatorInstance<Arena, &g_sceneArena>;
using StreamingTempAllocator = AllocatorInstance<Arena, &g_streamingArena>;

#if HYP_ENGINE_MEMORY_IMPLEMENTATION

static void InitMemoryPools()
{
    g_objectPool = new Pool(ObjectPoolBlockSize); // Defined in core lib, initialized here
    g_scenePool = new Pool(ScenePoolBlockSize);
    g_assetPool = new Pool(AssetPoolBlockSize);
    g_streamingPool = new Pool(StreamingPoolBlockSize);

    g_sceneArena = new Arena(SceneArenaSize);
    g_streamingArena = new Arena(StreamingArenaSize);
}

#endif

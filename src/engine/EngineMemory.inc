
#if HYP_ENGINE_MEMORY_IMPLEMENTATION
#define HYP_ENGINE_MEMORY_DECL HYP_EXPORT

HYP_ENGINE_MEMORY_DECL Pool* const* g_enginePools[EPN_MAX] = {
    &g_objectPool, // EPN_CORE
    &g_renderPool, // EPN_RENDER
    &g_scenePool   // EPN_SCENE
};

#else
#define HYP_ENGINE_MEMORY_DECL HYP_API extern
static constexpr SizeType ObjectPoolBlockSize = 64 * 1024 * 1024;
static constexpr SizeType RenderPoolBlockSize = 64 * 1024 * 1024;
static constexpr SizeType FramePoolBlockSize = 1 * 1024 * 1024;
static constexpr SizeType ScenePoolBlockSize = 8 * 1024 * 1024;
static constexpr SizeType TaskPoolBlockSize = 4 * 1024 * 1024;
static constexpr SizeType ResourcePoolBlockSize = 8 * 1024 * 1024;
static constexpr SizeType AssetPoolBlockSize = 16 * 1024 * 1024;
static constexpr SizeType StreamingPoolBlockSize = 16 * 1024 * 1024;
static constexpr SizeType ScriptPoolBlockSize = 16 * 1024 * 1024;

static constexpr SizeType SceneArenaSize = 1 * 1024 * 1024;
static constexpr SizeType StreamingArenaSize = 1 * 1024 * 1024;

#if HYP_SCRIPT
static constexpr SizeType ScriptArenaSize = 1 * 1024 * 1024;
#endif

HYP_ENGINE_MEMORY_DECL Pool* const* g_enginePools[EPN_MAX];
#endif

HYP_ENGINE_MEMORY_DECL Pool* g_objectPool;                  // Pool for object allocations - not thread safe per se, but used with proper locking in ObjectPool
HYP_ENGINE_MEMORY_DECL Pool* g_renderPool;                  // Pool for rendering allocations (render thread only)
HYP_ENGINE_MEMORY_DECL Pool* g_framePools[RingBufferDepth]; // Pools for per-frame allocations, on either game or render thread for their given frame index.
HYP_ENGINE_MEMORY_DECL Pool* g_scenePool;                   // Pool for scene-related allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_taskPool;                    // Pool for task system allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_resourcePool;                // Pool for IResource-derived allocations (thread safe)
HYP_ENGINE_MEMORY_DECL Pool* g_streamingPool;               // Pool for streaming system allocations (thread safe)

#if HYP_SCRIPT
HYP_ENGINE_MEMORY_DECL Pool* g_scriptPool; // Pool for script-related allocations (sim thread only)
#endif

using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;
using RenderAllocator = AllocatorInstance<Pool, &g_renderPool>;
using StreamingAllocator = AllocatorInstance<Pool, &g_streamingPool>;

#if HYP_SCRIPT
using ScriptAllocator = AllocatorInstance<Pool, &g_scriptPool>;
#endif

HYP_ENGINE_MEMORY_DECL TArena<RenderAllocator>* g_renderArena; // Arena for the render thread to use for temporary allocations during the frame
HYP_ENGINE_MEMORY_DECL TArena<SceneAllocator>* g_sceneArena;   // Arena for scene-related temporary allocations during the frame
HYP_ENGINE_MEMORY_DECL TArena<StreamingAllocator>* g_streamingArena;

#if HYP_SCRIPT
HYP_ENGINE_MEMORY_DECL TArena<ScriptAllocator>* g_scriptArena; // Arena for script-related temporary allocations during the frame (sim thread only)
#endif

using RenderTempAllocator = AllocatorInstance<TArena<RenderAllocator>, &g_renderArena>;
using SceneTempAllocator = AllocatorInstance<TArena<SceneAllocator>, &g_sceneArena>;
using StreamingTempAllocator = AllocatorInstance<TArena<StreamingAllocator>, &g_streamingArena>;

#if HYP_SCRIPT
using ScriptTempAllocator = AllocatorInstance<TArena<ScriptAllocator>, &g_scriptArena>;
#endif

#if HYP_ENGINE_MEMORY_IMPLEMENTATION

static void InitMemoryPools()
{
    g_objectPool = new Pool(ObjectPoolBlockSize, PF_NONE);
    g_renderPool = new Pool(RenderPoolBlockSize, PF_NONE, g_renderThread);

    for (uint32 i = 0; i < RingBufferDepth; i++)
    {
        g_framePools[i] = new Pool(FramePoolBlockSize, PF_NONE);
    }

    g_scenePool = new Pool(ScenePoolBlockSize, PF_THREAD_SAFE);
    g_taskPool = new Pool(TaskPoolBlockSize, PF_THREAD_SAFE);
    g_resourcePool = new Pool(ResourcePoolBlockSize, PF_THREAD_SAFE);
    g_assetPool = new Pool(AssetPoolBlockSize, PF_THREAD_SAFE);
    g_streamingPool = new Pool(StreamingPoolBlockSize, PF_THREAD_SAFE);
    g_scriptPool = new Pool(ScriptPoolBlockSize, PF_NONE, g_simThread);

    g_sceneArena = new TArena<SceneAllocator>(SceneArenaSize);
    g_streamingArena = new TArena<StreamingAllocator>(StreamingArenaSize);

#if HYP_SCRIPT
    g_scriptArena = new TArena<ScriptAllocator>(ScriptArenaSize);
#endif
}

#endif
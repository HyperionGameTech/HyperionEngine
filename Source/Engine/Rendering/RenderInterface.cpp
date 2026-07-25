/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/MaterialTextureCache.hpp>
#include <Rendering/Pass.hpp>
#include <Rendering/DrawCall.hpp>
#include <Rendering/GlobalBuffers.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/GenericPipelineCache.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/ComputePipeline.hpp>
#include <Rendering/RayTracingPipeline.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/AsyncCompute.hpp>
#include <Rendering/Bindless.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/Swapchain.hpp>
#include <Rendering/FinalPass.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/DescriptorSetCache.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/DebugDrawer.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/BLASCache.hpp>
#include <Rendering/CrashHandler.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RawBufferAllocator.hpp>
#include <Rendering/ScratchImageAllocator.hpp>
#include <Rendering/RenderGroupCache.hpp>
#include <Rendering/GpuTimerBackend.hpp>
#include <Rendering/RenderCommand.hpp>

#include <Framework/Resources/ResourceTracker.hpp>
#include <Framework/Resources/ResourceBinder.hpp>
#include <Rendering/resources/ResourceBindings.hpp>

#include <Rendering/Passes/EnvProbePass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/ShadowsPass.hpp>
#include <Rendering/Passes/ParticlesPass.hpp>
#include <Rendering/Passes/SpritePass.hpp>
#include <Rendering/Passes/UIPass.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>

#include <Scene/View.hpp>
#include <Scene/World.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/Light.hpp>
#include <Scene/ParticleVolume.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/Sprite.hpp>
#include <Scene/TextSprite.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Threading/Semaphore.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Memory/Pool/Pool.hpp>

// for EnumToString
#include <Core/Reflection/Enum.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/RawDataAsset.hpp>

#include <Framework/Config/EngineConfig.hpp>

#include <System/AppContext.hpp>

#include <HyperionEngine.hpp>

#include <semaphore>
#include <new>

namespace Hyperion {

using namespace Resources;

static_assert(RingBufferDepth <= MinSafeDeleteCycles,
              "RingBufferDepth must be less than or equal to MinSafeDeleteCycles to ensure safe deletion of resources.");

static constexpr uint32 MaxFramesBeforeDiscard = RingBufferDepth; // number of frames before ViewData is discarded if not written to

// must be greater than or equal to MinSafeDeleteCycles so that
// we can ensure no active views hold pointers to deleted objects.
static_assert(MaxFramesBeforeDiscard >= MinSafeDeleteCycles,
              "MaxFramesBeforeDiscard must be greater than or equal to MinSafeDeleteCycles");

// iterations per frame for cleaning up unused resources for passes
static constexpr int FrameCleanupBudget = 16;

EngineStatTimer g_statRenderThreadSync("Rendering/CPU/RenderThreadSync");
EngineStatTimer g_statSimThreadSync("Rendering/CPU/SimThreadSync");

EngineStatTimer g_statTotalStallTime("Rendering/CPU/TotalStallTime");

// Windows during which one thread holds the shared sim/render data region.
// These span two functions each, so they're timed manually rather than with ENGINE_STAT_SCOPE.
static EngineStatTimer s_statSimCommitWindow("Rendering/CPU/SimCommitWindow", /* resetPerFrame */ false);
static EngineStatTimer s_statRenderExclusiveWindow("Rendering/CPU/RenderExclusiveWindow");

static EngineStatTimer s_statCopyDependencies("Rendering/CPU/CopyDependencies");
static EngineStatTimer s_statResourceBindings("Rendering/CPU/ResourceBindings");
static EngineStatTimer s_statBuildDrawCalls("Rendering/CPU/BuildDrawCalls");

EngineStatGpuTimer g_statGpuFrameTime("Rendering/GPU/FrameTime");

static EngineStatTimer s_statViewDataAllocTime("Rendering/ViewData/AllocTime", /* resetPerFrame */ false);

/// ===== Memory pools =====
ENGINE_API Pool* g_renderPool;
ENGINE_API Arena* g_renderArena;

ENGINE_API Pool* g_rhiPool;
ENGINE_API Arena* g_rhiArena;
/// ========================

CVar<bool> g_cvEnableVSync("Rendering.VSync", true);
CVar<bool> g_cvEnableGpuStats("Rendering.EnableGpuStats", true);

namespace Framework {

/// atomic, incremented at the end of the render thread frame.
static volatile int64 s_frameCounter = 0;

/// thread-local ring buffer index for the game and render threads.
thread_local uint8* t_thisThreadRingIndex;
static uint8 s_ringIndex[2] = { 0 };

thread_local uint32 t_currentRenderThreadIndex;

/// Semaphores for synchronization the simulation and render threads.
static std::counting_semaphore<RingBufferDepth> s_dataProduced { 0 };
static std::counting_semaphore<RingBufferDepth> s_frameSubmitted { RingBufferDepth };

static PerformanceClock s_simCommitWindowStart;
static PerformanceClock s_renderExclusiveWindowStart;

enum
{
    TT_FrameDataProducer,
    TT_FrameDataConsumer
};

static inline int CurrentThreadType()
{
    const ThreadId& threadId = CurrentThreadId();

    if (threadId == g_renderThread)
    {
        return TT_FrameDataConsumer;
    }

    if (threadId == g_simThread)
    {
        return TT_FrameDataProducer;
    }

    // invalid
    return -1;
}

// Render thread owned View data
struct ViewData
{
    View* view = nullptr;
    RenderProxyList rplRender;
    RenderCollector renderCollector;
    uint32 lastUsedFrame = ~0u;
    uint32 numRefs = 0; // number of BufferedViewData holding refs to this

    ViewData()
        : rplRender(/* isShared */ false, /* useRefCounting */ false)
    {
    }

    ViewData(const ViewData& other) = delete;
    ViewData& operator=(const ViewData& other) = delete;

    ViewData(ViewData&& other) noexcept = delete;
    ViewData& operator=(ViewData&& other) noexcept = delete;

    HYP_FORCE_INLINE void AddRef()
    {
        ++numRefs;
    }

    HYP_FORCE_INLINE uint32 Release()
    {
        if (--numRefs == 0)
        {
            view->Release();
            view = nullptr;
        }

        return numRefs;
    }
};

static Map<View*, ViewData*> s_viewData;

static ViewData* GetViewData(View* view, bool createIfNotExist)
{
    if (createIfNotExist)
    {
        AssertOnThread(g_renderThread);
    }
    else
    {
        AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
    }

    const uint32 frameCounter = GetFrameCounter();

    AssertDebug(view != nullptr);

    auto viewDataIt = s_viewData.Find(view);

    if (viewDataIt == s_viewData.End())
    {
        if (!createIfNotExist)
        {
            return nullptr;
        }

        ENGINE_STAT_SCOPE(&s_statViewDataAllocTime);

        ViewData* viewData = HYP_POOL_NEW(g_renderPool, ViewData);
        viewData->view = view;
        viewData->lastUsedFrame = frameCounter;

        view->AddRef();

        HYP_LOG(Rendering, Verbose, "Allocating new ViewData {} for View {} at frame {}\t(Camera : {})",
                (void*)viewData,
                view->Id(),
                frameCounter,
                view->GetCamera() ? *view->GetCamera()->GetName() : "null");

        // If NO_PARALLEL_DRAW_CALL_COLLECTION flag is set, we need to make sure PARALLEL_COLLECTION is disabled on the group
        if (view->GetFlags() & ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION)
        {
            viewData->renderCollector.renderGroupFlags &= ~RenderGroupFlags::PARALLEL_COLLECTION;
        }

        if (view->GetViewDesc().entityBatchClass != nullptr)
        {
            viewData->renderCollector.batchAllocator = GetOrCreateEntityBatchAllocator(view->GetViewDesc().entityBatchClass->GetTypeId());
        }
        else
        {
            viewData->renderCollector.batchAllocator = GetOrCreateEntityBatchAllocator<MeshEntityInstanceBatch>();
        }

        AssertDebug(viewData->renderCollector.batchAllocator != nullptr);

        auto insertResult = s_viewData.Insert(view, viewData);
        AssertDebug(insertResult.second);

        return viewData;
    }

    ViewData& viewData = *viewDataIt->second;

    return &viewData;
}

// Data for views that is buffered over multiple frames
struct BufferedViewData
{
    View* view = nullptr;
    Viewport viewport {};
    RenderProxyList* rplShared = nullptr;

    // Only render thread touches this member, since ViewData is created from the render thread
    ViewData* viewData = nullptr;
};

struct RenderingData
{
    Map<View*, BufferedViewData*> perViewData;
    SharedMutex viewFrameDataMutex;

    FatArray<World*, InlineAllocator<2>> activeWorlds;

    FatArray<RenderProxyList*, InlineAllocator<16>> ownedLists; // render thread side owned lists
    FatArray<RenderProxyList*, InlineAllocator<16>> sharedLists;

    WorldShaderData worldBufferData {};

    /// Are producer, consumer threads synced for this frame?
    /// i.e have they waited on the appropriate semaphore
    uint8 threadSyncStates[2] = {};
    bool isFrameEnded = false; // Render thread only.
};

static RenderingData s_renderingData[RingBufferDepth];

struct SnapshotViewEntry
{
    View* view = nullptr;
    ViewData* viewData = nullptr;
    bool disableBuildRenderCollection = false;
};

/// The render thread's own copy of everything the sim produced for this frame.
/// Taken while the exclusive window is still held, so the sim can be let back in as soon as
/// the proxy copy is done rather than waiting for the whole frame to be submitted.
struct FrameSnapshot
{
    WorldShaderData worldBufferData {};
    FatArray<World*, InlineAllocator<2>> activeWorlds;
    FatArray<SnapshotViewEntry, InlineAllocator<16>> views;
};

static FrameSnapshot s_frameSnapshot;

static BufferedViewData* GetBufferedViewData(View* view, uint8 ringIndex)
{
    AssertDebug(view != nullptr);

    RenderingData& bufferedData = s_renderingData[ringIndex];

    TSharedLock<SharedMutex> sharedLock;
    TUniqueLock<SharedMutex> uniqueLock;

    // need to lock IFF on task thread
    // Sim/Render threads won't need this lock as they
    // do not operate on it at the same time as task threads do.
    if (Framework::t_thisThreadRingIndex == nullptr)
    {
        sharedLock.Reset(bufferedData.viewFrameDataMutex);
    }

    auto it = bufferedData.perViewData.Find(view);
    if (it != bufferedData.perViewData.End())
    {
        return it->second;
    }

    if (sharedLock)
    {
        sharedLock.Reset();
        uniqueLock.Reset(bufferedData.viewFrameDataMutex);

        it = bufferedData.perViewData.Find(view);

        if (it != bufferedData.perViewData.End())
        {
            return it->second;
        }
    }

    BufferedViewData* bufferedViewData = new BufferedViewData;
    bufferedViewData->view = view;

    bufferedViewData->rplShared = view->GetRenderProxyList(ringIndex);
    AssertDebug(bufferedViewData->rplShared != nullptr);
    AssertDebug(bufferedViewData->rplShared->isShared, "Expected isShared to be true to ensure multiple threads don't access the list concurrently");

    // Clear out any lingering tracked resources.
    // bufferedViewData->rplShared->BeginWrite();
    // bufferedViewData->rplShared->ClearAll();
    // bufferedViewData->rplShared->EndWrite();

    // AssertDebug(bufferedViewData->rplShared->GetMeshEntities().NumCurrent() == 0);

    bufferedData.perViewData[view] = bufferedViewData;

    return bufferedViewData;
}

} // namespace Framework

uint32 GetRingIndex()
{
    if (HYP_UNLIKELY(!Framework::t_thisThreadRingIndex))
    {
        const int threadType = Framework::CurrentThreadType();
        Assert(threadType >= 0, "GetRingIndex called from an invalid thread!");

        Framework::t_thisThreadRingIndex = &Framework::s_ringIndex[threadType];
    }

    return *Framework::t_thisThreadRingIndex;
}

uint32 GetFrameCounter()
{
    return (uint32)AtomicAdd(&Framework::s_frameCounter, 0);
}

RenderProxyList& GetProducerProxyList(View* view)
{
    // can be called on sim thread or on task thread for tasks enqueued and awaited by sim thread, **exclusively**
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);

    Framework::BufferedViewData* vd = Framework::GetBufferedViewData(view, Framework::s_ringIndex[Framework::TT_FrameDataProducer]);
    Assert(vd != nullptr);

    return *vd->rplShared;
}

RenderProxyList& GetConsumerProxyList(View* view)
{
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertDebug(view != nullptr);

    Framework::ViewData* vd = Framework::GetViewData(view, false);

    if (vd == nullptr)
    {
        static RenderProxyList s_fallbackRpl { /* isShared */ false, /* useRefCounting */ false };
        return s_fallbackRpl;
    }

    return vd->rplRender;
}

RenderCollector& GetRenderCollector(View* view)
{
    AssertOnThread(g_renderThread);

    Framework::ViewData* vd = Framework::GetViewData(view, false);

    if (vd == nullptr)
    {
        static RenderCollector s_fallbackRenderCollector;
        s_fallbackRenderCollector.isFallback = true;

        return s_fallbackRenderCollector;
    }

    return vd->renderCollector;
}

IRenderProxy* GetRenderProxy(const void* resource)
{
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertDebug(resource != nullptr);

    if (!resource)
    {
        return nullptr;
    }

    const ObjectBase* resourceCasted = static_cast<const ObjectBase*>(resource);

    ResourceSubtypeData& subtypeData = RI.resources->GetSubtypeData(resourceCasted->InstanceClass());
    AssertDebug(subtypeData.hasProxyData,
                "Cannot use GetRenderProxy() for type which does not have a RenderProxy! Type name: {}",
                subtypeData.typeInfo->name);

    const ObjIdBase resourceId = resourceCasted->Id();
    AssertDebug(resourceId.GetTypeId() == subtypeData.typeInfo->id);

    if (!subtypeData.proxies.HasIndex(resourceId.ToIndex()))
    {
        HYP_LOG(Rendering, Warning, "No render proxy found for resource: {}", resourceId);

        return nullptr; // no proxy for this resource
    }

    const IRenderProxy* proxy = reinterpret_cast<const IRenderProxy*>(subtypeData.proxies.GetElementRaw(resourceId.ToIndex()));
    AssertDebug(proxy != nullptr);

    return const_cast<IRenderProxy*>(proxy);
}

void UpdateGpuData(const void* resource)
{
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    const ObjectBase* resourceCasted = static_cast<const ObjectBase*>(resource);

    const ObjIdBase resourceId = resourceCasted->Id();

    ResourceSubtypeData& subtypeData = RI.resources->GetSubtypeData(resourceCasted->InstanceClass());
    AssertDebug(resourceId.GetTypeId() == subtypeData.typeInfo->id);

    AssertDebug(subtypeData.sbuffer != nullptr,
                "Cannot update GPU data for type which does not have a buffer! Type: {}",
                subtypeData.typeInfo->name);

    AssertDebug(subtypeData.hasProxyData,
                "Cannot use UpdateGpuData() for type which does not have a RenderProxy! Type: {}",
                subtypeData.typeInfo->name);

    const uint32 bindingIndex = GetBinding(resource);
    AssertDebug(bindingIndex != ~0u);

    const uint32 idx = resourceId.ToIndex();

    const IRenderProxy* proxy = reinterpret_cast<const IRenderProxy*>(subtypeData.proxies.GetElementRaw(idx));
    AssertDebug(proxy != nullptr);

    subtypeData.SetGpuElem(bindingIndex, const_cast<IRenderProxy*>(proxy));

    // set it as no longer needing update next frame since we updated immediately
    subtypeData.indicesPendingUpdate.Set(idx, false);
}

void SetForceRebind(ObjectBase* resource, bool forceRebind)
{
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    ResourceSubtypeData& subtypeData = RI.resources->GetSubtypeData(resource->InstanceClass());

    for (ResourceBinderBase** it = subtypeData.resourceBinders; *it; ++it)
    {
        ResourceBinderBase* resourceBinder = *it;
        resourceBinder->SetForceRebind(resource, forceRebind);
    }
}

WorldShaderData* GetWorldBufferData()
{
    AssertOnThread(g_simThread | g_renderThread);

    if (IsOnThread(g_renderThread))
    {
        return &Framework::s_frameSnapshot.worldBufferData;
    }

    return &Framework::s_renderingData[*Framework::t_thisThreadRingIndex].worldBufferData;
}

void CommitActiveWorlds(Span<World*> activeWorlds)
{
    AssertOnThread(g_simThread);

    Framework::RenderingData& bufferedData = Framework::s_renderingData[Framework::s_ringIndex[Framework::TT_FrameDataProducer]];
    AssertDebug(bufferedData.threadSyncStates[Framework::TT_FrameDataProducer]);

    bufferedData.activeWorlds.Resize(activeWorlds.Size());
    std::copy(activeWorlds.Begin(), activeWorlds.End(), bufferedData.activeWorlds.Begin());
}

Span<World*> GetActiveWorlds()
{
    AssertOnThread(g_simThread | g_renderThread);

    if (IsOnThread(g_renderThread))
    {
        return Framework::s_frameSnapshot.activeWorlds.ToSpan();
    }

    Framework::RenderingData& bufferedData = Framework::s_renderingData[*Framework::t_thisThreadRingIndex];
    AssertDebug(bufferedData.threadSyncStates[Framework::TT_FrameDataProducer]);

    return bufferedData.activeWorlds.ToSpan();
}

uint32 CurrentRenderThreadIndex()
{
    if (Framework::t_currentRenderThreadIndex == 0)
    {
        Framework::t_currentRenderThreadIndex = 1 + (IsOnThread(g_renderThread) ? 0 : GetCurrentThreadIndex() + 1);
    }

    return Framework::t_currentRenderThreadIndex - 1;
}

void BeginSimRenderSyncBlock(AtomicFlag* pCancelFlag)
{
    Framework::t_thisThreadRingIndex = &Framework::s_ringIndex[Framework::TT_FrameDataProducer];

    {
        ENGINE_STAT_SCOPE(&g_statSimThreadSync);
        ENGINE_STAT_SCOPE(&g_statTotalStallTime);

        while (!Framework::s_frameSubmitted.try_acquire_for(std::chrono::milliseconds(100)))
        {
            if (pCancelFlag != nullptr && pCancelFlag->Load())
            {
                return;
            }
        }
    }

    CVarManager::GetInstance().Publish(Framework::s_ringIndex[Framework::TT_FrameDataProducer]);

    Framework::s_simCommitWindowStart.Start();

    Framework::RenderingData& bufferedData = Framework::s_renderingData[*Framework::t_thisThreadRingIndex];
    bufferedData.threadSyncStates[Framework::TT_FrameDataProducer] = 1;
}

void EndSimRenderSyncBlock()
{
    AssertOnThread(g_simThread);

    const uint8 ringIndex = Framework::s_ringIndex[Framework::TT_FrameDataProducer];

    s_statSimCommitWindow.RecordElapsedMs(static_cast<float>(Framework::s_simCommitWindowStart.ElapsedMs()), /* accum */ false);

    Framework::RenderingData& bufferedData = Framework::s_renderingData[*Framework::t_thisThreadRingIndex];
    bufferedData.threadSyncStates[Framework::TT_FrameDataProducer] = 0;

    Framework::s_ringIndex[Framework::TT_FrameDataProducer] = (ringIndex + 1) % RingBufferDepth;
    Framework::s_dataProduced.release();
}

void CheckCurrentThreadSynced()
{
    AssertOnThread(g_simThread | g_renderThread);
    
    const int threadType = Framework::CurrentThreadType();
    AssertDebug(threadType >= 0, "AssertCurrentThreadSynced called from an invalid thread!");
    
    Framework::RenderingData& bufferedData = Framework::s_renderingData[*Framework::t_thisThreadRingIndex];
    AssertDebug(bufferedData.threadSyncStates[threadType] == 1);
}

#pragma region RenderInterface

RenderInterface::RenderInterface()
    : cbufferAllocator(nullptr),
      bufferAllocator(nullptr),
      scratchImageAllocator(nullptr),
      shaderManager(nullptr),
      bindlessStorage(nullptr),
      placeholderData(nullptr),
      materialTextureCache(nullptr),
      graphicsPipelineCache(nullptr),
      computePipelineCache(nullptr),
      rayTracingPipelineCache(nullptr),
      descriptorSetCache(nullptr),
      renderGroupCache(nullptr),
      textureViewCache(nullptr),
      samplerCache(nullptr),
      blasCache(nullptr),
      shadowMapCache(nullptr),
      finalPass(nullptr),
      stagingBufferPool(nullptr),
      m_gpuTimerBackend(nullptr)
{
    Assert(g_renderPool == nullptr);

    g_renderPool = new Pool(RenderPoolBlockSize);
    g_renderArena = new Arena(RenderArenaSize);

    Assert(g_rhiPool == nullptr);
    g_rhiPool = new Pool(RHIPoolBlockSize);
    g_rhiArena = new Arena(RHIArenaSize);
}

RenderInterface::~RenderInterface()
{
    delete g_renderPool;
    g_renderPool = nullptr;

    delete g_rhiPool;
    g_rhiPool = nullptr;
}

RendererResult RenderInterface::Initialize()
{
    HYP_LOG(Rendering, Verbose, "Initializing base render interface");

    Framework::t_thisThreadRingIndex = &Framework::s_ringIndex[Framework::TT_FrameDataConsumer];

    cbufferAllocator = PoolNew<CBufferAllocator>(*g_renderPool);
    bufferAllocator = PoolNew<BufferAllocator>(*g_renderPool);
    scratchImageAllocator = PoolNew<ScratchImageAllocator>(*g_renderPool);
    descriptorSetCache = PoolNew<DescriptorSetCache>(*g_renderPool);
    placeholderData = PoolNew<PlaceholderData>(*g_renderPool);
    materialTextureCache = PoolNew<MaterialTextureCache>(*g_renderPool);
    graphicsPipelineCache = PoolNew<GraphicsPipelineCache>(*g_renderPool);
    computePipelineCache = PoolNew<ComputePipelineCache>(*g_renderPool);
    rayTracingPipelineCache = PoolNew<RayTracingPipelineCache>(*g_renderPool);
    renderGroupCache = PoolNew<RenderGroupCache>(*g_renderPool);
    bindlessStorage = PoolNew<BindlessStorage>(*g_renderPool);
    shaderManager = PoolNew<ShaderManager>(*g_renderPool);
    textureViewCache = PoolNew<TextureViewCache>(*g_renderPool);
    samplerCache = PoolNew<SamplerCache>(*g_renderPool);
    blasCache = PoolNew<BLASCache>(*g_renderPool);
    shadowMapCache = PoolNew<ShadowMapCache>(*g_renderPool);
    stagingBufferPool = PoolNew<StagingBufferPool>(*g_renderPool);

    InitDeviceDetails(deviceDetails);

    namedBuffers[NamedBuffer::Worlds] = StructuredBuffer(MaxBoundWorlds, sizeof(WorldShaderData));
    namedBuffers[NamedBuffer::Cameras] = StructuredBuffer(MaxBoundCameras, sizeof(CameraShaderData));
    namedBuffers[NamedBuffer::Lights] = StructuredBuffer(MaxBoundLights, sizeof(LightShaderData));
    namedBuffers[NamedBuffer::Entities] = StructuredBuffer(MaxBoundEntities, sizeof(EntityShaderData));
    namedBuffers[NamedBuffer::Materials] = StructuredBuffer(MaxBoundMaterials, sizeof(MaterialShaderData));
    namedBuffers[NamedBuffer::Skeletons] = StructuredBuffer(MaxBoundSkeletons, sizeof(SkeletonShaderData));
    namedBuffers[NamedBuffer::EnvProbes] = StructuredBuffer(MaxBoundEnvProbes, sizeof(EnvProbeShaderData));
    namedBuffers[NamedBuffer::ProbeVolumes] = StructuredBuffer(MaxBoundProbeVolumes, sizeof(ProbeVolumeShaderData));
    namedBuffers[NamedBuffer::LightmapVolumes] = StructuredBuffer(MaxBoundLightmapVolumes, sizeof(LightmapVolumeShaderData));

    for (uint8 namedBufferIndex = 0; namedBufferIndex < NamedBuffer::Max; namedBufferIndex++)
    {
#if HYP_DEBUG_MODE
        HYP_LOG(Rendering, Verbose, "Initializing named buffer: {}", NamedBuffer::StringValues[namedBufferIndex]);
#endif

        StructuredBuffer& sbuffer = namedBuffers[namedBufferIndex];

        if (!sbuffer.cpuBuffer.Empty())
        {
            sbuffer.Initialize();

#ifdef HYP_RHI_DEBUG_NAMES
            AssertDebug(sbuffer.gpuBuffer != nullptr);
            sbuffer.gpuBuffer->SetDebugName(CreateNameFromDynamicString(NamedBuffer::StringValues[namedBufferIndex]));
#endif // HYP_DEBUG_MODE
        }
    }

    resources = PoolNew<ResourceContainer>(*g_renderPool);

    for (ResourceBinderBase* resourceBinder : s_resourceBinders)
    {
        resourceBinder->Initialize();
    }

    {
        EngineConfig cfg;
        cfg.Load();

        bool shouldDisableRayTracing = !GetRenderConfig().rayTracing;

        // if ray tracing is not supported, we need to update the configuration
        if (shouldDisableRayTracing)
        {
            cfg.Set("Rendering.RayTracingEnabled", false);
            cfg.Set("Rendering.RayTracedReflections", false);
            cfg.Set("Rendering.RayTracedGI", false);
            cfg.Set("Rendering.PathTracing", false);
        }

        if (cfg.IsChanged())
        {
            cfg.Save();
        }

        // Reinitialize the CVars based on the config.
        CVarManager::GetInstance().InitFromConfig(cfg);
    }

    finalPass = PoolNew<FinalPass>(*g_renderPool);
    finalPass->Create();

    ResourceContainerFactoryRegistry& registry = ResourceContainerFactoryRegistry::GetInstance();
    registry.InvokeAll(*resources);

    registry.funcs.Clear();

    globalDescriptorTable = MakeDescriptorTable(&GetStaticDescriptorTableDeclaration());

    placeholderData->Initialize();
    shadowMapCache->Initialize();

    DeletionQueue::GetInstance().Initialize();
    DebugDrawer::GetInstance().Initialize();

    CreateSphereSamplesBuffer();
    CreateBlueNoiseBuffer();

    CreateEnvProbesColorTexture();
    CreateEnvProbesDepthTexture();

    CheckResultOrReturn(globalDescriptorTable->Create());

    for (uint8 i = 0; i < NumNamedPasses; i++)
    {
        namedPasses[i] = Array<PassBase*, RenderAllocator>();
    }

    namedPasses[NamedPass::Deferred].PushBack(new DeferredPass);
    namedPasses[NamedPass::Deferred][0]->Initialize();

    namedPasses[NamedPass::UI].PushBack(new UIPass);
    namedPasses[NamedPass::UI][0]->Initialize();

    namedPasses[NamedPass::EnvProbe].ResizeZeroed(EPT_MAX);
    namedPasses[NamedPass::EnvProbe][EPT_REFLECTION] = new ReflectionProbePass;
    namedPasses[NamedPass::EnvProbe][EPT_SKY] = new ReflectionProbePass;
    namedPasses[NamedPass::EnvProbe][EPT_AMBIENT] = new IrradianceProbePass;

    namedPasses[NamedPass::ShadowMap].ResizeZeroed(NumLightTypes); // 1 ShadowMapRenderer per LightType
    namedPasses[NamedPass::ShadowMap][uint32(LightType::Point)] = new PointLightShadowsPass;
    namedPasses[NamedPass::ShadowMap][uint32(LightType::Directional)] = new DirectionalLightShadowsPass;

    // one global particle volume renderer
    namedPasses[NamedPass::ParticleVolume].ResizeZeroed(1);
    namedPasses[NamedPass::ParticleVolume][0] = new ParticlesPass;

    namedPasses[NamedPass::Sprite].ResizeZeroed(1);
    namedPasses[NamedPass::Sprite][0] = new SpritePass;
    namedPasses[NamedPass::Sprite][0]->Initialize();

    return {};
}

void RenderInterface::Shutdown()
{
    deferredFlushBuffers.Clear();

    for (uint32 i = 0; i < RingBufferDepth; i++)
    {
        for (auto& it : Framework::s_renderingData[i].perViewData)
        {
            delete it.second;
        }

        Framework::s_renderingData[i].perViewData.Clear();
    }

    for (auto& it : Framework::s_viewData)
    {
        Framework::ViewData*& vd = it.second;

        if (!vd)
        {
            continue;
        }

        PoolDelete(*g_renderPool, vd);
        vd = nullptr;
    }

    Framework::s_viewData.Clear();

    for (ResourceBinderBase* resourceBinder : s_resourceBinders)
    {
        resourceBinder->Shutdown();
    }

    ClearSubtypeBindings();

    commandRecorderAllocator.Shutdown();

    PoolDelete(*g_renderPool, resources);
    resources = nullptr;

    bindlessStorage->UnsetAllResources(BindlessStorage_Textures);
    bindlessStorage->UnsetAllResources(BindlessStorage_Buffers);

    PoolDelete(*g_renderPool, bindlessStorage);
    bindlessStorage = nullptr;

    for (uint8 i = 0; i < NumNamedPasses; i++)
    {
        for (uint32 j = 0; j < namedPasses[i].Size(); j++)
        {
            if (namedPasses[i][j])
            {
                namedPasses[i][j]->Shutdown();
                delete namedPasses[i][j];
            }
        }

        namedPasses[i].Clear();
    }

    DebugDrawer::GetInstance().Shutdown();

    for (StructuredBuffer& structuredBuffer : namedBuffers)
    {
        structuredBuffer.Shutdown();
    }

    blueNoiseBuffer.Shutdown();
    sphereSamplesBuffer.Shutdown();

    envProbesColorTexture.Reset();
    envProbesDepthTexture.Reset();

    shadowMapCache->Shutdown();
    placeholderData->Shutdown();

    globalDescriptorTable.Reset();

    CrashHandler::Shutdown();

    PoolDelete(*g_renderPool, shaderManager);
    shaderManager = nullptr;

    PoolDelete(*g_renderPool, blasCache);
    blasCache = nullptr;

    PoolDelete(*g_renderPool, shadowMapCache);
    shadowMapCache = nullptr;

    PoolDelete(*g_renderPool, stagingBufferPool);
    stagingBufferPool = nullptr;

    PoolDelete(*g_renderPool, textureViewCache);
    textureViewCache = nullptr;

    PoolDelete(*g_renderPool, samplerCache);
    samplerCache = nullptr;

    PoolDelete(*g_renderPool, finalPass);
    finalPass = nullptr;

    PoolDelete(*g_renderPool, cbufferAllocator);
    cbufferAllocator = nullptr;

    PoolDelete(*g_renderPool, bufferAllocator);
    bufferAllocator = nullptr;

    PoolDelete(*g_renderPool, scratchImageAllocator);
    scratchImageAllocator = nullptr;

    PoolDelete(*g_renderPool, descriptorSetCache);
    descriptorSetCache = nullptr;

    PoolDelete(*g_renderPool, placeholderData);
    placeholderData = nullptr;

    PoolDelete(*g_renderPool, materialTextureCache);
    materialTextureCache = nullptr;

    PoolDelete(*g_renderPool, graphicsPipelineCache);
    graphicsPipelineCache = nullptr;

    PoolDelete(*g_renderPool, computePipelineCache);
    computePipelineCache = nullptr;

    PoolDelete(*g_renderPool, rayTracingPipelineCache);
    rayTracingPipelineCache = nullptr;

    PoolDelete(*g_renderPool, renderGroupCache);
    renderGroupCache = nullptr;

    DeletionQueue::GetInstance().Shutdown();
}

void RenderInterface::BeginFrame(AtomicFlag* pCancelFlag)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if constexpr (UseRingBuffer)
    {
        if (!WaitForSync(pCancelFlag))
        {
            return;
        }
    }
    
    const uint8 ringIndex = Framework::s_ringIndex[Framework::TT_FrameDataConsumer];
    
    Framework::RenderingData& bufferedData = Framework::s_renderingData[ringIndex];
    bufferedData.isFrameEnded = false;

    const uint32 newFrameIndex = GetFrameCounter();

    PrepareFrame(GetCurrentFrame());
    
    g_engineStats->Prepare();

    cbufferAllocator->OnFrameStart(newFrameIndex);
    bufferAllocator->OnFrameStart(newFrameIndex);
    scratchImageAllocator->OnFrameStart(newFrameIndex);
    descriptorSetCache->OnFrameStart(newFrameIndex);
    stagingBufferPool->OnFrameStart(newFrameIndex);

    RenderCommands::Flush();

    UpdateResources(UseRingBuffer ? nullptr : pCancelFlag);
}

void RenderInterface::EndFrame()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint8 ringIndex = Framework::s_ringIndex[Framework::TT_FrameDataConsumer];

    Framework::RenderingData& bufferedData = Framework::s_renderingData[ringIndex];
    bufferedData.isFrameEnded = true;

    const uint32 prevFrameIndex = AtomicIncrement(&Framework::s_frameCounter) - 1;

    stagingBufferPool->OnFrameEnd(prevFrameIndex);

    for (uint8 i = 0; i < NumNamedPasses; i++)
    {
        for (size_t j = 0; j < namedPasses[i].Size(); j++)
        {
            if (PassBase* pass = namedPasses[i][j])
            {
                pass->OnFrameEnd(prevFrameIndex);
            }
        }
    }

    graphicsPipelineCache->OnFrameEnd(prevFrameIndex);
    computePipelineCache->OnFrameEnd(prevFrameIndex);
    rayTracingPipelineCache->OnFrameEnd(prevFrameIndex);

    for (ResourceSubtypeData& subtypeData : resources->dataByType)
    {
        for (Bitset::BitIndex i : subtypeData.indicesPendingDelete)
        {
            ResourceData& rd = subtypeData.data.Get(i);
            AssertDebug(rd.resource != nullptr);
            AssertDebug(rd.useCount == 0, "Use count should be 0 before deletion");

            // if we delete it, we want to make sure it is not in marked for update state (don't want to iterate over
            // dead items)
            subtypeData.indicesPendingUpdate.Set(i, false);

            for (ResourceBinderBase** it = subtypeData.resourceBinders; *it; ++it)
            {
                ResourceBinderBase* resourceBinder = *it;
                resourceBinder->Deconsider(rd.resource);
            }

            subtypeData.data.EraseAt(i);

            if (subtypeData.hasProxyData)
            {
                AssertDebug(subtypeData.proxies.HasIndex(i));

                const IRenderProxy* pProxy = reinterpret_cast<const IRenderProxy*>(subtypeData.proxies.GetElementRaw(i));
                AssertDebug(pProxy != nullptr);

                subtypeData.proxies.DeleteElementRaw(i, subtypeData.proxyDtor);
            }
        }

        subtypeData.indicesPendingDelete.Clear();
    }

    blasCache->OnFrameEnd(prevFrameIndex);

    DeletionQueue::GetInstance().UpdateEntryListQueue();
    DeletionQueue::GetInstance().OnFrameEnd(prevFrameIndex);

    cbufferAllocator->OnFrameEnd(prevFrameIndex);
    bufferAllocator->OnFrameEnd(prevFrameIndex);
    scratchImageAllocator->OnFrameEnd(prevFrameIndex);
    descriptorSetCache->OnFrameEnd(prevFrameIndex);
    stagingBufferPool->OnFrameEnd(prevFrameIndex);

    textureViewCache->OnFrameEnd(prevFrameIndex);

    const uint32 nextFrameIndex = (Framework::s_ringIndex[Framework::TT_FrameDataConsumer] + 1) % RingBufferDepth;
    Framework::s_ringIndex[Framework::TT_FrameDataConsumer] = static_cast<uint8>(nextFrameIndex);

    if constexpr (UseRingBuffer)
    { // Let simulation thread back in
        s_statRenderExclusiveWindow.RecordElapsedMs(static_cast<float>(Framework::s_renderExclusiveWindowStart.ElapsedMs()));
        
        g_engineStats->Publish();

        bufferedData.threadSyncStates[Framework::TT_FrameDataConsumer] = 0;
        Framework::s_frameSubmitted.release();
    }
    
    ReleaseTransientMemory();

    state.Reset();
}

HYP_NODISCARD bool RenderInterface::WaitForSync(AtomicFlag* pCancelFlag)
{
    ENGINE_STAT_SCOPE(&g_statRenderThreadSync);
    ENGINE_STAT_SCOPE(&g_statTotalStallTime);

    while (!Framework::s_dataProduced.try_acquire_for(std::chrono::milliseconds(100)))
    {
        if (pCancelFlag != nullptr && pCancelFlag->Load())
        {
            return false;
        }
    }

    return true;
}

void RenderInterface::UpdateResources(AtomicFlag* pCancelFlag)
{
    if constexpr (!UseRingBuffer)
    {
        if (!WaitForSync(pCancelFlag))
        {
            return;
        }
    }

    Framework::s_renderExclusiveWindowStart.Start();

    const uint8 ringIndex = Framework::s_ringIndex[Framework::TT_FrameDataConsumer];
    const uint32 currFrame = GetFrameCounter();

    Framework::RenderingData& bufferedData = Framework::s_renderingData[ringIndex];
    bufferedData.threadSyncStates[Framework::TT_FrameDataConsumer] = 1;

    Span<View* const> activeViews = g_engineDriver->GetCurrentFrameViews();

    for (View* view : activeViews)
    {
        // ensure BufferedViewData exists
        Framework::BufferedViewData& bufferedViewData = *Framework::GetBufferedViewData(view, ringIndex);
        AssertDebug(bufferedViewData.rplShared != nullptr);

        if (!bufferedViewData.viewData)
        {
            bufferedViewData.viewData = Framework::GetViewData(view, /* createIfNotExist */ true);
            AssertDebug(bufferedViewData.viewData != nullptr);

            bufferedViewData.viewData->AddRef();

            AssertDebug(bufferedViewData.rplShared != nullptr);

            bufferedData.ownedLists.PushBack(&bufferedViewData.viewData->rplRender);
            bufferedData.sharedLists.PushBack(bufferedViewData.rplShared);
        }

        bufferedViewData.viewData->lastUsedFrame = currFrame;
    }

    // copy deps to render side owned lists
    AssertDebug(bufferedData.ownedLists.Size() == bufferedData.sharedLists.Size());

    {
        ENGINE_STAT_SCOPE(&s_statCopyDependencies);

        for (size_t i = 0; i < bufferedData.ownedLists.Size(); i++)
        {
            RenderProxyList* rplRender = bufferedData.ownedLists[i];
            AssertDebug(rplRender != nullptr);

            RenderProxyList* rplShared = bufferedData.sharedLists[i];

            // Advance render side owned lists ResourceTrackers before we copy dependencies over
            int resourceTrackerIndex = 0;
            StaticForEach<typename RenderProxyList::ResourceTrackerTypes>(
                [rplRender, &resourceTrackerIndex]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>)
                {
                    ResourceTrackerType& resourceTracker = static_cast<ResourceTrackerType&>(*rplRender->resourceTrackers[resourceTrackerIndex]);
                    resourceTracker.Advance();

                    ++resourceTrackerIndex;
                });

            if (!rplShared)
            {
                static RenderProxyList s_defaultRenderProxyList { /* isShared */ false, /* useRefCounting */ false };
                rplShared = &s_defaultRenderProxyList;
            }

            bool readLockAcquired = false;
            rplShared->BeginRead(&readLockAcquired);

            if (!readLockAcquired)
            {
                HYP_LOG(Rendering, Warning, "Read lock for RenderProxyList could not be acquired, may result in invalid resource bindings or stale pointers!!!");

                continue;
            }

            // copy dependencies from shared to ViewData
            CopyDependencies(*resources, *rplRender, *rplShared);

            rplShared->EndRead();
        }
    }

    // All dependencies has now been copied into render thread owned storage.
    // Other data needed on the render thread needs to be copied still for !UseRingBuffer mode to work.
    {
        HYP_NAMED_SCOPE("Snapshot frame data");

        Framework::FrameSnapshot& snapshot = Framework::s_frameSnapshot;

        snapshot.worldBufferData = bufferedData.worldBufferData;

        snapshot.activeWorlds.Resize(bufferedData.activeWorlds.Size());
        std::copy(bufferedData.activeWorlds.Begin(), bufferedData.activeWorlds.End(), snapshot.activeWorlds.Begin());

        for (World* world : snapshot.activeWorlds)
        {
            world->SnapshotViewsForRender();
        }

        snapshot.views.Resize(0);
        snapshot.views.Reserve(activeViews.Size());

        for (View* view : activeViews)
        {
            Framework::BufferedViewData& bufferedViewData = *Framework::GetBufferedViewData(view, ringIndex);
            AssertDebug(bufferedViewData.rplShared != nullptr);
            AssertDebug(bufferedViewData.viewData != nullptr);

            Framework::SnapshotViewEntry entry;
            entry.view = view;
            entry.viewData = bufferedViewData.viewData;
            entry.disableBuildRenderCollection = bufferedViewData.rplShared->disableBuildRenderCollection;

            snapshot.views.PushBack(entry);
        }

        DebugDrawer::GetInstance().AcquireRenderCommands();

        // Discards ViewData for views that went away. This mutates perViewData and deletes BufferedViewData,
        // which the sim reaches through when collecting, so has to happen before we let sim thread back in
        CleanupUnusedResources(currFrame);
    }

    if constexpr (!UseRingBuffer)
    { // Let sim thread back in
        s_statRenderExclusiveWindow.RecordElapsedMs(static_cast<float>(Framework::s_renderExclusiveWindowStart.ElapsedMs()));
        
        g_engineStats->Publish();

        bufferedData.threadSyncStates[Framework::TT_FrameDataConsumer] = 0;
        Framework::s_frameSubmitted.release();
    }

    {
        HYP_NAMED_SCOPE("Resource bindings - select candidates");
        ENGINE_STAT_SCOPE(&s_statResourceBindings);

        for (ResourceSubtypeData& subtypeData : resources->dataByType)
        {
            for (ResourceData& elem : subtypeData.data)
            {
                AssertDebug(elem.resource != nullptr);

                bool forceRebind = false;
                IRenderProxy* pProxy = nullptr;

                if (subtypeData.hasProxyData && subtypeData.indicesPendingUpdate.Test(elem.resource->Id().ToIndex()))
                {
                    pProxy = const_cast<IRenderProxy*>(reinterpret_cast<const IRenderProxy*>(subtypeData.proxies.GetElementRaw(elem.resource->Id().ToIndex())));
                    AssertDebug(pProxy != nullptr);

                    forceRebind = pProxy->forceRebind;
                    pProxy->forceRebind = false;
                }

                for (ResourceBinderBase** it = subtypeData.resourceBinders; *it; ++it)
                {
                    ResourceBinderBase* resourceBinder = *it;
                    resourceBinder->Consider(elem.resource, forceRebind);
                }
            }
        }
    }

    // assign the actual bindings:
    for (ResourceBinderBase* resourceBinder : s_resourceBinders)
    {
        resourceBinder->ApplyUpdates();
    }

    TBitset<RenderAllocator> currentBoundIndices;

    for (ResourceSubtypeData& subtypeData : resources->dataByType)
    {
        if (subtypeData.indicesPendingUpdate.Count() != 0)
        {
            currentBoundIndices.Clear();

            for (ResourceBinderBase** it = subtypeData.resourceBinders; *it; ++it)
            {
                ResourceBinderBase* resourceBinder = *it;
                currentBoundIndices |= resourceBinder->GetBoundIndices(subtypeData.typeInfo->id);
            }

            if (currentBoundIndices.Count() == 0)
            {
                // nothing is bound for this type, skip
                continue;
            }

            if (!subtypeData.sbuffer)
            {
                // in the loop below we only do anything if we have gpu data to update.
                // short circuit here and just clear the bits without doing anything if we don't have a gpu buffer holder set.
                subtypeData.indicesPendingUpdate.Clear();

                continue;
            }

            // Handle proxies that were updated on sim thread
            for (Bitset::BitIndex i = subtypeData.indicesPendingUpdate.FirstSetBitIndex();
                 i != Bitset::NotFound;
                 i = subtypeData.indicesPendingUpdate.NextSetBitIndex(i + 1))
            {
                if (!currentBoundIndices.Test(i))
                {
                    continue;
                }

                ObjectBase* resource = subtypeData.data.Get(i).resource;

                AssertDebug(subtypeData.hasProxyData);

                const uint32 bindingIndex = GetBinding(resource);
                AssertDebug(bindingIndex != ~0u,
                            "Failed to retrieve binding for resource: {} in frame {}, but it is marked as bound (index: {})",
                            i, ringIndex, i);

                const IRenderProxy* pProxy = reinterpret_cast<const IRenderProxy*>(subtypeData.proxies.GetElementRaw(i));
                AssertDebug(pProxy != nullptr);

                subtypeData.SetGpuElem(bindingIndex, const_cast<IRenderProxy*>(pProxy));
                subtypeData.indicesPendingUpdate.Set(i, false);
            }
        }
    }

    // flush structured buffer data writes now that the data has been written cpu-side
    FlushStructuredBuffers();

    // Build draw call lists
    {
        ENGINE_STAT_SCOPE(&s_statBuildDrawCalls);

        for (const Framework::SnapshotViewEntry& entry : Framework::s_frameSnapshot.views)
        {
            Framework::ViewData& vd = *entry.viewData;

            if (entry.disableBuildRenderCollection || (entry.view->GetFlags() & ViewFlags::NO_DRAW_CALLS))
            {
                continue;
            }

            vd.rplRender.BeginRead();

            vd.renderCollector.BuildRenderGroups(vd.view, vd.rplRender);
            vd.renderCollector.CollectRenderables(0);

            vd.rplRender.EndRead();
        }
    }

    GetCurrentCommandBuffer()->Begin();

    if (m_gpuTimerBackend != nullptr)
    {
        m_gpuTimerBackend->OnFrameStart();
        m_gpuTimerBackend->WriteStartTimestamp(GetCurrentCommandBuffer(), &g_statGpuFrameTime);
    }
}

void RenderInterface::CleanupUnusedResources(uint32 frameIndex)
{
    AssertOnThread(g_renderThread);

    const uint8 ringIndex = Framework::s_ringIndex[Framework::TT_FrameDataConsumer];
    
    Framework::RenderingData& bufferedData = Framework::s_renderingData[ringIndex];

    for (auto it = bufferedData.perViewData.Begin(); it != bufferedData.perViewData.End();)
    {
        Framework::BufferedViewData* bufferedViewData = it->second;

        if (bufferedViewData->viewData != nullptr)
        {
            Framework::ViewData* viewData = bufferedViewData->viewData;

            View* view = viewData->view;
            AssertDebug(view != nullptr);

            viewData->renderCollector.RemoveEmptyRenderGroups();

            // Clear out data for views that haven't been written to for a while
            if (static_cast<int64>(frameIndex) - static_cast<int64>(viewData->lastUsedFrame) >= MaxFramesBeforeDiscard)
            {
                // Decrement ref count on the ViewData,
                // if we hit zero there are no more BufferedViewData holding refs to the ViewData so we delete it
                AssertDebug(viewData->numRefs > 0);

                auto rplRenderIt = bufferedData.ownedLists.Find(&viewData->rplRender);
                AssertDebug(rplRenderIt != bufferedData.ownedLists.End());

                const size_t rplIndex = std::distance(bufferedData.ownedLists.Begin(), rplRenderIt);

                // Drain all tracked resources from the render-side list before discarding,
                // so their useCount is properly decremented in ResourceSubtypeData.
                // Without this, resources that were tracked in rplRender would remain in
                // ResourceSubtypeData with useCount > 0, leaking into the ResourceBinder indefinitely.
                {
                    int resourceTrackerIndex = 0;
                    StaticForEach<typename RenderProxyList::ResourceTrackerTypes>(
                        [&viewData, &resourceTrackerIndex]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>)
                        {
                            ResourceTrackerType& resourceTracker = static_cast<ResourceTrackerType&>(*viewData->rplRender.resourceTrackers[resourceTrackerIndex]);
                            resourceTracker.Advance();

                            ++resourceTrackerIndex;
                        });

                    static RenderProxyList s_emptyRpl { /* isShared */ false, /* useRefCounting */ false };
                    CopyDependencies(*resources, viewData->rplRender, s_emptyRpl);
                }

                bufferedData.ownedLists.Erase(rplRenderIt);

                AssertDebug(rplIndex < bufferedData.sharedLists.Size());
                bufferedData.sharedLists.EraseAt(rplIndex);

                if (viewData->Release() == 0)
                {
                    HYP_LOG(Rendering, Verbose, "Discarding ViewData {} for view {} at frame {}", (void*)viewData, view->Id(), GetFrameCounter());

                    auto viewDataIt = Framework::s_viewData.Find(view);
                    AssertDebug(viewDataIt != Framework::s_viewData.End() && viewDataIt->second == viewData);

                    Framework::s_viewData.Erase(viewDataIt);

                    PoolDelete(*g_renderPool, viewData);
                }

                bufferedViewData->viewData = nullptr;

                delete bufferedViewData;

                it = bufferedData.perViewData.Erase(it);

                continue;
            }
        }

        ++it;
    }
}

void RenderInterface::WriteCommandBuffer()
{
    CommandBuffer* commandBuffer = GetCurrentCommandBuffer();
    AssertDebug(commandBuffer->IsRecording());

    Frame* frame = GetCurrentFrame();
    frame->WriteCommandBuffer(commandBuffer);

    if (m_gpuTimerBackend != nullptr)
    {
        m_gpuTimerBackend->WriteStopTimestamp(commandBuffer, &g_statGpuFrameTime);
        m_gpuTimerBackend->OnFrameEnd(GetFrameCounter());
    }

    commandBuffer->End();
}

void RenderInterface::AddPass(NamedPass passName, PassBase* pass)
{
    AssertOnThread(g_renderThread);

    AssertDebug(passName < NumNamedPasses);

    AssertDebug(pass != nullptr);
    AssertDebug(!namedPasses[passName].Contains(pass));

    namedPasses[passName].PushBack(pass);
}

void RenderInterface::RemovePass(NamedPass passName, PassBase* pass)
{
    AssertOnThread(g_renderThread);

    AssertDebug(passName < NumNamedPasses);

    AssertDebug(pass != nullptr);
    AssertDebug(namedPasses[passName].Contains(pass));

    PoolDelete(*g_renderPool, pass);

    namedPasses[passName].Erase(pass);
}

void RenderInterface::CommitPipelineState(PSOType psoType, CommandBuffer* commandBuffer)
{
    AssertDebug(commandBuffer != nullptr);

    ShaderInstance* shaderInstance = nullptr;
    bool pipelineChanged = false;

    // set prev pipeline to null if state changed,
    // we cannot rely upon descriptors being valid between switches
    if (psoType != state.boundPsoType)
    {
        state.boundGraphicsPipeline = nullptr;
    }

    GraphicsPipelineCacheHandle cacheHandle;

    switch (psoType)
    {
    case PSO_Graphics:
    {
        AssertDebug(state.framebuffer != nullptr);

        GraphicsPipeline* pipeline = nullptr;

        if (!state.boundGraphicsPipeline || state.boundShaderDesc.properties != state.attributes.GetShaderProperties()
            || !state.boundGraphicsPipeline->MatchesSignature(
                state.attributes,
                state.framebuffer->GetFramebufferDesc(),
                state.stencilWriteMask,
                state.stencilCompareMask))
        {
            AssertDebug(state.attributes.GetMeshAttributes().inputLayout.mask != 0,
                        "Input layout cannot be empty for graphics pipeline");

            graphicsPipelineCache->GetOrCreate(
                state.attributes,
                state.framebuffer->GetFramebufferDesc(),
                state.stencilWriteMask,
                state.stencilCompareMask,
                cacheHandle);

            if (!cacheHandle.IsAlive())
            {
                HYP_LOG(Rendering, Error,
                        "Failed to create graphics pipeline for shader '{}' (shader may still be compiling or compilation failed).",
                        state.attributes.GetShaderName());

                state.boundGraphicsPipeline = nullptr;

                return;
            }

            pipeline = *cacheHandle;

            pipeline->lastFrame = GetFrameCounter();
            if (state.viewport.position != Vec2i(0, 0) || state.viewport.extent != Vec2u(0, 0))
                pipeline->Bind(commandBuffer, state.viewport.position, state.viewport.extent);
            else
                pipeline->Bind(commandBuffer);
            state.boundGraphicsPipeline = pipeline;

            pipelineChanged = true;
        }
        else
        {
            pipeline = state.boundGraphicsPipeline;
        }

        shaderInstance = pipeline->GetShader();
    }

    break;

    case PSO_Compute:
    {
        ComputePipeline* pipeline = nullptr;

        if (!state.boundComputePipeline
            || state.boundShaderDesc.properties != state.attributes.GetShaderProperties()
            || !state.boundComputePipeline->MatchesSignature(ShaderDesc(state.attributes.GetShaderName(), state.attributes.GetShaderProperties())))
        {
            pipeline = computePipelineCache->GetOrCreate(state.attributes.GetShaderName(), state.attributes.GetShaderProperties());
            AssertDebug(pipeline != nullptr);

            pipeline->Bind(commandBuffer);
            state.boundComputePipeline = pipeline;

            pipelineChanged = true;
        }
        else
        {
            pipeline = state.boundComputePipeline;
        }

        shaderInstance = pipeline->GetShader();
    }

    break;

    case PSO_RayTracing:
    {
        RayTracingPipeline* pipeline = nullptr;

        if (!state.boundRayTracingPipeline
            || state.boundShaderDesc.properties != state.attributes.GetShaderProperties()
            || !state.boundRayTracingPipeline->MatchesSignature(ShaderDesc(state.attributes.GetShaderName(), state.attributes.GetShaderProperties())))
        {
            pipeline = rayTracingPipelineCache->GetOrCreate(state.attributes.GetShaderName(), state.attributes.GetShaderProperties());
            AssertDebug(pipeline != nullptr);

            pipeline->Bind(commandBuffer);
            state.boundRayTracingPipeline = pipeline;

            pipelineChanged = true;
        }
        else
        {
            pipeline = state.boundRayTracingPipeline;
        }

        shaderInstance = pipeline->GetShader();
    }

    break;

    default:
        HYP_UNREACHABLE();
    }

    state.boundPsoType = psoType;
    state.boundShaderDesc = ShaderDesc(state.attributes.GetShaderName(), state.attributes.GetShaderProperties());

    AssertDebug(shaderInstance != nullptr);

    if (pipelineChanged)
    {
        // invalidate all uniforms on pipeline change
        state.dirtyUniforms |= (state.validUniforms | state.dirtyBufferOffsets);
        state.validUniforms = 0;

        Memory::Zero(state.prevBoundDescriptorSets, sizeof(state.prevBoundDescriptorSets));
    }

    const Shader* shader = shaderInstance->GetShader();
    AssertDebug(shader != nullptr);

    const ShaderInputGroup* tableDecl = shader->GetDescriptorTableDeclaration();
    AssertDebug(tableDecl != nullptr);

    enum DescriptorSetStateFlags : uint8
    {
        DSS_NotDirty = 0x0,
        DSS_BufferOffsetChanged = 0x1,
        DSS_Dirty = 0x2,
        DSS_GlobalReference = 0x4
    };

    const auto FetchDescriptorSet = [frameIndex = GetCurrentFrame()->GetFrameIndex()](const ShaderInputSet& inputSet, uint8& outStateFlags) -> DescriptorSet*
    {
        // reference to globally shared set
        if (inputSet.flags & ShaderInputSetFlags::Reference)
        {
            if (inputSet.flags & ShaderInputSetFlags::Template)
            {
                const ShaderInputSet* refDsDecl = RI.globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(inputSet.name);
                AssertDebug(refDsDecl != nullptr);

                DescriptorSetLayout layout { refDsDecl };
                return RI.descriptorSetCache->GetOrCreate(layout);
            }

            outStateFlags |= DSS_GlobalReference;

            return RI.globalDescriptorTable->GetDescriptorSet(inputSet.name, frameIndex);
        }
        else
        {
            DescriptorSetLayout layout { &inputSet };
            return RI.descriptorSetCache->GetOrCreate(layout);
        }
    };

    constexpr uint32 MaxDynamicOffsetsPerSet = 16; // 8;
    constexpr uint32 MaxDescriptorSetsBound = 4;

    DescriptorSet* setsToBind[MaxDescriptorSetsBound] {};

    uint8 bufferOffsets[MaxDescriptorSetsBound][MaxDynamicOffsetsPerSet] {}; // index of ShaderUniform
    uint8 bufferOffsetCounts[MaxDescriptorSetsBound] {};

#define IS_BIT_SET(bits, bitIdx) ((bits) & (1u << bitIdx))

    struct
    {
        uint8 setIndex : 4;
        ShaderRegister reg : 4;
    } uniformMappings[RenderInterface::State::MaxShaderUniforms] {};

    static_assert(0b1111 >= MaxDescriptorSetsBound);
    static_assert(0b1111 >= uint8(ShaderRegister::MAX));

    uint8 dsStates[MaxDescriptorSetsBound] {};
    [[maybe_unused]] uint8 dsIndices = 0;

    // set up uniform index to sets mapping
    TBitset<FixedAllocator<2>> bits { state.dirtyUniforms | state.dirtyBufferOffsets | state.validUniforms };

    for (auto currBit = bits.Begin(); bits.AnyBitsSet(); currBit = bits.Begin())
    {
        const uint8 uniformIndex = (uint8)*currBit;
        const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

        const ShaderInput* decl = nullptr;

        const ShaderInputSet* foundSetDecl = nullptr;

        for (const ShaderInputSet& setDecl : tableDecl->elements)
        {
            const ShaderInputSet* pSetDecl = &setDecl;

            // If this assertion fires, likely the Shader was destroyed from underneath us
            // likely due to shader recompilation and invalidation of cached pipelines for that shader not properly removing the cached pipelines...
            // This is a bug seen on mac sometimes (especially when switching selected node).
            // ---
            // We could add some more tracking on the Shader object, e.g set a flag before we call ExpirePipelinesForShader() and then
            // we can assert that this flag is not set before rendering, might help us get a better idea of where the issues stem from
            // ---
            // We also possibly need to unset active pipeline if the current bound pipeline happens to be one we're expiring in ExpirePipelinesForShader(). Hmm...
            AssertDebug(pSetDecl != nullptr);

            if (setDecl.flags & ShaderInputSetFlags::Reference)
            {
                pSetDecl = RI.globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(setDecl.name);
                AssertDebug(pSetDecl != nullptr);
            }

            decl = pSetDecl->FindDescriptorDeclaration(uniform.name);

            if (decl)
            {
                foundSetDecl = &setDecl;

                break;
            }
        }

        if (!decl)
        {
            // not found; skip
            state.validUniforms &= ~(1u << uniformIndex);
            state.dirtyUniforms &= ~(1u << uniformIndex);
            state.dirtyBufferOffsets &= ~(1u << uniformIndex);

            bits.Set(currBit, false);

            continue;
        }

        const uint8 setIndex = uint8(foundSetDecl->setIndex);

        if (IS_BIT_SET(state.dirtyUniforms, uniformIndex))
        {
            if (!(dsStates[setIndex] & DSS_Dirty))
            {
                setsToBind[setIndex] = FetchDescriptorSet(*foundSetDecl, dsStates[setIndex]);
                AssertDebug(setsToBind[setIndex] != nullptr);

                dsStates[setIndex] |= DSS_Dirty;
            }
        }

        if (IS_BIT_SET(state.dirtyBufferOffsets, uniformIndex))
        {
            if (!setsToBind[setIndex])
            {
                if (state.prevBoundDescriptorSets[setIndex])
                {
                    setsToBind[setIndex] = state.prevBoundDescriptorSets[setIndex];
                }
                else
                {
                    setsToBind[setIndex] = FetchDescriptorSet(*foundSetDecl, dsStates[setIndex]);
                    AssertDebug(setsToBind[setIndex] != nullptr);

                    dsStates[setIndex] |= DSS_Dirty;
                }
            }

            dsStates[setIndex] |= DSS_BufferOffsetChanged;
        }

        uniformMappings[uniformIndex] = { setIndex, decl->slot };

        dsIndices |= uint8(1u << setIndex);

        bits.Set(currBit, false);
    }

    // valid uniforms / buffer offset updates need to be rebound if the set is dirty
    FOR_EACH_BIT(state.validUniforms | state.dirtyBufferOffsets, uniformIndex)
    {
        const uint8 setIndex = uniformMappings[uniformIndex].setIndex;

        if (dsStates[setIndex] & DSS_Dirty)
        {
            state.dirtyUniforms |= (1u << uniformIndex);

            // state.validUniforms &= ~(1u << uniformIndex);
        }
    }

    // remaining valid uniforms need to be included in buffer offset updating if we are to update their sets' dynamic offsets.
    FOR_EACH_BIT(state.validUniforms, uniformIndex)
    {
        if (state.shaderUniforms[uniformIndex].type != ShaderUniform::UT_Buffer)
            continue;

        const uint8 setIndex = uniformMappings[uniformIndex].setIndex;

        if ((dsStates[setIndex] & (DSS_BufferOffsetChanged | DSS_Dirty)) == DSS_BufferOffsetChanged)
        {
            state.dirtyBufferOffsets |= (1u << uniformIndex);

            // state.validUniforms &= ~(1u << uniformIndex);
        }
    }

    if (psoType == PSO_Graphics && state.framebuffer != state.boundFramebuffer)
    {
        if (state.boundFramebuffer != nullptr)
        {
            state.boundFramebuffer->EndCapture(commandBuffer);
            state.boundFramebuffer = nullptr;
        }
    }

    // Transition images for use in shaders
    FOR_EACH_BIT(state.validUniforms | state.dirtyUniforms, uniformIndex)
    {
        if (state.shaderUniforms[uniformIndex].type != ShaderUniform::UT_ImageView)
            continue;

        const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

        GpuImageView* imageView = uniform.imageView;
        AssertDebug(imageView != nullptr, "Invalid image view for uniform {}", uniform.name);

        GpuImage* image = imageView->GetImage();
        AssertDebug(image != nullptr);

        const ShaderRegister reg = uniformMappings[uniformIndex].reg;
        AssertDebug(reg == ShaderRegister::SRV || reg == ShaderRegister::UAV);

        const ResourceState desiredResourceState = (reg == ShaderRegister::SRV) ? RS_SHADER_RESOURCE : RS_UNORDERED_ACCESS;

        // normalize counts
        ImageSubResource subResource = imageView->GetImageSubResource();
        subResource.numLayers = MathUtil::Min(subResource.numLayers, image->NumArrayLayers() - subResource.baseArrayLayer);
        subResource.numLevels = MathUtil::Min(subResource.numLevels, image->NumMips() - subResource.baseMipLevel);

        // HYP_LOG_TEMP("Shader: {}  Desire {} (mip: {} : {}) in resource state {} for {} shader input {}.",
        //     state.attributes.GetShaderName(),
        //     image->GetDebugName(), subResource.baseMipLevel, subResource.numLevels, EnumToString(desiredResourceState), EnumToString(reg), uniform.name);

        if (image->GetResourceState() != desiredResourceState || image->HasSubResourceStates())
        {
            if (image->IsFullSubResource(subResource))
            {
                // HYP_LOG_TEMP("Shader: {} needs to transition target {} from state {} to state {}", state.attributes.GetShaderName(), image->GetDebugName(), EnumToString(image->GetResourceState()), EnumToString(desiredResourceState));

                if (psoType == PSO_Graphics && state.boundFramebuffer != nullptr)
                {
                    // HYP_LOG_TEMP("Breaking framebuffer {} (bound to shader {})", state.boundFramebuffer->GetDebugName(), state.attributes.GetShaderName());

                    // have to end render pass if we are going to insert a barrier
                    state.boundFramebuffer->EndCapture(commandBuffer);
                    state.boundFramebuffer = nullptr;
                }

                image->InsertBarrier(commandBuffer, desiredResourceState, ShaderModuleType::None);

                AssertDebug(image->GetResourceState() == desiredResourceState);
            }
            else
            {
                bool needsTransition = false;

                for (uint8 mipIndex = subResource.baseMipLevel; mipIndex < subResource.baseMipLevel + subResource.numLevels; mipIndex++)
                {
                    for (uint16 layerIndex = subResource.baseArrayLayer; layerIndex < subResource.baseArrayLayer + subResource.numLayers; layerIndex++)
                    {
                        ImageSubResource currSubResource {};
                        currSubResource.baseMipLevel = mipIndex;
                        currSubResource.numLevels = 1;
                        currSubResource.baseArrayLayer = layerIndex;
                        currSubResource.numLayers = 1;

                        const ResourceState currResourceState = image->GetSubResourceState(currSubResource);

                        if (currResourceState != desiredResourceState)
                        {
                            // HYP_LOG_TEMP("Shader: {} needs to transition target {} from state {} to state {} (subresource: {}/{}/{})", state.attributes.GetShaderName(), image->GetDebugName(), EnumToString(image->GetResourceState()), EnumToString(desiredResourceState), mipIndex, layerIndex, subResource.baseArrayLayer + subResource.numLayers - 1);

                            needsTransition = true;

                            if (psoType == PSO_Graphics && state.boundFramebuffer != nullptr)
                            {
                                // HYP_LOG_TEMP("Breaking framebuffer {} (bound to shader {})", state.boundFramebuffer->GetDebugName(), state.attributes.GetShaderName());

                                // have to end render pass if we are going to insert a barrier
                                state.boundFramebuffer->EndCapture(commandBuffer);
                                state.boundFramebuffer = nullptr;
                            }

                            break;
                        }
                    }

                    if (needsTransition)
                    {
                        break;
                    }
                }

                if (needsTransition)
                {
                    image->InsertBarrier(commandBuffer, subResource, desiredResourceState, ShaderModuleType::None);

                    AssertDebug(image->GetSubResourceState(subResource) == desiredResourceState);
                }
            }
        }
    }

    if (psoType == PSO_Graphics)
    {
        if (state.framebuffer != state.boundFramebuffer)
        {
            if (state.boundFramebuffer != nullptr)
            {
                state.boundFramebuffer->EndCapture(commandBuffer);
                state.boundFramebuffer = nullptr;
            }

            AssertDebug(state.framebuffer != nullptr,
                        "No framebuffer bound at the time of CommitDrawState!");

            state.framebuffer->BeginCapture(commandBuffer);

            state.boundFramebuffer = state.framebuffer;
        }
        else if (state.boundFramebuffer == nullptr)
        {
            AssertDebug(state.framebuffer != nullptr,
                        "No framebuffer bound at the time of CommitDrawState!");

            state.framebuffer->BeginCapture(commandBuffer);

            state.boundFramebuffer = state.framebuffer;
        }
    }

    // Update dynamic states for existing graphics pipeline
    if (psoType == PSO_Graphics)
    {
        PSOCacheKey psoCacheKey = { state.attributes, state.boundFramebuffer->GetFramebufferDesc() };

        if (psoCacheKey != state.boundGraphicsPipeline->GetKey())
        {
            state.boundGraphicsPipeline->UpdateDynamicStates(commandBuffer);

            state.boundGraphicsPipeline->SetKey(psoCacheKey);
        }
    }

    AssertDebug(state.boundGraphicsPipeline != nullptr, "Pipeline not bound");

    if (state.dirtyUniforms)
    {
        bits.Clear();
        bits |= TBitset<FixedAllocator<2>> { state.dirtyUniforms };

        // Set dirty descriptors
        for (auto currBit = bits.Begin(); bits.AnyBitsSet(); currBit = bits.Begin())
        {
            const uint8 uniformIndex = (uint8)*currBit;
            const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

            uint8 setIndex = uniformMappings[uniformIndex].setIndex;

            DescriptorSet* ds = setsToBind[setIndex];
            AssertDebug(ds != nullptr);

            AssertDebug(dsStates[setIndex] & DSS_Dirty);

            switch (uniform.type)
            {
            case ShaderUniform::UT_Buffer:
                ds->SetElement(uniform.name, uniform.buffer, state.shaderUniformBufferStrides[uniformIndex]);

                state.dirtyBufferOffsets |= (1u << uniformIndex);

                break;
            case ShaderUniform::UT_ImageView:
                ds->SetElement(uniform.name, uniform.imageView);

                break;
            case ShaderUniform::UT_Sampler:
                ds->SetElement(uniform.name, uniform.sampler);

                break;
            case ShaderUniform::UT_Tlas:
                ds->SetElement(uniform.name, uniform.tlas);

                break;
            default:
                HYP_UNREACHABLE();
            }

            bits.Set(currBit, false);
        }
    }

    if (state.dirtyBufferOffsets)
    {
        bits.Clear();
        bits |= state.dirtyBufferOffsets;

        for (auto currBit = bits.Begin(); bits.AnyBitsSet(); currBit = bits.Begin())
        {
            const uint8 uniformIndex = (uint8)*currBit;

            uint8 setIndex = uniformMappings[uniformIndex].setIndex;

            const uint8 offsetIndex = bufferOffsetCounts[setIndex]++;
            AssertDebug(offsetIndex < MaxDynamicOffsetsPerSet);

            bufferOffsets[setIndex][offsetIndex] = (uint8)uniformIndex;

            bits.Set(currBit, false);
        }
    }

#if 0
    // For debugging:
    Array<Name, RenderTempAllocator> dirtyUniforms;
    dirtyUniforms.Reserve(State::MaxShaderUniforms);

    FOR_EACH_BIT(state.dirtyUniforms, bit)
    {
        dirtyUniforms.PushBack(Name(state.shaderUniforms[bit].name));
    }

    Array<Name, RenderTempAllocator> validUniforms;
    validUniforms.Reserve(State::MaxShaderUniforms);

    FOR_EACH_BIT(state.validUniforms, bit)
    {
        validUniforms.PushBack(Name(state.shaderUniforms[bit].name));
    }
#endif

    // now, we need to rebind sets that have NOT been modified (for example, in case of the first binding of graphics pipeline)
    for (uint32 setIndex = 0; setIndex < uint32(tableDecl->elements.Size()); setIndex++)
    {
        if (!setsToBind[setIndex])
        {
            // need to bind it again anyway if no prev descriptor set here.
            if (!state.prevBoundDescriptorSets[setIndex])
            {
                setsToBind[setIndex] = FetchDescriptorSet(tableDecl->elements[setIndex], dsStates[setIndex]);

                if (!(dsStates[setIndex] & DSS_GlobalReference) && !setsToBind[setIndex]->IsCreated())
                {
                    // just create it here, we have nothing to bind for it
                    Assert(setsToBind[setIndex]->Create());
                }
            }
        }
    }

    // bind descriptor sets
    for (uint8 setIndex = 0; setIndex < MaxDescriptorSetsBound; setIndex++)
    {
        DescriptorSet* ds = setsToBind[setIndex];

        if (!ds)
        {
            continue;
        }

        if ((dsStates[setIndex] & DSS_Dirty) && !(dsStates[setIndex] & DSS_GlobalReference))
        {
            if (!ds->IsCreated())
            {
                Check(ds->Create());
            }
            else
            {
                bool isDirty = false;
                ds->UpdateDirtyState(&isDirty);

                if (isDirty)
                {
                    ds->Update();
                }
            }
        }

        DescriptorSetOffsetMap offsets {};
        for (uint8 bufferOffsetIndex = 0; bufferOffsetIndex < bufferOffsetCounts[setIndex]; bufferOffsetIndex++)
        {
            const uint8 shaderUniformIndex = bufferOffsets[setIndex][bufferOffsetIndex];

            const ShaderUniform& uniform = state.shaderUniforms[shaderUniformIndex];
            AssertDebug(uniform.type == ShaderUniform::UT_Buffer,
                        "Uniform {} is not a buffer, cannot use buffer offset", uniform.name);

            offsets.Add(uniform.name, state.shaderUniformBufferOffsets[shaderUniformIndex]);
        }

        switch (psoType)
        {
        case PSO_Graphics:
            AssertDebug(state.boundGraphicsPipeline != nullptr);
            ds->Bind(commandBuffer, state.boundGraphicsPipeline, offsets, setIndex);
            break;
        case PSO_Compute:
            AssertDebug(state.boundComputePipeline != nullptr);
            ds->Bind(commandBuffer, state.boundComputePipeline, offsets, setIndex);
            break;
        case PSO_RayTracing:
            AssertDebug(state.boundRayTracingPipeline != nullptr);
            ds->Bind(commandBuffer, state.boundRayTracingPipeline, offsets, setIndex);
            break;
        default:
            HYP_UNREACHABLE();
        }

        state.prevBoundDescriptorSets[setIndex] = ds;
    }

    state.validUniforms |= state.dirtyUniforms;
    state.dirtyUniforms = 0;
    state.dirtyBufferOffsets = 0;

#undef IS_BIT_SET
}

void RenderInterface::FlushStructuredBuffers()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    bool anyNeedStaging = false;

    for (StructuredBuffer& sbuffer : namedBuffers)
    {
        if (sbuffer.IsDirty())
        {
            anyNeedStaging = true;

            break;
        }
    }

    if (!anyNeedStaging)
    {
        return;
    }

    // Batch all dirty structured buffers that need to use a staging buffer to copy data from the CPU to the GPU.
    CommandBuffer& cmdBuffer = GetTransientCommandBuffer();

    for (StructuredBuffer& sbuffer : namedBuffers)
    {
        if (sbuffer.IsDirty())
        {
            sbuffer.FlushInto(cmdBuffer);
        }
    }

    SubmitTransientCommandBuffer(cmdBuffer);
}

void RenderInterface::DeferFlushBuffer(RawBuffer* buffer)
{
    AssertOnThread(g_renderThread);

    if (!buffer || deferredFlushBuffers.Contains(buffer))
    {
        return;
    }

    deferredFlushBuffers.PushBack(buffer);
}

void RenderInterface::CreateBlueNoiseBuffer()
{
    Handle<AssetRegistry> registry = GetEngineAssetRegistry();
    if (!registry.IsValid())
    {
        HYP_FAIL("Engine asset registry not available, cannot load BlueNoise");
        return;
    }

    Handle<RawDataAsset> blueNoiseAsset = registry->GetAsset<RawDataAsset>(AssetBuckets::RawData, NAME("BlueNoise"));

    if (!blueNoiseAsset.IsValid())
    {
        HYP_FAIL("Failed to load BlueNoise asset from engine asset registry");
        return;
    }

    auto readScope = blueNoiseAsset->GetReadScope();

    ConstByteView blobData = blueNoiseAsset->GetData();

    constexpr size_t ExpectedSize = (sizeof(uint32) * 256 * 256)
        + (sizeof(uint32) * 128 * 128 * 8)
        + (sizeof(uint32) * 128 * 128 * 8);

    if (blobData.Size() != ExpectedSize)
    {
        HYP_FAIL("BlueNoise blob size mismatch: expected {} bytes, got {} bytes",
                 ExpectedSize, blobData.Size());
        return;
    }

    blueNoiseBuffer = StructuredBuffer(ExpectedSize / sizeof(Vec4i), sizeof(Vec4i));
    blueNoiseBuffer.Initialize();

#ifdef HYP_RHI_DEBUG_NAMES
    blueNoiseBuffer.gpuBuffer->SetDebugName(NAME("BlueNoiseBuffer"));
#endif

    blueNoiseBuffer.Write(0, ExpectedSize, blobData.Data());
    blueNoiseBuffer.Flush();
}

void RenderInterface::CreateSphereSamplesBuffer()
{
    sphereSamplesBuffer = StructuredBuffer(4096, sizeof(Vec4f));
    sphereSamplesBuffer.Initialize();

#ifdef HYP_RHI_DEBUG_NAMES
    sphereSamplesBuffer.gpuBuffer->SetDebugName(NAME("SphereSamplesBuffer"));
#endif

    Vec4f* sphereSamples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++)
    {
        Vec3f sample = MathUtil::RandomInSphere(
            Vec3f { MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed) });

        sphereSamples[i] = Vec4f(sample, 0.0f);
    }

    sphereSamplesBuffer.Write(0, sizeof(Vec4f) * 4096, sphereSamples);
    sphereSamplesBuffer.Flush();

    delete[] sphereSamples;
}

void RenderInterface::CreateEnvProbesColorTexture()
{
    TextureDesc textureDesc;
    textureDesc.format = TextureFormat::RGBA8;
    textureDesc.extent = Vec3u { 128, 128, 1 };
    textureDesc.imageUsage = IU_SAMPLED;
    textureDesc.type = TextureType::CubemapArray;
    textureDesc.numLayers = MaxBoundReflectionProbes;
    textureDesc.filterModeMin = TFM_LINEAR_MIPMAP;
    textureDesc.filterModeMag = TFM_LINEAR;

    envProbesColorTexture = MakeHandle<Texture>(textureDesc);
    envProbesColorTexture->SetName(NAME("EnvProbesColorTexture"));
    envProbesColorTexture->SetIsTransient(true);

    Check(envProbesColorTexture->Create());
}

void RenderInterface::CreateEnvProbesDepthTexture()
{
    // @TODO: not only reflection probes can have visibility texture.
    // we should make it so the dimensions of this are smaller so we can fit more. maybe use an atlas?

    TextureDesc textureDesc;
    textureDesc.format = TextureFormat::RG16F;
    textureDesc.extent = Vec3u { 16, 16, 1 };
    textureDesc.imageUsage = IU_SAMPLED;
    textureDesc.type = TextureType::CubemapArray;
    textureDesc.numLayers = MaxBoundReflectionProbes;
    textureDesc.filterModeMin = TFM_LINEAR;
    textureDesc.filterModeMag = TFM_LINEAR;

    envProbesDepthTexture = MakeHandle<Texture>(textureDesc);
    envProbesDepthTexture->SetName(NAME("EnvProbesDepthTexture"));
    envProbesDepthTexture->SetIsTransient(true);

    Check(envProbesDepthTexture->Create());
}

#pragma endregion RenderInterface

namespace Resources {

DECLARE_RENDER_DATA_CONTAINER(Entity, RenderProxyMesh, NamedBuffer::Entities, &WriteBufferData_MeshEntity, &s_meshEntityBinder);

DECLARE_RENDER_DATA_CONTAINER(Mesh, NullProxy, NamedBuffer::Invalid, nullptr, &s_meshBinder);

DECLARE_RENDER_DATA_CONTAINER(Camera, RenderProxyCamera, NamedBuffer::Cameras, nullptr, &s_cameraBinder);

DECLARE_RENDER_DATA_CONTAINER(ProbeVolume, RenderProxyProbeVolume, NamedBuffer::ProbeVolumes, nullptr, &s_probeVolumeBinder);

DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe, NamedBuffer::EnvProbes, &WriteBufferData_EnvProbe, &s_envProbeBinder);
DECLARE_RENDER_DATA_CONTAINER(ReflectionProbe, RenderProxyEnvProbe, NamedBuffer::EnvProbes, &WriteBufferData_EnvProbe, &s_envProbeBinder, &s_reflectionProbeTextureBinder);
DECLARE_RENDER_DATA_CONTAINER(SkyProbe, RenderProxyEnvProbe, NamedBuffer::EnvProbes, &WriteBufferData_EnvProbe, &s_envProbeBinder, &s_reflectionProbeTextureBinder);
DECLARE_RENDER_DATA_CONTAINER(IrradianceProbe, RenderProxyEnvProbe, NamedBuffer::EnvProbes, &WriteBufferData_EnvProbe, &s_envProbeBinder);

DECLARE_RENDER_DATA_CONTAINER(Light, RenderProxyLight, NamedBuffer::Lights, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(DirectionalLight, RenderProxyLight, NamedBuffer::Lights, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(PointLight, RenderProxyLight, NamedBuffer::Lights, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(AreaRectLight, RenderProxyLight, NamedBuffer::Lights, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(SpotLight, RenderProxyLight, NamedBuffer::Lights, &WriteBufferData_Light, &s_lightBinder);

DECLARE_RENDER_DATA_CONTAINER(LightmapVolume, RenderProxyLightmapVolume, NamedBuffer::LightmapVolumes, nullptr, &s_lightmapVolumeBinder);

DECLARE_RENDER_DATA_CONTAINER(ParticleVolume, RenderProxyParticleVolume, NamedBuffer::Invalid, nullptr, &s_particleVolumeBinder);
DECLARE_RENDER_DATA_CONTAINER(FogVolume, RenderProxyFogVolume, NamedBuffer::Invalid, nullptr, &s_fogVolumeBinder);

DECLARE_RENDER_DATA_CONTAINER(Sprite, RenderProxySprite, NamedBuffer::Invalid, nullptr, &s_spriteBinder);
DECLARE_RENDER_DATA_CONTAINER(TextSprite, RenderProxySprite, NamedBuffer::Invalid, nullptr, &s_spriteBinder);

DECLARE_RENDER_DATA_CONTAINER(Material, RenderProxyMaterial, NamedBuffer::Materials, nullptr, &s_materialBinder);

DECLARE_RENDER_DATA_CONTAINER(Texture, NullProxy, NamedBuffer::Invalid, nullptr, &s_textureBinder);

DECLARE_RENDER_DATA_CONTAINER(Skeleton, RenderProxySkeleton, NamedBuffer::Skeletons, nullptr, &s_skeletonBinder);

#define DECLARE_SRV_COND(setName, name, type, count, cond, cat) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::SRV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, ~0u, false, cat)
#define DECLARE_UAV_COND(setName, name, type, count, cond, cat) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::UAV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, ~0u, false, cat)
#define DECLARE_BUFFER_COND(setName, name, type, count, size, isDynamic, cond) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::BUFFER, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, isDynamic)
#define DECLARE_SAMPLER_COND(setName, name, type, count, cond) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::SAMPLER, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, ~0u, false)

#define DECLARE_SRV(setName, name, type, count) DECLARE_SRV_COND(setName, name, type, count, true, ShaderResourceCategory::Buffer)
#define DECLARE_UAV(setName, name, type, count) DECLARE_UAV_COND(setName, name, type, count, true, ShaderResourceCategory::Buffer)
#define DECLARE_BUFFER(setName, name, type, count, size, isDynamic) DECLARE_BUFFER_COND(setName, name, type, count, size, isDynamic, true)
#define DECLARE_SAMPLER(setName, name, type, count) DECLARE_SAMPLER_COND(setName, name, type, count, true)

} // namespace Resources

namespace {
static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <Rendering/Inl/DescriptorSets.inl>
    }
} s_globalDescriptorSetsDeclarations;
} // namespace

} // namespace Hyperion

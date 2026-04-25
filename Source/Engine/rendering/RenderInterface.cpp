/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/MaterialTextureCache.hpp>
#include <rendering/RendererBase.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/GlobalBuffers.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/GenericPipelineCache.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RayTracingPipeline.hpp>
#include <rendering/CommandRecorder.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Texture.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/SamplerCache.hpp>
#include <rendering/DescriptorSetCache.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/DebugDrawer.hpp>
#include <rendering/Shader.hpp>
#include <rendering/BLASCache.hpp>
#include <rendering/CrashHandler.hpp>
#include <rendering/CBufferAllocator.hpp>
#include <rendering/StructuredBufferAllocator.hpp>

#include <engine/resources/ResourceTracker.hpp>
#include <engine/resources/ResourceBinder.hpp>
#include <rendering/resources/ResourceBindings.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/ShadowRenderer.hpp>
#include <rendering/renderers/ParticleVolumeRenderer.hpp>
#include <rendering/renderers/SpriteRenderer.hpp>
#include <rendering/renderers/UIRenderer.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <rendering/util/ShaderPropertyDictionary.hpp>
#include <rendering/util/ShaderCompiler.hpp>

#include <scene/View.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Light.hpp>
#include <scene/ParticleVolume.hpp>
#include <scene/FogVolume.hpp>
#include <scene/LightmapVolume.hpp>
#include <scene/Sprite.hpp>
#include <scene/TextSprite.hpp>

#include <scene/animation/Skeleton.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/containers/SparsePagedArray.hpp>

#include <Core/threading/Semaphore.hpp>
#include <Core/threading/Threads.hpp>

#include <Core/memory/pool/Pool.hpp>

// for EnumToString
#include <Core/reflection/Enum.hpp>

#include <util/BlueNoise.hpp>

#include <engine/EngineStats.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/CVarManager.hpp>

#include <engine/config/EngineConfig.hpp>

#include <system/AppContext.hpp>

#include <HyperionEngine.hpp>

#include <semaphore>
#include <new>

#include <RenderInterface.generated.inl>

namespace Hyperion {

using namespace Resources;

static_assert(RingBufferDepth <= MinSafeDeleteCycles,
    "RingBufferDepth must be less than or equal to MinSafeDeleteCycles to ensure safe deletion of resources.");

static constexpr uint32 MaxFramesBeforeDiscard = 100; // number of frames before ViewData is discarded if not written to

// must be greater than or equal to MinSafeDeleteCycles so that
// we can ensure no active views hold pointers to deleted objects.
static_assert(MaxFramesBeforeDiscard >= MinSafeDeleteCycles,
    "MaxFramesBeforeDiscard must be greater than or equal to MinSafeDeleteCycles");

// iterations per frame for cleaning up unused resources for passes
static constexpr int FrameCleanupBudget = 16;

EngineStatTimer g_statRenderThreadSync("Render/Sync");
static EngineStatTimer s_statViewDataAllocTime { "Rendering/ViewData/AllocTime", /* resetPerFrame */ false };

namespace Framework {

static volatile int64 s_frameCounter; // atomic

static std::counting_semaphore<RingBufferDepth> s_fullSemaphore { 0 };
static std::counting_semaphore<RingBufferDepth> s_freeSemaphore { RingBufferDepth };

// thread-local frame index for the game and render threads
static thread_local uint32* s_threadFrameIndex;
static uint32 s_frameIndex[2] = { 0 };

static thread_local uint32 s_currentRenderThreadIndex;

static EngineConfig s_engineConfig[RingBufferDepth];

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
        : rplRender(g_renderPool, /* isShared */ false, /* useRefCounting */ false)
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

static HashMap<View*, ViewData*> s_viewData;

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
        viewData->lastUsedFrame = GetFrameCounter();

        view->AddRef();

        HYP_LOG(Rendering, Verbose, "Allocating new ViewData {} for View {} at frame {}\t(Camera : {})",
            (void*)viewData,
            view->Id(),
            GetFrameCounter(),
            view->GetCamera() ? *view->GetCamera()->GetName() : "null");

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

    return viewDataIt->second;
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

struct BufferedData
{
    HashMap<View*, BufferedViewData*> perViewData;
    SharedMutex viewFrameDataMutex;

    Array<World*> activeWorlds;

    Array<RenderProxyList*> ownedLists; // render thread side owned lists
    Array<RenderProxyList*> sharedLists;

    WorldShaderData worldBufferData {};
};

static BufferedData s_bufferedData[RingBufferDepth];

static BufferedViewData* GetBufferedViewData(View* view, uint32 slot)
{
    AssertDebug(view != nullptr);

    BufferedData& bufferedData = s_bufferedData[slot];

    TSharedLock<SharedMutex> sharedLock;
    TUniqueLock<SharedMutex> uniqueLock;

    // need to lock IFF on task thread
    if (Framework::s_threadFrameIndex == nullptr)
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

    bufferedViewData->rplShared = view->GetRenderProxyList(slot);
    AssertDebug(bufferedViewData->rplShared != nullptr);
    AssertDebug(bufferedViewData->rplShared->isShared, "Expected isShared to be true to ensure multiple threads don't access the list concurrently");

    bufferedData.perViewData[view] = bufferedViewData;

    return bufferedViewData;
}

} // namespace FrameData

uint32 GetRingIndex()
{
    if (HYP_UNLIKELY(!Framework::s_threadFrameIndex))
    {
        const int threadType = Framework::CurrentThreadType();
        Assert(threadType >= 0, "GetRingIndex called from an invalid thread!");

        Framework::s_threadFrameIndex = &Framework::s_frameIndex[threadType];
    }

    return *Framework::s_threadFrameIndex;
}

uint32 GetFrameCounter()
{
    return (uint32)AtomicAdd(&Framework::s_frameCounter, 0);
}

RenderProxyList& GetProducerProxyList(View* view)
{
    HYP_SCOPE;

    // can be called on sim thread or on task thread for tasks enqueued and awaited by sim thread, **exclusively**
    AssertOnThread(g_simThread | ThreadCategory::THREAD_CATEGORY_TASK);

    Framework::BufferedViewData* vd = Framework::GetBufferedViewData(
        view,
        Framework::s_frameIndex[Framework::TT_FrameDataProducer]);

    Assert(vd != nullptr);

    return *vd->rplShared;
}

RenderProxyList& GetConsumerProxyList(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertDebug(view != nullptr);

    Framework::ViewData* vd = Framework::GetViewData(view, false);

    if (vd == nullptr)
    {
        static RenderProxyList s_fallbackRpl { g_renderPool, /* isShared */ false, /* useRefCounting */ false };
        return s_fallbackRpl;
    }

    return vd->rplRender;
}

RenderCollector& GetRenderCollector(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Framework::ViewData* vd = Framework::GetViewData(view, false);
    
    if (vd == nullptr)
    {
        static RenderCollector s_fallbackRenderCollector;
        return s_fallbackRenderCollector;
    }
    
    return vd->renderCollector;
}

IRenderProxy* GetRenderProxy(const ObjectBase* resource)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertDebug(resource != nullptr);

    ResourceSubtypeData& subtypeData = g_renderInterface->resources->GetSubtypeData(resource->InstanceClass());
    AssertDebug(subtypeData.hasProxyData,
        "Cannot use GetRenderProxy() for type which does not have a RenderProxy! Type name: {}",
        subtypeData.typeInfo->name);

    const ObjIdBase resourceId = resource->Id();
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

void UpdateGpuData(const ObjectBase* resource)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    const ObjIdBase resourceId = resource->Id();

    ResourceSubtypeData& subtypeData = g_renderInterface->resources->GetSubtypeData(resource->InstanceClass());
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
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    ResourceSubtypeData& subtypeData = g_renderInterface->resources->GetSubtypeData(resource->InstanceClass());

    for (ResourceBinderBase** it = subtypeData.resourceBinders; *it; ++it)
    {
        ResourceBinderBase* resourceBinder = *it;
        resourceBinder->SetForceRebind(resource, forceRebind);
    }
}

WorldShaderData* GetWorldBufferData()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | g_renderThread);

    return &Framework::s_bufferedData[*Framework::s_threadFrameIndex].worldBufferData;
}

void CommitActiveWorlds(Span<World*> activeWorlds)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    Framework::BufferedData& bufferedData = Framework::s_bufferedData[Framework::s_frameIndex[Framework::TT_FrameDataProducer]];
    bufferedData.activeWorlds = Array<World*>(activeWorlds);
}

Span<World*> GetActiveWorlds()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | g_renderThread);

    return Framework::s_bufferedData[*Framework::s_threadFrameIndex].activeWorlds.ToSpan();
}

Viewport& GetViewport(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread | g_renderThread);

    return Framework::GetBufferedViewData(view, *Framework::s_threadFrameIndex)->viewport;
}

uint32 CurrentRenderThreadIndex()
{
    if (Framework::s_currentRenderThreadIndex == 0)
    {
        Framework::s_currentRenderThreadIndex = 1 + (IsOnThread(g_renderThread) ? 0 : GetCurrentThreadIndex() + 1);
    }

    return Framework::s_currentRenderThreadIndex - 1;
}

void BeginFrameSim()
{
    HYP_SCOPE;

    Framework::s_threadFrameIndex = &Framework::s_frameIndex[Framework::TT_FrameDataProducer];

    Framework::s_freeSemaphore.acquire();
}

void EndFrameSim()
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    const uint32 slot = Framework::s_frameIndex[Framework::TT_FrameDataProducer];

    Framework::BufferedData& bufferedData = Framework::s_bufferedData[slot];

    g_sceneArena->Reset();

    Framework::s_frameIndex[Framework::TT_FrameDataProducer] = (Framework::s_frameIndex[Framework::TT_FrameDataProducer] + 1) % RingBufferDepth;

    Framework::s_fullSemaphore.release();
}

EngineConfig& GetEngineConfig()
{
    return Framework::s_engineConfig[Framework::s_threadFrameIndex ? *Framework::s_threadFrameIndex : Framework::s_frameIndex[Framework::TT_FrameDataConsumer]];
}

#pragma region RenderInterface

RenderInterface::RenderInterface()
    : gpuBufferHolders(PoolNew<GpuBufferHolderMap>(*g_renderPool)),
      cbufferAllocator(PoolNew<CBufferAllocator>(*g_renderPool)),
      sbufferAllocator(PoolNew<StructuredBufferAllocator>(*g_renderPool)),
      descriptorSetCache(PoolNew<DescriptorSetCache>(*g_renderPool)),
      placeholderData(PoolNew<PlaceholderData>(*g_renderPool)),
      materialTextureCache(PoolNew<MaterialTextureCache>(*g_renderPool)),
      graphicsPipelineCache(PoolNew<GraphicsPipelineCache>(*g_renderPool)),
      computePipelineCache(PoolNew<ComputePipelineCache>(*g_renderPool)),
      rayTracingPipelineCache(PoolNew<RayTracingPipelineCache>(*g_renderPool)),
      bindlessStorage(PoolNew<BindlessStorage>(*g_renderPool)),
      shaderManager(PoolNew<ShaderManager>(*g_renderPool)),
      finalPass(nullptr),
      textureViewCache(PoolNew<TextureViewCache>(*g_renderPool)),
      samplerCache(PoolNew<SamplerCache>(*g_renderPool)),
      stagingBufferPool(PoolNew<StagingBufferPool>(*g_renderPool)),
      blasCache(PoolNew<BLASCache>(*g_renderPool)),
      shadowMapCache(PoolNew<ShadowMapCache>(*g_renderPool)),
      crashHandler(PoolNew<CrashHandler>(*g_renderPool))
{
}

RenderInterface::~RenderInterface()
{
}

RendererResult RenderInterface::Initialize()
{
    HYP_LOG(Rendering, Verbose, "Initializing base render interface");

    Framework::s_threadFrameIndex = &Framework::s_frameIndex[Framework::TT_FrameDataConsumer];

    InitDeviceDetails(deviceDetails);

    namedBuffers[NamedBuffer::Worlds] = StructuredBuffer(MaxBoundWorlds, sizeof(WorldShaderData));
    namedBuffers[NamedBuffer::Cameras] = StructuredBuffer(MaxBoundCameras, sizeof(CameraShaderData));
    namedBuffers[NamedBuffer::Lights] = StructuredBuffer(MaxBoundLights, sizeof(LightShaderData));
    namedBuffers[NamedBuffer::Entities] = StructuredBuffer(MaxBoundEntities, sizeof(EntityShaderData));
    namedBuffers[NamedBuffer::Materials] = StructuredBuffer(MaxBoundMaterials, sizeof(MaterialShaderData));
    namedBuffers[NamedBuffer::Skeletons] = StructuredBuffer(MaxBoundSkeletons, sizeof(SkeletonShaderData));
    namedBuffers[NamedBuffer::EnvProbes] = StructuredBuffer(MaxBoundEnvProbes, sizeof(EnvProbeShaderData));
    namedBuffers[NamedBuffer::EnvGrids] = StructuredBuffer(MaxBoundEnvGrids, sizeof(EnvGridShaderData));
    namedBuffers[NamedBuffer::LightmapVolumes] = StructuredBuffer(MaxBoundLightmapVolumes, sizeof(LightmapVolumeShaderData));

    for (uint8 namedBufferIndex = 0; namedBufferIndex < NamedBuffer::Max; namedBufferIndex++)
    {
        StructuredBuffer& sbuffer = namedBuffers[namedBufferIndex];

        if (!sbuffer.cpuBuffer.Empty())
        {
            sbuffer.Initialize();

#if HYP_DEBUG_MODE
            sbuffer.gpuBuffer->SetDebugName(CreateNameFromDynamicString(NamedBuffer::StringValues[namedBufferIndex]));
#endif // HYP_DEBUG_MODE
        }
    }

    crashHandler->Initialize();

    resources = PoolNew<ResourceContainer>(*g_renderPool);

    for (ResourceBinderBase* resourceBinder : s_resourceBinders)
    {
        resourceBinder->Initialize();
    }

    {
        EngineConfig& engineConfig = Framework::s_engineConfig[0];
        engineConfig.Load();

        bool shouldDisableRayTracing = !GetRenderConfig().rayTracing;

#if HYP_ANDROID
        shouldDisableRayTracing = true;

        // For Android leave these rendering settings off.
        engineConfig.Set("Rendering.IndirectRendering", false);
        engineConfig.Set("Rendering.SSGI", false);
        engineConfig.Set("Rendering.TAA", false);
        engineConfig.Set("Rendering.SSR.Enabled", false);
        engineConfig.Set("Rendering.HBAO.Enabled", false);
#endif

        // if ray tracing is not supported, we need to update the configuration
        if (shouldDisableRayTracing)
        {
            engineConfig.Set("Rendering.RayTracing.Enabled", false);
            engineConfig.Set("Rendering.RayTracing.Reflections.Enabled", false);
            engineConfig.Set("Rendering.RayTracing.GI.Enabled", false);
            engineConfig.Set("Rendering.RayTracing.PathTracing.Enabled", false);
        }

        if (engineConfig.IsChanged())
        {
            engineConfig.Save();
        }
        
        CVarManager::GetInstance().InitFromConfig(engineConfig);

        for (uint32 i = 1; i < RingBufferDepth; i++)
        {
            Framework::s_engineConfig[i] = engineConfig;
        }
    }

    finalPass = PoolNew<FinalPass>(*g_renderPool);
    finalPass->Create();

    ResourceContainerFactoryRegistry& registry = ResourceContainerFactoryRegistry::GetInstance();
    registry.InvokeAll(*resources);

    registry.funcs.Clear();

    globalDescriptorTable = MakeDescriptorTable(&GetStaticDescriptorTableDeclaration());

    placeholderData->Initialize();
    shadowMapCache->Initialize();

    DebugDrawer::GetInstance().Initialize();

    CreateSphereSamplesBuffer();
    CreateBlueNoiseBuffer();
    CreateEnvProbesTexture();

    globalDescriptorTable->Create();

    for (uint32 i = 0; i < GRT_MAX; i++)
    {
        globalRenderers[i] = Array<RendererBase*>();
    }

    globalRenderers[GRT_MAIN].PushBack(new DeferredRenderer);
    globalRenderers[GRT_MAIN][0]->Initialize();

    globalRenderers[GRT_UI].PushBack(new UIRenderer);
    globalRenderers[GRT_MAIN][0]->Initialize();

    globalRenderers[GRT_ENV_PROBE].ResizeZeroed(EPT_MAX);
    globalRenderers[GRT_ENV_PROBE][EPT_REFLECTION] = new ReflectionProbeRenderer;
    globalRenderers[GRT_ENV_PROBE][EPT_SKY] = new ReflectionProbeRenderer;

    globalRenderers[GRT_SHADOW_MAP].ResizeZeroed(NumLightTypes); // 1 ShadowMapRenderer per LightType
    globalRenderers[GRT_SHADOW_MAP][uint32(LightType::Point)] = new PointShadowRenderer;
    globalRenderers[GRT_SHADOW_MAP][uint32(LightType::Directional)] = new DirectionalShadowRenderer;

    // one global particle volume renderer
    globalRenderers[GRT_PARTICLE_VOLUME].ResizeZeroed(1);
    globalRenderers[GRT_PARTICLE_VOLUME][0] = new ParticleVolumeRenderer;

    // one global sprite renderer
    globalRenderers[GRT_SPRITE].ResizeZeroed(1);
    globalRenderers[GRT_SPRITE][0] = new SpriteRenderer;
    globalRenderers[GRT_SPRITE][0]->Initialize();

    return {};
}

void RenderInterface::Shutdown()
{
    for (uint32 i = 0; i < RingBufferDepth; i++)
    {
        for (auto& it : Framework::s_bufferedData[i].perViewData)
        {
            delete it.second;
        }

        Framework::s_bufferedData[i].perViewData.Clear();
    }

    for (auto& it : Framework::s_viewData)
    {
        Framework::ViewData* vd = it.second;

        if (!vd)
        {
            continue;
        }

        PoolDelete(*g_renderPool, vd);
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

    for (uint32 i = 0; i < GRT_MAX; i++)
    {
        for (uint32 j = 0; j < globalRenderers[i].Size(); j++)
        {
            if (globalRenderers[i][j])
            {
                globalRenderers[i][j]->Shutdown();
                delete globalRenderers[i][j];
            }
        }
    }
    
    DebugDrawer::GetInstance().Shutdown();

    for (StructuredBuffer& grb : namedBuffers)
    {
        grb.Shutdown();
    }

    blueNoiseBuffer.Reset();
    sphereSamplesBuffer.Reset();
    envProbesTexture.Reset();

    shadowMapCache->Shutdown();
    placeholderData->Shutdown();

    globalDescriptorTable.Reset();

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
    
    PoolDelete(*g_renderPool, sbufferAllocator);
    sbufferAllocator = nullptr;

    PoolDelete(*g_renderPool, gpuBufferHolders);
    gpuBufferHolders = nullptr;

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
    
    PoolDelete(*g_renderPool, crashHandler);
    crashHandler = nullptr;

    DeletionQueue::GetInstance().Flush();
}

void RenderInterface::BeginFrame(AtomicFlag* pCancelFlag)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    {
        ENGINE_STAT_SCOPE(&g_statRenderThreadSync);

        while (!Framework::s_fullSemaphore.try_acquire())
        {
            if (pCancelFlag != nullptr && pCancelFlag->Load())
            {
                return;
            }

            ThreadSleep(0);
        }
    }

    const uint32 slot = Framework::s_frameIndex[Framework::TT_FrameDataConsumer];
    const uint32 currFrame = GetFrameCounter();

    PrepareNextFrame();

    Framework::BufferedData& bufferedData = Framework::s_bufferedData[slot];

    cbufferAllocator->OnFrameStart();
    sbufferAllocator->OnFrameStart();
    descriptorSetCache->OnFrameStart();
    stagingBufferPool->OnFrameStart();

    g_engineStats->Prepare();

    RenderCommands::Flush();

    Span<View* const> activeViews = g_engineDriver->GetCurrentFrameViews();
    
    for (View* view : activeViews)
    {
        // ensure BufferedViewData exists
        Framework::BufferedViewData& bufferedViewData = *Framework::GetBufferedViewData(view, slot);
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

    for (size_t i = 0; i < bufferedData.ownedLists.Size(); i++)
    {
        RenderProxyList* rplRender = bufferedData.ownedLists[i];
        AssertDebug(rplRender != nullptr);

        RenderProxyList* rplShared = bufferedData.sharedLists[i];

        // Advance render side owned lists ResourceTrackers before we copy dependencies over
        int resourceTrackerIndex = 0;
        StaticForEach<typename RenderProxyList::ResourceTrackerTypes>([rplRender, &resourceTrackerIndex]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>)
            {
                ResourceTrackerType& resourceTracker = static_cast<ResourceTrackerType&>(*rplRender->resourceTrackers[resourceTrackerIndex]);
                resourceTracker.Advance();

                ++resourceTrackerIndex;
            });

        if (!rplShared)
        {
            static RenderProxyList s_defaultRenderProxyList { g_renderPool, /* isShared */ false, /* useRefCounting */ false };
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

    {
        HYP_NAMED_SCOPE("Resource bindings - select candidates");

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

                    if (forceRebind)
                    {
                        pProxy->forceRebind = false; // swap
                    }
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
                    i, slot, i);

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
    for (View* view : activeViews)
    {
        Framework::BufferedViewData& bufferedViewData = *Framework::GetBufferedViewData(view, slot);
        AssertDebug(bufferedViewData.rplShared != nullptr);
        AssertDebug(bufferedViewData.viewData != nullptr);

        Framework::ViewData& vd = *bufferedViewData.viewData;

        if (bufferedViewData.rplShared->disableBuildRenderCollection || (bufferedViewData.view->GetFlags() & ViewFlags::NO_DRAW_CALLS))
        {
            continue;
        }

        vd.rplRender.BeginRead();

        vd.renderCollector.BuildRenderGroups(vd.view, vd.rplRender);
        vd.renderCollector.BuildDrawCalls(0);

        vd.rplRender.EndRead();
    }
    
    GetCurrentCommandBuffer()->Begin();
}

void RenderInterface::EndFrame()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 slot = Framework::s_frameIndex[Framework::TT_FrameDataConsumer];

    Framework::BufferedData& bufferedData = Framework::s_bufferedData[slot];
    bufferedData.activeWorlds.Clear();
    
    //sbufferAllocator->UpdateAllUsedInFrame(GetCurrentFrame()->preRenderCommands);

    stagingBufferPool->Cleanup();

    const uint32 currFrame = GetFrameCounter();

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
            if (int64(currFrame) - int64(viewData->lastUsedFrame) >= MaxFramesBeforeDiscard)
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
                    StaticForEach<typename RenderProxyList::ResourceTrackerTypes>([&viewData, &resourceTrackerIndex]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>)
                        {
                            ResourceTrackerType& resourceTracker = static_cast<ResourceTrackerType&>(*viewData->rplRender.resourceTrackers[resourceTrackerIndex]);
                            resourceTracker.Advance();

                            ++resourceTrackerIndex;
                        });

                    static RenderProxyList s_emptyRpl { g_renderPool, /* isShared */ false, /* useRefCounting */ false };
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

    for (uint32 i = 0; i < GRT_MAX; i++)
    {
        for (uint32 j = 0; j < globalRenderers[i].Size(); j++)
        {
            if (RendererBase* renderer = globalRenderers[i][j])
            {
                renderer->RunCleanupCycle();
            }
        }
    }

    graphicsPipelineCache->RunCleanupCycle(16);
    computePipelineCache->RunCleanupCycle(4);
    rayTracingPipelineCache->RunCleanupCycle(1);

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

    blasCache->RunCleanupCycle(32);

    // shadowViewCache->RunCleanupCycle(8);

    DeletionQueue::GetInstance().UpdateEntryListQueue();
    DeletionQueue::GetInstance().Iterate();

    g_engineStats->Advance();

    CVarManager::GetInstance().Advance();

    ReleaseTransientMemory();
    NextFrame();

    state.Reset();

    cbufferAllocator->OnFrameEnd();
    sbufferAllocator->OnFrameEnd();
    descriptorSetCache->OnFrameEnd();
    stagingBufferPool->OnFrameEnd();

    textureViewCache->CleanupUnusedTextures();

    const uint32 nextFrameIndex = (Framework::s_frameIndex[Framework::TT_FrameDataConsumer] + 1) % RingBufferDepth;

    /// Sync the engine config for the current frame to prev frame
    Framework::s_engineConfig[nextFrameIndex] = Framework::s_engineConfig[slot];

    Framework::s_frameIndex[Framework::TT_FrameDataConsumer] = nextFrameIndex;

    AtomicIncrement(&Framework::s_frameCounter);

    Framework::s_freeSemaphore.release();
}

void RenderInterface::AddRenderer(GlobalRendererType globalRendererType, RendererBase* renderer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(!globalRenderers[globalRendererType].Contains(renderer));

    globalRenderers[globalRendererType].PushBack(renderer);
}

void RenderInterface::RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(globalRenderers[globalRendererType].Contains(renderer));

    PoolDelete(*g_renderPool, renderer);

    globalRenderers[globalRendererType].Erase(renderer);
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

    union
    {
        ValueStorage<BindGraphicsPipeline> bindGraphicsCmd;
        ValueStorage<BindComputePipeline> bindComputeCmd;
        ValueStorage<BindRayTracingPipeline> bindRayTracingCmd;
    } deferredBindCommandMemory;

    void (*executeBindCmdFunction)(CmdBase*, CommandBuffer*) = nullptr;

    switch (psoType)
    {
    case PSO_Graphics:
    {
        AssertDebug(state.framebuffer != nullptr);
        
        GraphicsPipeline* pipeline = nullptr;

        if (!state.boundGraphicsPipeline
            || state.boundShaderDesc.properties != state.attributes.GetShaderProperties()
            || !state.boundGraphicsPipeline->MatchesSignature(state.attributes, state.framebuffer->GetFramebufferDesc()))
        {
            GraphicsPipelineCacheHandle cacheHandle;

            graphicsPipelineCache->GetOrCreate(
                state.attributes,
                state.framebuffer->GetFramebufferDesc(),
                cacheHandle);

            pipeline = *cacheHandle;

            new (&deferredBindCommandMemory.bindGraphicsCmd) BindGraphicsPipeline(pipeline, state.viewport);
            executeBindCmdFunction = &BindGraphicsPipeline::InvokeStatic;

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

            new (&deferredBindCommandMemory.bindComputeCmd) BindComputePipeline(pipeline);
            executeBindCmdFunction = &BindComputePipeline::InvokeStatic;

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

            new (&deferredBindCommandMemory.bindRayTracingCmd) BindRayTracingPipeline(pipeline);
            executeBindCmdFunction = &BindRayTracingPipeline::InvokeStatic;

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

        Memory::Fill(state.prevBoundDescriptorSets, 0, sizeof(state.prevBoundDescriptorSets));
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
                const ShaderInputSet* refDsDecl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(inputSet.name);
                AssertDebug(refDsDecl != nullptr);

                DescriptorSetLayout layout { refDsDecl };
                return g_renderInterface->descriptorSetCache->GetOrCreate(layout);
            }

            outStateFlags |= DSS_GlobalReference;

            return g_renderInterface->globalDescriptorTable->GetDescriptorSet(inputSet.name, frameIndex);
        }
        else
        {
            DescriptorSetLayout layout { &inputSet };
            return g_renderInterface->descriptorSetCache->GetOrCreate(layout);
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
    uint8 dsIndices = 0;

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

            if (setDecl.flags & ShaderInputSetFlags::Reference)
            {
                pSetDecl = g_renderInterface->globalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(setDecl.name);
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

        const ShaderRegister reg = uniformMappings[uniformIndex].reg;
        AssertDebug(reg == ShaderRegister::SRV || reg == ShaderRegister::UAV);

        const ResourceState desiredResourceState = (reg == ShaderRegister::SRV) ? RS_SHADER_RESOURCE : RS_UNORDERED_ACCESS;

        // normalize counts
        ImageSubResource subResource = imageView->GetImageSubResource();
        subResource.numLayers = MathUtil::Min(subResource.numLayers, image->NumArrayLayers() - subResource.baseArrayLayer);
        subResource.numLevels = MathUtil::Min(subResource.numLevels, image->NumMips() - subResource.baseMipLevel);

        /*HYP_LOG_TEMP("Desire {} (mip: {} : {}) in resource state {} for {} shader input {}.", image->GetDebugName(), subResource.baseMipLevel, subResource.numLevels, EnumToString(desiredResourceState),
            EnumToString(inputType), uniform.name);*/

        if (image->GetResourceState() != desiredResourceState || image->HasSubResourceStates())
        {
            if (image->IsFullSubResource(subResource))
            {
                if (psoType == PSO_Graphics && state.boundFramebuffer != nullptr)
                {
                    // have to end render pass if we are going to insert a barrier
                    state.boundFramebuffer->EndCapture(commandBuffer);
                    state.boundFramebuffer = nullptr;
                }

                image->InsertBarrier(commandBuffer, desiredResourceState, ShaderModuleType::None);
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
                            needsTransition = true;

                            if (psoType == PSO_Graphics && state.boundFramebuffer != nullptr)
                            {
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
                }
            }
        }
    }

    if (psoType == PSO_Graphics)
    {
        if (state.boundFramebuffer == nullptr)
        {
            AssertDebug(state.framebuffer != nullptr,
                "No framebuffer bound at the time of CommitDrawState!");

            state.framebuffer->BeginCapture(commandBuffer);

            state.boundFramebuffer = state.framebuffer;
        }
    }

    // Bind the new pipeline (if we need to)
    if (executeBindCmdFunction != nullptr)
    {
        executeBindCmdFunction(
            reinterpret_cast<CmdBase*>(&deferredBindCommandMemory),
            commandBuffer);
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
                ds->SetElement(uniform.name, uniform.buffer, state.shaderUniformBufferOffsetStrides[uniformIndex]);

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
            const ShaderUniform& uniform = state.shaderUniforms[uniformIndex];

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
                Assert(ds->Create());
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
            ds->Bind(commandBuffer, state.boundGraphicsPipeline, offsets, setIndex);
            break;
        case PSO_Compute:
            ds->Bind(commandBuffer, state.boundComputePipeline, offsets, setIndex);
            break;
        case PSO_RayTracing:
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

    // Check whether any buffer is actually dirty before acquiring a command buffer.
    bool anyDirty = false;

    for (StructuredBuffer& sbuffer : namedBuffers)
    {
        if (sbuffer.IsDirty())
        {
            anyDirty = true;
            break;
        }
    }

    if (!anyDirty)
    {
        return;
    }

    // Batch all dirty structured-buffer copies into a single command buffer and
    // submit once. Previously each Flush() issued its own vkQueueSubmit + fence,
    // meaning up to 9 separate submissions per frame and 9 fence waits next frame.
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

void RenderInterface::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    static_assert(sizeof(BlueNoiseBuffer::sobol256spp256d) == sizeof(BlueNoise::sobol256spp256d));
    static_assert(sizeof(BlueNoiseBuffer::scramblingTile) == sizeof(BlueNoise::scramblingTile));
    static_assert(sizeof(BlueNoiseBuffer::rankingTile) == sizeof(BlueNoise::rankingTile));

    constexpr size_t blueNoiseBufferSize = sizeof(BlueNoiseBuffer);

    constexpr size_t sobol256spp256dOffset = offsetof(BlueNoiseBuffer, sobol256spp256d);
    constexpr size_t sobol256spp256dSize = sizeof(BlueNoise::sobol256spp256d);
    constexpr size_t scramblingTileOffset = offsetof(BlueNoiseBuffer, scramblingTile);
    constexpr size_t scramblingTileSize = sizeof(BlueNoise::scramblingTile);
    constexpr size_t rankingTileOffset = offsetof(BlueNoiseBuffer, rankingTile);
    constexpr size_t rankingTileSize = sizeof(BlueNoise::rankingTile);

    static_assert(blueNoiseBufferSize
        == (sobol256spp256dOffset + sobol256spp256dSize)
            + ((scramblingTileOffset - (sobol256spp256dOffset + sobol256spp256dSize)) + scramblingTileSize)
            + ((rankingTileOffset - (scramblingTileOffset + scramblingTileSize)) + rankingTileSize));

    blueNoiseBuffer = MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, sizeof(BlueNoiseBuffer));
#if HYP_DEBUG_MODE
    blueNoiseBuffer->SetDebugName(NAME("BlueNoiseBuffer"));
#endif

    blueNoiseBuffer->SetIsCpuAccessible(true);
    CheckResult(blueNoiseBuffer->Create());

    blueNoiseBuffer->Copy(sobol256spp256dOffset, sobol256spp256dSize, &BlueNoise::sobol256spp256d[0]);
    blueNoiseBuffer->Copy(scramblingTileOffset, scramblingTileSize, &BlueNoise::scramblingTile[0]);
    blueNoiseBuffer->Copy(rankingTileOffset, rankingTileSize, &BlueNoise::rankingTile[0]);
}

void RenderInterface::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;

    sphereSamplesBuffer = MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, sizeof(Vec4f) * 4096);
#if HYP_DEBUG_MODE
    sphereSamplesBuffer->SetDebugName(NAME("SphereSamplesBuffer"));
#endif

    sphereSamplesBuffer->SetIsCpuAccessible(true);
    CheckResult(sphereSamplesBuffer->Create());

    Vec4f* sphereSamples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++)
    {
        Vec3f sample = MathUtil::RandomInSphere(
            Vec3f { MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed) });

        sphereSamples[i] = Vec4f(sample, 0.0f);
    }

    sphereSamplesBuffer->Copy(sizeof(Vec4f) * 4096, sphereSamples);
    sphereSamplesBuffer->Flush(0, sizeof(Vec4f) * 4096);

    delete[] sphereSamples;
}

void RenderInterface::CreateEnvProbesTexture()
{
    TextureDesc textureDesc;
    textureDesc.format = TextureFormat::RGBA16F;
    textureDesc.extent = Vec3u { 128, 128, 1 };
    textureDesc.imageUsage = IU_SAMPLED;
    textureDesc.type = TextureType::CubemapArray;
    textureDesc.numLayers = MaxBoundReflectionProbes;
    textureDesc.filterModeMin = TFM_LINEAR_MIPMAP;
    textureDesc.filterModeMag = TFM_LINEAR;

    envProbesTexture = MakeHandle<Texture>(textureDesc);
    envProbesTexture->SetName(NAME("EnvProbesTexture"));
    envProbesTexture->SetIsTransient(true);
    CheckResult(envProbesTexture->Create());
}

#pragma endregion RenderInterface

namespace Resources {

DECLARE_RENDER_DATA_CONTAINER(Entity, RenderProxyMesh, NamedBuffer::Entities, &WriteBufferData_MeshEntity, &s_meshEntityBinder);

DECLARE_RENDER_DATA_CONTAINER(Mesh, NullProxy, NamedBuffer::Invalid, nullptr, &s_meshBinder);

DECLARE_RENDER_DATA_CONTAINER(Camera, RenderProxyCamera, NamedBuffer::Cameras, nullptr, &s_cameraBinder);

DECLARE_RENDER_DATA_CONTAINER(EnvGrid, RenderProxyEnvGrid, NamedBuffer::EnvGrids, nullptr, &s_envGridBinder);

DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe, NamedBuffer::EnvProbes, &WriteBufferData_EnvProbe, &s_envProbeBinder);
DECLARE_RENDER_DATA_CONTAINER(ReflectionProbe, RenderProxyEnvProbe, NamedBuffer::EnvProbes, &WriteBufferData_EnvProbe, &s_envProbeBinder, &s_reflectionProbeTextureBinder);
DECLARE_RENDER_DATA_CONTAINER(SkyProbe, RenderProxyEnvProbe, NamedBuffer::EnvProbes, &WriteBufferData_EnvProbe, &s_envProbeBinder, &s_reflectionProbeTextureBinder);

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

DECLARE_RENDER_DATA_CONTAINER(MaterialInstance, RenderProxyMaterial, NamedBuffer::Materials, nullptr, &s_materialBinder);

DECLARE_RENDER_DATA_CONTAINER(Texture, NullProxy, NamedBuffer::Invalid, nullptr, &s_textureBinder);

DECLARE_RENDER_DATA_CONTAINER(Skeleton, RenderProxySkeleton, NamedBuffer::Skeletons, nullptr, &s_skeletonBinder);


#define DECLARE_SRV_COND(setName, name, type, count, cond) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::SRV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define DECLARE_UAV_COND(setName, name, type, count, cond) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::UAV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define DECLARE_BUFFER_COND(setName, name, type, count, size, isDynamic, cond) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::BUFFER, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, isDynamic)
#define DECLARE_SAMPLER_COND(setName, name, type, count, cond) \
    static ShaderInputGroup::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(&GetStaticDescriptorTableDeclaration(), HYP_NAME_UNSAFE(setName), type, ShaderRegister::SAMPLER, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)

#define DECLARE_SRV(setName, name, type, count) DECLARE_SRV_COND(setName, name, type, count, true)
#define DECLARE_UAV(setName, name, type, count) DECLARE_UAV_COND(setName, name, type, count, true)
#define DECLARE_BUFFER(setName, name, type, count, size, isDynamic) DECLARE_BUFFER_COND(setName, name, type, count, size, isDynamic, true)
#define DECLARE_SAMPLER(setName, name, type, count) DECLARE_SAMPLER_COND(setName, name, type, count, true)

} // namespace Resources

namespace {
static struct GlobalDescriptorSetsDeclarations
{
    GlobalDescriptorSetsDeclarations()
    {
#include <rendering/inl/DescriptorSets.inl>
    }
} s_globalDescriptorSetsDeclarations;
} // namespace

} // namespace Hyperion

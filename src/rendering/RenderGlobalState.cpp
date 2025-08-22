/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/GpuBufferHolderMap.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderStats.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/env_grid/EnvGridRenderer.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowRenderer.hpp>

#include <rendering/rt/DDGI.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <rendering/Material.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/animation/Skeleton.hpp>

#include <core/object/HypClass.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/threading/Semaphore.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/BlueNoise.hpp>

#include <engine/EngineGlobals.hpp>

#include <system/AppContext.hpp>

#include <HyperionEngine.hpp>

#include <semaphore>

namespace hyperion {

static constexpr uint32 g_numFrames = g_numMultiBuffers;
static_assert(g_numFrames <= g_minSafeDeleteCycles,
    "g_numFrames must be less than or equal to g_minSafeDeleteCycles to ensure safe deletion of resources.");

static constexpr uint32 g_maxFramesBeforeDiscard = 10; // number of frames before ViewData is discarded if not written to

// must be greater than or equal to g_minSafeDeleteCycles so that
// we can ensure no active views hold pointers to deleted objects.
static_assert(g_maxFramesBeforeDiscard >= g_minSafeDeleteCycles,
    "g_maxFramesBeforeDiscard must be greater than or equal to g_minSafeDeleteCycles");

// iterations per frame for cleaning up unused resources for passes
static constexpr int g_frameCleanupBudget = 16;

// thread-local frame index for the game and render threads
// @NOTE: thread local so initialized to 0 on each thread by default
thread_local volatile int32* g_threadFrameIndex;

static volatile int64 g_frameCounter; // atomic
static volatile int32 g_frameIndex[2] = { 0 };

// Render thread only
static RenderStats g_renderStats {};
static RenderStatsCalculator g_renderStatsCalculator {};

static std::counting_semaphore<g_numFrames> g_fullSemaphore { 0 };
static std::counting_semaphore<g_numFrames> g_freeSemaphore { g_numFrames };

enum
{
    PRODUCER,
    CONSUMER
};

#pragma region ResourceBindings

/// TODO: refactor to use mappings instead of idx (void* directly to element on cpu)
typedef void (*WriteBufferDataFunction)(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

template <class T>
static void OnBindingChanged_Default(T* resource, uint32 prev, uint32 next)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    AssertDebug(resource != nullptr);

    RenderApi_AssignResourceBinding(resource, next);
}

template <class ProxyType>
static void WriteBufferData_Default(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u, "Invalid index for writing buffer data!");

    ProxyType* proxyCasted = static_cast<ProxyType*>(proxy);
    AssertDebug(proxyCasted != nullptr, "Proxy is null!");

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

extern void OnBindingChanged_MeshEntity(Entity* envProbe, uint32 prev, uint32 next);
extern void WriteBufferData_MeshEntity(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next);
extern void OnBindingChanged_AmbientProbe(EnvProbe* envProbe, uint32 prev, uint32 next);
extern void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_EnvGrid(EnvGrid* envGrid, uint32 prev, uint32 next);
extern void WriteBufferData_EnvGrid(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Light(Light* light, uint32 prev, uint32 next);
extern void WriteBufferData_Light(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Material(Material* lightmapVolume, uint32 prev, uint32 next);

extern void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next);

struct ResourceBindings
{
    struct SubtypeResourceBindings
    {
        const HypClass* resourceClass;
        GpuBufferHolderBase* gpuBufferHolder;

        // Element binding index to mapping in CPU memory (only if gpuBufferHolder is not null)
        SparsePagedArray<Pair<uint32, void*>, 1024> indexAndMapping;

        SubtypeResourceBindings(const HypClass* resourceClass, GpuBufferHolderBase* gpuBufferHolder)
            : resourceClass(resourceClass),
              gpuBufferHolder(gpuBufferHolder)
        {
            AssertDebug(resourceClass != nullptr);
        }
    };

    SparsePagedArray<SubtypeResourceBindings, 64> subtypeBindings;

    ResourceBindingAllocator<> meshEntityBindingsAllocator;
    ResourceBinder<Entity, &OnBindingChanged_MeshEntity> meshEntityBinder { &meshEntityBindingsAllocator };

    ResourceBindingAllocator<> cameraBindingsAllocator;
    ResourceBinder<Camera, &OnBindingChanged_Default<Camera>> cameraBinder { &cameraBindingsAllocator };

    // Shared index allocator for reflection probes and sky probes.
    ResourceBindingAllocator<g_maxBoundReflectionProbes> reflectionProbeBindingsAllocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_ReflectionProbe> reflectionProbeBinder {
        &reflectionProbeBindingsAllocator
    };

    // ambient probes bind to their own slot since they don't set image data
    ResourceBindingAllocator<g_maxBoundAmbientProbes> ambientProbeBindingsAllocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_AmbientProbe> ambientProbeBinder { &ambientProbeBindingsAllocator };

    ResourceBindingAllocator<16> envGridBindingsAllocator;
    ResourceBinder<EnvGrid, &OnBindingChanged_EnvGrid> envGridBinder { &envGridBindingsAllocator };

    ResourceBindingAllocator<> lightBindingsAllocator;
    ResourceBinder<Light, &OnBindingChanged_Light> lightBinder { &lightBindingsAllocator };

    ResourceBindingAllocator<> lightmapVolumeBindingsAllocator;
    ResourceBinder<LightmapVolume, &OnBindingChanged_Default<LightmapVolume>> lightmapVolumeBinder {
        &lightmapVolumeBindingsAllocator
    };

    ResourceBindingAllocator<> materialBindingsAllocator;
    ResourceBinder<Material, &OnBindingChanged_Material> materialBinder { &materialBindingsAllocator };

    ResourceBindingAllocator<> textureBindingsAllocator;
    ResourceBinder<Texture, &OnBindingChanged_Texture> textureBinder { &textureBindingsAllocator };

    ResourceBindingAllocator<> skeletonBindingsAllocator;
    ResourceBinder<Skeleton, &OnBindingChanged_Default<Skeleton>> skeletonBinder { &skeletonBindingsAllocator };

    void Assign(HypObjectBase* resource, uint32 binding)
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        AssertDebug(resource != nullptr);

        SubtypeResourceBindings& bindings = GetSubtypeBindings(resource->InstanceClass());

        ObjIdBase resourceId = resource->Id();
        AssertDebug(resourceId.IsValid());

        if (binding == ~0u)
        {
            bindings.indexAndMapping.EraseAt(resourceId.ToIndex());

            return;
        }

        void* cpuMapping = nullptr;

        if (bindings.gpuBufferHolder != nullptr)
        {
            bindings.gpuBufferHolder->EnsureCapacity(binding);

            cpuMapping = bindings.gpuBufferHolder->GetCpuMapping(binding);
            AssertDebug(cpuMapping != nullptr);
        }

        bindings.indexAndMapping.Emplace(resourceId.ToIndex(), binding, cpuMapping);
    }

    Pair<uint32, void*> Retrieve(const HypObjectBase* resource) const
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        if (!resource)
        {
            return Pair<uint32, void*>(~0u, nullptr); // invalid resource
        }

        const SubtypeResourceBindings& bindings = const_cast<ResourceBindings*>(this)->GetSubtypeBindings(resource->InstanceClass());

        const ObjIdBase resourceId = resource->Id();

        const auto* elem = bindings.indexAndMapping.TryGet(resourceId.ToIndex());

        AssertDebug(elem != nullptr, "Failed to retrieve resource binding for resource with ID: {}", resourceId);

        return elem ? *elem : Pair<uint32, void*>(~0u, nullptr);
    }

    SubtypeResourceBindings& GetSubtypeBindings(const HypClass* hypClass)
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        AssertDebug(hypClass != nullptr);

        int staticIndex = hypClass->GetStaticIndex();
        AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *hypClass->GetName());

        SubtypeResourceBindings* bindings = subtypeBindings.TryGet(staticIndex);
        AssertDebug(bindings != nullptr, "No SubtypeBindings container found for {}", hypClass->GetName());

        return *bindings;
    }
};

#pragma endregion ResourceBindings

#pragma region ResourceContainer

struct ResourceData final
{
    HypObjectBase* resource;
    uint32 useCount;

    ResourceData(HypObjectBase* resource)
        : resource(resource),
          useCount(0)
    {
        AssertDebug(resource != nullptr);
    }

    ResourceData(const ResourceData& other) = delete;
    ResourceData& operator=(const ResourceData& other) = delete;

    ResourceData(ResourceData&& other) noexcept = delete;
    ResourceData& operator=(ResourceData&& other) noexcept = delete;

    ~ResourceData() = default;
};

struct ResourceSubtypeData final
{
    TypeId typeId;

    // Map from id -> ResourceData
    SparsePagedArray<ResourceData, 256> data;

    Bitset indicesPendingDelete;
    Bitset indicesPendingUpdate;

    ResourceBinderBase* resourceBinder;
    GpuBufferHolderBase* gpuBufferHolder;

    WriteBufferDataFunction writeBufferDataFn;

    // == optional render proxy data ==
    SparsePagedArray<IRenderProxy*, 1024> proxies;
    bool hasProxyData : 1;

    template <class ResourceType, class ProxyType>
    ResourceSubtypeData(TypeWrapper<ResourceType>, TypeWrapper<ProxyType>,
        GpuBufferHolderBase* gpuBufferHolder = nullptr, ResourceBinderBase* resourceBinder = nullptr,
        WriteBufferDataFunction writeBufferDataFn = nullptr)
        : typeId(TypeId::ForType<ResourceType>()),
          hasProxyData(false),
          gpuBufferHolder(gpuBufferHolder),
          resourceBinder(resourceBinder),
          writeBufferDataFn(writeBufferDataFn)
    {
        // if ProxyType != NullProxy then we setup proxy pool
        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            hasProxyData = true;

            // set the WriteBufferData function pointer to some default if one has not been provided
            if (!writeBufferDataFn)
            {
                this->writeBufferDataFn = &WriteBufferData_Default<ProxyType>;
            }
        }
    }

    ResourceSubtypeData(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData& operator=(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData(ResourceSubtypeData&& other) noexcept = default;
    ResourceSubtypeData& operator=(ResourceSubtypeData&& other) noexcept = default;

    ~ResourceSubtypeData() = default;

    HYP_FORCE_INLINE void SetGpuElem(uint32 idx, IRenderProxy* proxy)
    {
        AssertDebug(writeBufferDataFn != nullptr);
        AssertDebug(gpuBufferHolder != nullptr);
        AssertDebug(idx != ~0u);

        writeBufferDataFn(gpuBufferHolder, idx, proxy);
    }
};

struct ResourceContainer
{
    ResourceSubtypeData& GetSubtypeData(const HypClass* hypClass)
    {
        AssertDebug(hypClass != nullptr);

        int staticIndex = hypClass->GetStaticIndex();
        AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *hypClass->GetName());

        AssertDebug(dataByType.HasIndex(staticIndex), "Missing resource data for {}", *hypClass->GetName());

        return dataByType.Get(staticIndex);
    }

    SparsePagedArray<ResourceSubtypeData, 64> dataByType;
};

struct ResourceContainerFactoryRegistry
{
    Array<Proc<void(ResourceBindings&, ResourceContainer&)>> funcs;

    static ResourceContainerFactoryRegistry& GetInstance()
    {
        static ResourceContainerFactoryRegistry instance;
        return instance;
    }

    void InvokeAll(ResourceBindings& resourceBindings, ResourceContainer& resourceContainer)
    {
        for (auto& func : funcs)
        {
            func(resourceBindings, resourceContainer);
        }
    }
};

template <class ResourceType, class ProxyType>
struct ResourceContainerFactory
{
    static constexpr TypeId typeId = TypeId::ForType<ResourceType>();

    static const HypClass* GetResourceClass()
    {
        return GetClass(typeId);
    }

    template <class ResourceBinderType>
    ResourceContainerFactory(GlobalRenderBuffer buf, ResourceBinderType ResourceBindings::* memResourceBinder,
        WriteBufferDataFunction writeBufferDataFn = nullptr)
    {
        ResourceContainerFactoryRegistry::GetInstance().funcs.PushBack(
            [=](ResourceBindings& resourceBindings, ResourceContainer& container)
            {
                const HypClass* resourceClass = GetResourceClass();
                AssertDebug(resourceClass != nullptr, "Class not found for TypeId '{}'!", typeId.Value());

                const int staticIndex = resourceClass->GetStaticIndex();
                AssertDebug(
                    staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *resourceClass->GetName());

                GpuBufferHolderBase* gpuBufferHolder = buf < GRB_MAX ? g_renderGlobalState->gpuBuffers[buf] : nullptr;
                ResourceBinderType* resourceBinder =
                    memResourceBinder ? &(resourceBindings.*memResourceBinder) : nullptr;

                if (!resourceBindings.subtypeBindings.HasIndex(staticIndex))
                {
                    // add new ResourceSubtypeBindings slot for the given class
                    resourceBindings.subtypeBindings.Emplace(staticIndex, resourceClass, gpuBufferHolder);
                }

                AssertDebug(!container.dataByType.HasIndex(staticIndex),
                    "SubtypeData container already exists for TypeId {} (HypClass: {})! Duplicate DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?",
                    typeId.Value(), *GetClass(typeId)->GetName());

                container.dataByType.Emplace(staticIndex, TypeWrapper<ResourceType>(), TypeWrapper<ProxyType>(),
                    gpuBufferHolder, resourceBinder, writeBufferDataFn);

                HYP_LOG(Rendering, Debug, "Registered resource container for resource class '{}'",
                    *resourceClass->GetName());
            });
    }
};

#define DECLARE_RENDER_DATA_CONTAINER(resourceType, proxyType, ...)                                             \
    static ResourceContainerFactory<class resourceType, class proxyType> g_##resourceType##_container_factory { \
        __VA_ARGS__                                                                                             \
    };

#pragma endregion ResourceContainer

// Render thread owned View data
struct ViewData
{
    View* view = nullptr;
    RenderProxyList rplRender { /* isShared */ false, /* refCounting */ false };
    RenderCollector renderCollector;
    uint32 framesSinceUsed = 0;
    uint32 numRefs = 0; // number of ViewFrameData holding refs to this
};

// Data for views that is buffered over multiple frames
struct ViewFrameData
{
    View* view = nullptr;
    Viewport viewport {};
    RenderProxyList* rplShared = nullptr;

    // Only render thread touches this member, since ViewData is created from the render thread
    ViewData* viewData = nullptr;
};

struct FrameData
{
    HashMap<View*, ViewFrameData*> viewFrameData;

    WorldShaderData worldBufferData {};
    RenderStats renderStats {}; // for game thread to write to and render thread to read from
};

static FrameData g_frameData[g_numFrames];
static HashMap<View*, ViewData*> g_viewData;
static ResourceContainer g_resources;

static ViewData* GetViewData(View* view)
{
    AssertDebug(view != nullptr);

#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

    auto viewDataIt = g_viewData.Find(view);
    if (viewDataIt == g_viewData.End())
    {
        HYP_LOG(Rendering, Debug, "Allocating new ViewData for View {}", view->Id());

        ViewData* vd = new ViewData();
        vd->view = view;

        if (view->GetViewDesc().drawCallCollectionImpl != nullptr)
        {
            vd->renderCollector.drawCallCollectionImpl = view->GetViewDesc().drawCallCollectionImpl;
        }
        else
        {
            vd->renderCollector.drawCallCollectionImpl = GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>();
        }

        AssertDebug(vd->renderCollector.drawCallCollectionImpl != nullptr);

        viewDataIt = g_viewData.Insert(view, vd).first;
    }

    AssertDebug(viewDataIt->second != nullptr);

    viewDataIt->second->framesSinceUsed = 0;

    return viewDataIt->second;
}

void RenderApi_Init()
{
    Threads::AssertOnThread(g_mainThread);

    g_threadFrameIndex = &g_frameIndex[CONSUMER];

    Assert(g_appContext != nullptr, "AppContext must be initialized before RenderApi_Init!");
    Assert(g_renderBackend != nullptr);

    RendererResult result = g_renderBackend->Initialize();
    Assert(result, "Failed to initialize rendering backend: {}", result.GetError().GetMessage());

    { // override global config after renderer initialize
        ConfigurationTable renderGlobalConfigOverrides;

        // if ray tracing is not supported, we need to update the configuration
        if (!g_renderBackend->GetRenderConfig().raytracing)
        {
            renderGlobalConfigOverrides.Set("rendering.raytracing.enabled", false);
            renderGlobalConfigOverrides.Set("rendering.raytracing.reflections.enabled", false);
            renderGlobalConfigOverrides.Set("rendering.raytracing.globalIllumination.enabled", false);
            renderGlobalConfigOverrides.Set("rendering.raytracing.pathTracing.enabled", false);

            UpdateGlobalConfig(renderGlobalConfigOverrides);
        }
    }

    g_renderGlobalState = new RenderGlobalState();
    g_renderGlobalState->materialDescriptorSetManager->CreateFallbackMaterialDescriptorSet();

    ResourceContainerFactoryRegistry& registry = ResourceContainerFactoryRegistry::GetInstance();
    registry.InvokeAll(*g_renderGlobalState->resourceBindings, g_resources);

    registry.funcs.Clear();
}

void RenderApi_Shutdown()
{
    Threads::AssertOnThread(g_mainThread);

    for (uint32 i = 0; i < g_numFrames; i++)
    {
        for (auto& it : g_frameData[i].viewFrameData)
        {
            delete it.second;
        }

        g_frameData[i].viewFrameData.Clear();
    }

    for (auto& it : g_viewData)
    {
        delete it.second;
    }

    g_viewData.Clear();

    delete g_renderGlobalState;
    g_renderGlobalState = nullptr;

    Assert(g_renderBackend->Destroy());
}

static inline int RenderApi_CurrentThreadType()
{
    const ThreadId& threadId = Threads::CurrentThreadId();

    if (threadId == g_renderThread)
    {
        return CONSUMER;
    }

    if (threadId == g_gameThread)
    {
        return PRODUCER;
    }

    // invalid
    return -1;
}

uint32 RenderApi_GetFrameIndex()
{
    if (!g_threadFrameIndex)
    {
        const int threadType = RenderApi_CurrentThreadType();
        Assert(threadType >= 0, "RenderApi_GetFrameIndex called from an invalid thread!");

        g_threadFrameIndex = &g_frameIndex[threadType];
    }

    return AtomicAdd(g_threadFrameIndex, 0);
}

uint32 RenderApi_GetFrameCounter()
{
    return (uint32)AtomicAdd(&g_frameCounter, 0);
}

static ViewFrameData* GetViewFrameData(View* view, uint32 slot)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    ViewFrameData*& vfd = g_frameData[slot].viewFrameData[view];

    if (!vfd)
    {
        vfd = new ViewFrameData();
        vfd->view = view;

        vfd->rplShared = view->GetRenderProxyList(slot);
        AssertDebug(vfd->rplShared != nullptr);
        AssertDebug(vfd->rplShared->isShared, "Expected isShared to be true to ensure multiple threads don't access the list concurrently");
    }

    return vfd;
}

/// Conditionally copy RenderProxy data to global state, if proxyVersion is greater than the current held version.
template <class ElementType, class ProxyType>
static HYP_FORCE_INLINE void CopyRenderProxy(ResourceSubtypeData& subtypeData, const ObjId<ElementType>& id, ProxyType* pNewProxy)
{
    AssertDebug(pNewProxy != nullptr);

    const uint32 idx = id.ToIndex();

    AssertDebug(subtypeData.typeId == id.GetTypeId(),
        "Attempting to use ID for type {} as index into proxy collection that requires index type {}",
        LookupTypeName(id.GetTypeId()),
        LookupTypeName(subtypeData.typeId));

    subtypeData.proxies.Set(idx, static_cast<IRenderProxy*>(pNewProxy));

    subtypeData.indicesPendingUpdate.Set(idx, true);
}

template <class ElementType, class ProxyType>
static HYP_FORCE_INLINE void SyncResourcesImpl(
    ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>& resourceTracker,
    const typename ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>::Impl& impl)
{
    if (impl.elements.Empty())
    {
        return;
    }

    for (Bitset::BitIndex i : impl.next)
    {
        ElementType* elem = impl.elements.Get(i);
        const int version = impl.versions.Get(i);

        resourceTracker.Track(elem->Id(), elem, &version);
    }
}

template <class ElementType, class ProxyType>
static inline void SyncResources(
    ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>* pLhs,
    ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>* pRhs)
{
    AssertDebug(pLhs != nullptr && pRhs != nullptr);

    auto& lhs = *pLhs;
    auto& rhs = *pRhs;

    lhs.Advance();

    SyncResourcesImpl(lhs, rhs.GetSubclassImpl(-1));

    for (Bitset::BitIndex subclassIndex : rhs.GetSubclassIndices())
    {
        SyncResourcesImpl(lhs, rhs.GetSubclassImpl(int(subclassIndex)));
    }

    const ResourceTrackerDiff& diff = lhs.GetDiff();

    if (!diff.NeedsUpdate())
    {
        return;
    }

    Array<ElementType*> removed;
    lhs.GetRemoved(removed, false);

    Array<ElementType*> added;
    lhs.GetAdded(added, false);

    for (ElementType* pResource : added)
    {
        AssertDebug(pResource != nullptr);

        const ObjId<ElementType> resourceId = pResource->Id();
        AssertDebug(resourceId.IsValid());

        ResourceSubtypeData& subtypeData = g_resources.GetSubtypeData(pResource->InstanceClass());
        AssertDebug(resourceId.GetTypeId() == subtypeData.typeId);

        ResourceData* rd = subtypeData.data.TryGet(resourceId.ToIndex());

        if (!rd)
        {
            rd = &*subtypeData.data.Emplace(resourceId.ToIndex(), pResource);
        }

        subtypeData.indicesPendingDelete.Set(resourceId.ToIndex(), false);

        ++rd->useCount;

        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            ProxyType* proxy = rhs.GetProxy(resourceId);
            AssertDebug(proxy != nullptr);

            if (!proxy)
            {
                continue;
            }

            lhs.SetProxy(resourceId, *proxy);

            CopyRenderProxy(subtypeData, resourceId, proxy);
        }
    }

    for (ElementType* pResource : removed)
    {
        AssertDebug(pResource != nullptr);

        const ObjId<ElementType> resourceId = pResource->Id();
        AssertDebug(resourceId.IsValid());

        ResourceSubtypeData& subtypeData = g_resources.GetSubtypeData(pResource->InstanceClass());
        AssertDebug(resourceId.GetTypeId() == subtypeData.typeId);

        ResourceData* rd = subtypeData.data.TryGet(resourceId.ToIndex());
        AssertDebug(rd != nullptr, "No resource data for {}", resourceId);

        if (!rd)
        {
            continue;
        }

        AssertDebug(rd->useCount != 0);

        if (!(--rd->useCount))
        {
            subtypeData.indicesPendingDelete.Set(resourceId.ToIndex(), true);
        }
    }

    Array<ElementType*> changed;

    if constexpr (!std::is_same_v<ProxyType, NullProxy>)
    {
        lhs.GetChanged(changed);

        if (changed.Any())
        {
            for (ElementType* pResource : changed)
            {
                ObjId<ElementType> resourceId = pResource->Id();

                ProxyType* proxy = rhs.GetProxy(resourceId);
                AssertDebug(proxy != nullptr);

                if (!proxy)
                {
                    continue;
                }

                lhs.SetProxy(resourceId, *proxy);

                ResourceSubtypeData& subtypeData = g_resources.GetSubtypeData(pResource->InstanceClass());

                CopyRenderProxy(subtypeData, resourceId, proxy);
            }
        }
    }

    //    if (added.Any() || removed.Any() || changedIds.Any())
    //    {
    //        HYP_LOG_TEMP("Updated resources for {}: added={}, removed={}, changed={}",
    //            TypeNameWithoutNamespace<ElementType>().Data(),
    //            added.Size(), removed.Size(), changedIds.Size());
    //    }
}

template <SizeType... Indices>
static HYP_FORCE_INLINE void SyncResourcesT(ResourceTrackerBase** dstResourceTrackers, ResourceTrackerBase** srcResourceTrackers, std::index_sequence<Indices...>)
{
    (SyncResources(
         static_cast<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type*>(dstResourceTrackers[Indices]),
         static_cast<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type*>(srcResourceTrackers[Indices])),
        ...);
}

static HYP_FORCE_INLINE void CopyDependencies(ViewData& vd, RenderProxyList& rpl)
{
    AssertDebug(vd.rplRender.resourceTrackers.Size() == TupleSize<RenderProxyList::ResourceTrackerTypes>::value);
    AssertDebug(rpl.resourceTrackers.Size() == TupleSize<RenderProxyList::ResourceTrackerTypes>::value);

    SyncResourcesT(vd.rplRender.resourceTrackers.Data(), rpl.resourceTrackers.Data(), std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());

    if (rpl.useOrdering)
    {
        vd.rplRender.meshEntityOrdering = rpl.meshEntityOrdering;
    }
}

RenderProxyList& RenderApi_GetProducerProxyList(View* view)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_gameThread);
#endif

    ViewFrameData* vd = GetViewFrameData(view, g_frameIndex[PRODUCER]);

    return *vd->rplShared;
}

RenderProxyList& RenderApi_GetConsumerProxyList(View* view)
{
    AssertDebug(view != nullptr);

#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

    return GetViewData(view)->rplRender;
}

RenderCollector& RenderApi_GetRenderCollector(View* view)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    return GetViewData(view)->renderCollector;
}

Array<Pair<View*, RenderCollector*>> RenderApi_GetAllRenderCollectors()
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    Array<Pair<View*, RenderCollector*>> result;

    for (auto& it : g_viewData)
    {
        result.PushBack(Pair<View*, RenderCollector*>(it.first, &it.second->renderCollector));
    }

    return result;
}

IRenderProxy* RenderApi_GetRenderProxy(const HypObjectBase* resource)
{
    AssertDebug(resource != nullptr);

    Threads::AssertOnThread(g_renderThread);

    ResourceSubtypeData& subtypeData = g_resources.GetSubtypeData(resource->InstanceClass());
    AssertDebug(subtypeData.hasProxyData,
        "Cannot use GetRenderProxy() for type which does not have a RenderProxy! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    const ObjIdBase resourceId = resource->Id();
    AssertDebug(resourceId.GetTypeId() == subtypeData.typeId);

    if (!subtypeData.proxies.HasIndex(resourceId.ToIndex()))
    {
        HYP_LOG(Rendering, Warning, "No render proxy found for resource: {}", resourceId);

        return nullptr; // no proxy for this resource
    }

    IRenderProxy* pProxy = subtypeData.proxies.Get(resourceId.ToIndex());
    AssertDebug(pProxy != nullptr);

    return pProxy;
}

void RenderApi_UpdateGpuData(const HypObjectBase* resource)
{
    AssertDebug(resource != nullptr);

    Threads::AssertOnThread(g_renderThread);

    const ObjIdBase resourceId = resource->Id();

    ResourceSubtypeData& subtypeData = g_resources.GetSubtypeData(resource->InstanceClass());
    AssertDebug(resourceId.GetTypeId() == subtypeData.typeId);

    AssertDebug(subtypeData.gpuBufferHolder != nullptr,
        "Cannot update GPU data for type which does not have a GpuBufferHolder! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    AssertDebug(subtypeData.hasProxyData,
        "Cannot use UpdateGpuData() for type which does not have a RenderProxy! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    const Pair<uint32, void*> bindingData = g_renderGlobalState->resourceBindings->Retrieve(resource);
    AssertDebug(bindingData.first != ~0u && bindingData.second != nullptr);

    const uint32 idx = resourceId.ToIndex();

    IRenderProxy* pProxy = subtypeData.proxies.Get(idx);
    AssertDebug(pProxy != nullptr);

    subtypeData.SetGpuElem(bindingData.first, pProxy);

    // set it as no longer needing update next frame since we updated immediately
    subtypeData.indicesPendingUpdate.Set(idx, false);
}

void RenderApi_AssignResourceBinding(HypObjectBase* resource, uint32 binding)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    g_renderGlobalState->resourceBindings->Assign(resource, binding);
}

uint32 RenderApi_RetrieveResourceBinding(const HypObjectBase* resource)
{
#ifdef HYP_DEBUG_MODE
    // FIXME: Add better check to ensure it is from a render task thread.
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

    return g_renderGlobalState->resourceBindings->Retrieve(resource).first;
}

WorldShaderData* RenderApi_GetWorldBufferData()
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_gameThread | g_renderThread);
#endif

    return &g_frameData[AtomicAdd(g_threadFrameIndex, 0)].worldBufferData;
}

Viewport& RenderApi_GetViewport(View* view)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_gameThread | g_renderThread);
#endif

    return GetViewFrameData(view, AtomicAdd(g_threadFrameIndex, 0))->viewport;
}

RenderStats* RenderApi_GetRenderStats()
{
    if (Threads::IsOnThread(g_renderThread))
    {
        return &g_renderStats;
    }

#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_gameThread);
#endif

    return &g_frameData[AtomicAdd(g_threadFrameIndex, 0)].renderStats;
}

void RenderApi_AddRenderStats(const RenderStatsCounts& counts)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    g_renderStatsCalculator.AddCounts(counts);
}

void RenderApi_SuppressRenderStats()
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    g_renderStatsCalculator.Suppress();
}

void RenderApi_UnsuppressRenderStats()
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    g_renderStatsCalculator.Unsuppress();
}

void RenderApi_BeginFrame_GameThread()
{
    HYP_SCOPE;

    g_threadFrameIndex = &g_frameIndex[PRODUCER];

    g_freeSemaphore.acquire();
}

void RenderApi_EndFrame_GameThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_gameThread);
#endif

    int32 slot = AtomicAdd(&g_frameIndex[PRODUCER], 0);

    FrameData& frameData = g_frameData[slot];

    while (!AtomicCompareExchange(&g_frameIndex[PRODUCER], slot, (slot + 1) % g_numFrames))
    {
        HYP_FAIL("Data race !");
    }

    g_fullSemaphore.release();
}

void RenderApi_BeginFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    g_fullSemaphore.acquire();

    int32 slot = AtomicAdd(&g_frameIndex[CONSUMER], 0);

    FrameData& fd = g_frameData[slot];

    HYP_GFX_ASSERT(RenderCommands::Flush());

    for (auto it = fd.viewFrameData.Begin(); it != fd.viewFrameData.End(); ++it)
    {
        ViewFrameData& vfd = *it->second;
        AssertDebug(vfd.rplShared != nullptr);

        if (!vfd.viewData)
        {
            vfd.viewData = GetViewData(vfd.view);
            ++vfd.viewData->numRefs;
        }

        vfd.rplShared->BeginRead();

#ifdef HYP_DEBUG_MODE
        vfd.rplShared->debugIsSynced = true;
#endif

        AssertDebug(vfd.rplShared->debugIsDestroyed == false, "RenderProxyList for view {} has been destroyed!", vfd.view->Id());

        // copy dependencies from shared to ViewData
        CopyDependencies(*vfd.viewData, *vfd.rplShared);

        vfd.rplShared->EndRead();
    }

    for (ResourceSubtypeData& subtypeData : g_resources.dataByType)
    {
        if (subtypeData.resourceBinder)
        {
            for (ResourceData& elem : subtypeData.data)
            {
                AssertDebug(elem.resource != nullptr);
                subtypeData.resourceBinder->Consider(elem.resource);
            }
        }
    }

    // assign the actual bindings:
    /// TODO: This should be done in the ResourceBinder itself, not here.
    g_renderGlobalState->resourceBindings->meshEntityBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->cameraBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->ambientProbeBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->reflectionProbeBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->envGridBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->lightBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->lightmapVolumeBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->materialBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->textureBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->skeletonBinder.ApplyUpdates();

    //    HYP_LOG(Rendering, Debug, "Mesh entities: {} bound",
    //        g_renderGlobalState->resourceBindings->meshEntityBinder.TotalBoundResources());
    //    HYP_LOG(Rendering, Debug, "Ambient probes: {} bound",
    //        g_renderGlobalState->resourceBindings->ambientProbeBinder.TotalBoundResources());
    //    HYP_LOG(Rendering, Debug, "Reflection probes: {} bound",
    //        g_renderGlobalState->resourceBindings->reflectionProbeBinder.TotalBoundResources());
    //    HYP_LOG(Rendering, Debug, "Env grids: {} bound",
    //        g_renderGlobalState->resourceBindings->envGridBinder.TotalBoundResources());
    //    HYP_LOG(
    //        Rendering, Debug, "Lights: {} bound", g_renderGlobalState->resourceBindings->lightBinder.TotalBoundResources());
    //    HYP_LOG(Rendering, Debug, "Lightmap volumes: {} bound",
    //        g_renderGlobalState->resourceBindings->lightmapVolumeBinder.TotalBoundResources());
    //    HYP_LOG(Rendering, Debug, "Materials: {} bound",
    //        g_renderGlobalState->resourceBindings->materialBinder.TotalBoundResources());
    //    HYP_LOG(Rendering, Debug, "Textures: {} bound",
    //        g_renderGlobalState->resourceBindings->textureBinder.TotalBoundResources());
    //    HYP_LOG(Rendering, Debug, "Skeletons: {} bound",
    //        g_renderGlobalState->resourceBindings->skeletonBinder.TotalBoundResources());

    // Build draw call lists

    for (auto it = fd.viewFrameData.Begin(); it != fd.viewFrameData.End(); ++it)
    {
        ViewFrameData& vfd = *it->second;
        AssertDebug(vfd.rplShared != nullptr);
        AssertDebug(vfd.viewData != nullptr);

        ViewData& vd = *vfd.viewData;

        if (vfd.rplShared->disableBuildRenderCollection || (vfd.view->GetFlags() & ViewFlags::NO_DRAW_CALLS))
        {
            continue;
        }

        vd.rplRender.BeginRead();

        vd.renderCollector.BuildRenderGroups(vd.view, vd.rplRender);

        /// TODO: Use View's bucket mask property to pass to BuildDrawCalls().
        vd.renderCollector.BuildDrawCalls(0);

        vd.rplRender.EndRead();
    }

    for (ResourceSubtypeData& subtypeData : g_resources.dataByType)
    {
        if (subtypeData.indicesPendingUpdate.Count() != 0)
        {
            AssertDebug(subtypeData.resourceBinder != nullptr);

            const Bitset& currentBoundIndices = subtypeData.resourceBinder->GetBoundIndices(subtypeData.typeId);

            if (currentBoundIndices.Count() == 0)
            {
                // early out; nothing is bound.
                continue;
            }

            // Handle proxies that were updated on game thread
            for (Bitset::BitIndex i = subtypeData.indicesPendingUpdate.FirstSetBitIndex(); i != Bitset::notFound;
                i = subtypeData.indicesPendingUpdate.NextSetBitIndex(i + 1))
            {
                if (!currentBoundIndices.Test(i))
                {
                    continue;
                }

                HypObjectBase* resource = subtypeData.data.Get(i).resource;

                AssertDebug(subtypeData.hasProxyData);
                AssertDebug(subtypeData.writeBufferDataFn != nullptr);

                const Pair<uint32, void*> bindingData = g_renderGlobalState->resourceBindings->Retrieve(resource);
                AssertDebug(bindingData.first != ~0u && bindingData.second != nullptr,
                    "Failed to retrieve binding for resource: {} in frame {}, but it is marked as bound (index: {})",
                    i, slot, i);

                IRenderProxy* pProxy = subtypeData.proxies.Get(i);
                AssertDebug(pProxy != nullptr);

                subtypeData.SetGpuElem(bindingData.first, pProxy);

                subtypeData.indicesPendingUpdate.Set(i, false);
            }
        }
    }
}

void RenderApi_EndFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_renderThread);
#endif

    int32 slot = AtomicAdd(&g_frameIndex[CONSUMER], 0);

    FrameData& frameData = g_frameData[slot];

    // cull ViewData that hasn't been written to for a while, as well as remove unused render groups.
    for (auto it = frameData.viewFrameData.Begin(); it != frameData.viewFrameData.End();)
    {
        ViewFrameData& vfd = *it->second;
        AssertDebug(vfd.viewData != nullptr);

        ViewData& vd = *vfd.viewData;

        View* view = vd.view;
        AssertDebug(view != nullptr);

        vd.renderCollector.RemoveEmptyRenderGroups();

        // Clear out data for views that haven't been written to for a while
        if (++vd.framesSinceUsed == g_maxFramesBeforeDiscard)
        {
            HYP_LOG(Rendering, Debug, "Discarding ViewData for view {} after {} frames",
                view->Id(), g_maxFramesBeforeDiscard);

            // Decrement ref count on the ViewData,
            // if we hit zero there are no more ViewFrameData holding refs to the ViewData so we delete it
            AssertDebug(vd.numRefs > 0);

            if ((--vd.numRefs) == 0)
            {
                auto viewDataIt = g_viewData.Find(view);
                AssertDebug(viewDataIt != g_viewData.End() && viewDataIt->second == &vd);

                g_viewData.Erase(viewDataIt);

                delete &vd;
            }

#ifdef HYP_DEBUG_MODE
            vfd.rplShared->debugIsSynced = false;
#endif

            delete &vfd;

            it = frameData.viewFrameData.Erase(it);

            continue;
        }

        ++it;
    }

    int numCleanupCycles = g_frameCleanupBudget;
    numCleanupCycles -= g_renderGlobalState->mainRenderer->RunCleanupCycle(numCleanupCycles);

    for (uint32 i = 0; i < GRT_MAX && numCleanupCycles > 0; i++)
    {
        for (uint32 j = 0; j < g_renderGlobalState->globalRenderers[i].Size() && numCleanupCycles > 0; j++)
        {
            if (RendererBase* renderer = g_renderGlobalState->globalRenderers[i][j])
            {
                numCleanupCycles -= renderer->RunCleanupCycle(numCleanupCycles);
            }
        }
    }

    numCleanupCycles -= g_renderGlobalState->graphicsPipelineCache->RunCleanupCycle(16);

    for (ResourceSubtypeData& subtypeData : g_resources.dataByType)
    {
        for (Bitset::BitIndex i : subtypeData.indicesPendingDelete)
        {
            ResourceData& rd = subtypeData.data.Get(i);
            AssertDebug(rd.resource != nullptr);
            AssertDebug(rd.useCount == 0, "Use count should be 0 before deletion");

            // if we delete it, we want to make sure it is not in marked for update state (don't want to iterate over
            // dead items)
            subtypeData.indicesPendingUpdate.Set(i, false);

            subtypeData.resourceBinder->Deconsider(rd.resource);

            // Swap refcount owner over to the Handle
            AnyHandle resource { rd.resource };
            subtypeData.data.EraseAt(i);

            if (subtypeData.hasProxyData)
            {
                AssertDebug(subtypeData.proxies.HasIndex(i), "Proxy missing for resource {}", resource.Id());

                IRenderProxy* pProxy = subtypeData.proxies.Get(i);
                AssertDebug(pProxy != nullptr);

                HYP_LOG(Rendering, Debug, "Deleting render proxy for resource id {} at index {} for frame {}",
                    resource.Id(), i, slot);

                subtypeData.proxies.EraseAt(i);
            }

            // safely release all the held resources:
            //            if (resource.IsValid())
            //            {
            //                g_safeDeleter->SafeDelete(std::move(resource));
            //            }
            resource.Reset();
        }

        subtypeData.indicesPendingDelete.Clear();
    }

    g_safeDeleter->UpdateEntryListQueue();
    
    // update render stats and copy to frame data so the game thread can read it
    // do this after calling UpdateEntryListQueue() on SafeDeleter so we can get the total
    // number of deletion queue items for our stats
    g_renderStatsCalculator.Advance(g_renderStats);
    frameData.renderStats = g_renderStats;
    
    g_safeDeleter->Iterate();
    
    while (!AtomicCompareExchange(&g_frameIndex[CONSUMER], slot, (slot + 1) % g_numFrames))
    {
        HYP_FAIL("Data race !");
    }

    AtomicIncrement(&g_frameCounter);

    g_freeSemaphore.release();
}

#pragma region RenderGlobalState

RenderGlobalState::RenderGlobalState()
    : shadowMapAllocator(MakeUnique<ShadowMapAllocator>()),
      gpuBufferHolders(MakeUnique<GpuBufferHolderMap>()),
      placeholderData(MakeUnique<PlaceholderData>()),
      resourceBindings(new ResourceBindings()),
      materialDescriptorSetManager(new MaterialDescriptorSetManager()),
      graphicsPipelineCache(new GraphicsPipelineCache()),
      bindlessStorage(new BindlessStorage())
{
    gpuBuffers.buffers[GRB_WORLDS] = gpuBufferHolders->GetOrCreate<WorldShaderData, GpuBufferType::CBUFF>(1);
    gpuBuffers.buffers[GRB_CAMERAS] = gpuBufferHolders->GetOrCreate<CameraShaderData, GpuBufferType::CBUFF>();
    gpuBuffers.buffers[GRB_LIGHTS] = gpuBufferHolders->GetOrCreate<LightShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_ENTITIES] = gpuBufferHolders->GetOrCreate<EntityShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_MATERIALS] = gpuBufferHolders->GetOrCreate<MaterialShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_SKELETONS] = gpuBufferHolders->GetOrCreate<SkeletonShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_ENV_PROBES] = gpuBufferHolders->GetOrCreate<EnvProbeShaderData, GpuBufferType::SSBO>();
    gpuBuffers.buffers[GRB_ENV_GRIDS] = gpuBufferHolders->GetOrCreate<EnvGridShaderData, GpuBufferType::CBUFF>();
    gpuBuffers.buffers[GRB_LIGHTMAP_VOLUMES] = gpuBufferHolders->GetOrCreate<LightmapVolumeShaderData, GpuBufferType::SSBO>();

#ifdef HYP_DEBUG_MODE
    for (int i = 0; i < HYP_ARRAY_SIZE(gpuBuffers.buffers); i++)
    {
        if (!gpuBuffers.buffers[i])
        {
            continue;
        }

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const GpuBufferRef& buffer = gpuBuffers.buffers[i]->GetBuffer(frameIndex);
            AssertDebug(buffer.IsValid());

            buffer->SetDebugName(CreateNameFromDynamicString(EnumToString(GlobalRenderBuffer(i))));
        }
    }
#endif

    globalDescriptorTable = g_renderBackend->MakeDescriptorTable(&GetStaticDescriptorTableDeclaration());

    placeholderData->Create();
    shadowMapAllocator->Initialize();

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        SetDefaultDescriptorSetElements(frameIndex);
    }

    CreateSphereSamplesBuffer();
    CreateBlueNoiseBuffer();

    globalDescriptorTable->Create();

    mainRenderer = new DeferredRenderer();
    mainRenderer->Initialize();

    Memory::MemSet(globalRenderers, 0, sizeof(globalRenderers));

    globalRenderers[GRT_ENV_PROBE].ResizeZeroed(EPT_MAX);
    globalRenderers[GRT_ENV_PROBE][EPT_REFLECTION] = new ReflectionProbeRenderer();
    globalRenderers[GRT_ENV_PROBE][EPT_SKY] = new ReflectionProbeRenderer();

    globalRenderers[GRT_ENV_GRID].PushBack(new EnvGridRenderer());

    globalRenderers[GRT_SHADOW_MAP].ResizeZeroed(LT_MAX); // 1 ShadowMapRenderer per LightType
    globalRenderers[GRT_SHADOW_MAP][LT_POINT] = new PointShadowRenderer();
    globalRenderers[GRT_SHADOW_MAP][LT_DIRECTIONAL] = new DirectionalShadowRenderer();
}

RenderGlobalState::~RenderGlobalState()
{
    delete resourceBindings;

    bindlessStorage->UnsetAllResources();
    delete bindlessStorage;
    bindlessStorage = nullptr;

    shadowMapAllocator->Destroy();
    placeholderData->Destroy();

    globalDescriptorTable.Reset();

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

    delete materialDescriptorSetManager;
    materialDescriptorSetManager = nullptr;

    delete graphicsPipelineCache;
    graphicsPipelineCache = nullptr;

    mainRenderer->Shutdown();
    delete mainRenderer;
    mainRenderer = nullptr;
}

void RenderGlobalState::UpdateBuffers(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    for (auto& it : gpuBufferHolders->GetItems())
    {
        it.second->UpdateBufferSize(frame->GetFrameIndex());
        it.second->UpdateBufferData(frame->GetFrameIndex());
    }
}

void RenderGlobalState::AddRenderer(GlobalRendererType globalRendererType, RendererBase* renderer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(!globalRenderers[globalRendererType].Contains(renderer));

    globalRenderers[globalRendererType].PushBack(renderer);
}

void RenderGlobalState::RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(globalRenderers[globalRendererType].Contains(renderer));

    delete renderer;

    globalRenderers[globalRendererType].Erase(renderer);
}

void RenderGlobalState::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    static_assert(sizeof(BlueNoiseBuffer::sobol256spp256d) == sizeof(BlueNoise::sobol256spp256d));
    static_assert(sizeof(BlueNoiseBuffer::scramblingTile) == sizeof(BlueNoise::scramblingTile));
    static_assert(sizeof(BlueNoiseBuffer::rankingTile) == sizeof(BlueNoise::rankingTile));

    constexpr SizeType blueNoiseBufferSize = sizeof(BlueNoiseBuffer);

    constexpr SizeType sobol256spp256dOffset = offsetof(BlueNoiseBuffer, sobol256spp256d);
    constexpr SizeType sobol256spp256dSize = sizeof(BlueNoise::sobol256spp256d);
    constexpr SizeType scramblingTileOffset = offsetof(BlueNoiseBuffer, scramblingTile);
    constexpr SizeType scramblingTileSize = sizeof(BlueNoise::scramblingTile);
    constexpr SizeType rankingTileOffset = offsetof(BlueNoiseBuffer, rankingTile);
    constexpr SizeType rankingTileSize = sizeof(BlueNoise::rankingTile);

    static_assert(blueNoiseBufferSize
        == (sobol256spp256dOffset + sobol256spp256dSize)
            + ((scramblingTileOffset - (sobol256spp256dOffset + sobol256spp256dSize)) + scramblingTileSize)
            + ((rankingTileOffset - (scramblingTileOffset + scramblingTileSize)) + rankingTileSize));

    GpuBufferRef blueNoiseBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(BlueNoiseBuffer));
    HYP_GFX_ASSERT(blueNoiseBuffer->Create());
    blueNoiseBuffer->Copy(sobol256spp256dOffset, sobol256spp256dSize, &BlueNoise::sobol256spp256d[0]);
    blueNoiseBuffer->Copy(scramblingTileOffset, scramblingTileSize, &BlueNoise::scramblingTile[0]);
    blueNoiseBuffer->Copy(rankingTileOffset, rankingTileSize, &BlueNoise::rankingTile[0]);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
            ->SetElement("BlueNoiseBuffer", blueNoiseBuffer);
    }
}

void RenderGlobalState::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;

    GpuBufferRef sphereSamplesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(Vec4f) * 4096);
    HYP_GFX_ASSERT(sphereSamplesBuffer->Create());

    Vec4f* sphereSamples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++)
    {
        Vec3f sample = MathUtil::RandomInSphere(
            Vec3f { MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed), MathUtil::RandomFloat(seed) });

        sphereSamples[i] = Vec4f(sample, 0.0f);
    }

    sphereSamplesBuffer->Copy(sizeof(Vec4f) * 4096, sphereSamples);

    delete[] sphereSamples;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
            ->SetElement("SphereSamplesBuffer", sphereSamplesBuffer);
    }
}

void RenderGlobalState::SetDefaultDescriptorSetElements(uint32 frameIndex)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // Global
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("WorldsBuffer", gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("LightsBuffer", gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("CurrentLight", gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("ObjectsBuffer", gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("CamerasBuffer", gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("EnvGridsBuffer", gpuBuffers[GRB_ENV_GRIDS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("EnvProbesBuffer", gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("CurrentEnvProbe", gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("VoxelGridTexture", placeholderData->GetImageView3D1x1x1R8());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("LightFieldColorTexture", placeholderData->GetImageView2D1x1R8());
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("LightFieldDepthTexture", placeholderData->GetImageView2D1x1R8());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("BlueNoiseBuffer", GpuBufferRef::Null());
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("SphereSamplesBuffer", GpuBufferRef::Null());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("LightmapVolumesBuffer", gpuBuffers[GRB_LIGHTMAP_VOLUMES]->GetBuffer(frameIndex));

    for (uint32 i = 0; i < g_maxBoundReflectionProbes; i++)
    {
        globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
            ->SetElement(
                NAME("EnvProbeTextures"), i, g_renderBackend->GetTextureImageView(placeholderData->defaultTexture2d));
    }

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("DDGIUniforms",
            placeholderData->GetOrCreateBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms), true /* exact size */));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("DDGIIrradianceTexture", placeholderData->GetImageView2D1x1R8());
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("DDGIDepthTexture", placeholderData->GetImageView2D1x1R8());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("RTRadianceResultTexture", placeholderData->GetImageView2D1x1R8());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("SamplerNearest", placeholderData->GetSamplerNearest());
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("SamplerLinear", placeholderData->GetSamplerLinearMipmap());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("UITexture", placeholderData->GetImageView2D1x1R8());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("FinalOutputTexture", placeholderData->GetImageView2D1x1R8());

    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("ShadowMapsTextureArray", shadowMapAllocator->GetAtlasImageView());
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("PointLightShadowMapsTextureArray", shadowMapAllocator->GetPointLightShadowMapImageView());

    // Object
    globalDescriptorTable->GetDescriptorSet("Object", frameIndex)
        ->SetElement("CurrentObject", gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Object", frameIndex)
        ->SetElement("MaterialsBuffer", gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Object", frameIndex)
        ->SetElement("SkeletonsBuffer", gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Object", frameIndex)
        ->SetElement("LightmapVolumeIrradianceTexture", placeholderData->GetImageView2D1x1R8());
    globalDescriptorTable->GetDescriptorSet("Object", frameIndex)
        ->SetElement("LightmapVolumeRadianceTexture", placeholderData->GetImageView2D1x1R8());

    // Material
    if (g_renderBackend->GetRenderConfig().bindlessTextures)
    {
        for (uint32 textureIndex = 0; textureIndex < g_maxBindlessResources; textureIndex++)
        {
            globalDescriptorTable->GetDescriptorSet("Material", frameIndex)
                ->SetElement("Textures", textureIndex,
                    g_renderBackend->GetTextureImageView(placeholderData->defaultTexture2d));
        }
    }
    else
    {
        for (uint32 textureIndex = 0; textureIndex < g_maxBoundTextures; textureIndex++)
        {
            globalDescriptorTable->GetDescriptorSet("Material", frameIndex)
                ->SetElement("Textures", textureIndex,
                    g_renderBackend->GetTextureImageView(placeholderData->defaultTexture2d));
        }
    }
}

#pragma endregion RenderGlobalState

DECLARE_RENDER_DATA_CONTAINER(Entity, RenderProxyMesh, GRB_ENTITIES, &ResourceBindings::meshEntityBinder, &WriteBufferData_MeshEntity);

DECLARE_RENDER_DATA_CONTAINER(Camera, RenderProxyCamera, GRB_CAMERAS, &ResourceBindings::cameraBinder);

DECLARE_RENDER_DATA_CONTAINER(EnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &ResourceBindings::envGridBinder, &WriteBufferData_EnvGrid);
DECLARE_RENDER_DATA_CONTAINER(LegacyEnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &ResourceBindings::envGridBinder, &WriteBufferData_EnvGrid);

// FIXME: Overlap with ambient probes / reflection and sky probes causing issues where indices are overlapping,
// due to using same bindings allocator.
DECLARE_RENDER_DATA_CONTAINER(ReflectionProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::reflectionProbeBinder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(SkyProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::reflectionProbeBinder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::ambientProbeBinder, &WriteBufferData_EnvProbe);

DECLARE_RENDER_DATA_CONTAINER(Light, RenderProxyLight, GRB_LIGHTS, &ResourceBindings::lightBinder, &WriteBufferData_Light);
DECLARE_RENDER_DATA_CONTAINER(DirectionalLight, RenderProxyLight, GRB_LIGHTS, &ResourceBindings::lightBinder, &WriteBufferData_Light);
DECLARE_RENDER_DATA_CONTAINER(PointLight, RenderProxyLight, GRB_LIGHTS, &ResourceBindings::lightBinder, &WriteBufferData_Light);
DECLARE_RENDER_DATA_CONTAINER(AreaRectLight, RenderProxyLight, GRB_LIGHTS, &ResourceBindings::lightBinder, &WriteBufferData_Light);
DECLARE_RENDER_DATA_CONTAINER(SpotLight, RenderProxyLight, GRB_LIGHTS, &ResourceBindings::lightBinder, &WriteBufferData_Light);

DECLARE_RENDER_DATA_CONTAINER(LightmapVolume, RenderProxyLightmapVolume, GRB_LIGHTMAP_VOLUMES, &ResourceBindings::lightmapVolumeBinder);

DECLARE_RENDER_DATA_CONTAINER(Material, RenderProxyMaterial, GRB_MATERIALS, &ResourceBindings::materialBinder);

DECLARE_RENDER_DATA_CONTAINER(Texture, NullProxy, GRB_INVALID, &ResourceBindings::textureBinder);

DECLARE_RENDER_DATA_CONTAINER(Skeleton, RenderProxySkeleton, GRB_SKELETONS, &ResourceBindings::skeletonBinder);

} // namespace hyperion

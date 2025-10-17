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
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderStats.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderMemory.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/env_grid/EnvGridRenderer.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowRenderer.hpp>

#include <rendering/rt/DDGI.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/animation/Skeleton.hpp>

#include <core/reflection/HypClass.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/threading/Semaphore.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

// for EnumToString
#include <core/reflection/HypEnum.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/BlueNoise.hpp>

#include <engine/EngineGlobals.hpp>

#include <system/AppContext.hpp>

#include <HyperionEngine.hpp>

#include <semaphore>

namespace hyperion {

static_assert(NumMultiBuffers <= MinSafeDeleteCycles,
    "NumMultiBuffers must be less than or equal to MinSafeDeleteCycles to ensure safe deletion of resources.");

static constexpr uint32 MaxFramesBeforeDiscard = 10; // number of frames before ViewData is discarded if not written to

// must be greater than or equal to MinSafeDeleteCycles so that
// we can ensure no active views hold pointers to deleted objects.
static_assert(MaxFramesBeforeDiscard >= MinSafeDeleteCycles,
    "MaxFramesBeforeDiscard must be greater than or equal to MinSafeDeleteCycles");

// iterations per frame for cleaning up unused resources for passes
static constexpr int FrameCleanupBudget = 16;

static constexpr SizeType RenderPoolBlockSize = 16 * 1024 * 1024; // 16 MiB

// thread-local frame index for the game and render threads
// @NOTE: thread local so initialized to 0 on each thread by default
static thread_local uint32* s_threadFrameIndex;

static volatile int64 s_frameCounter; // atomic
static uint32 s_frameIndex[2] = { 0 };

// Render thread only
static RenderStats s_renderStats {};
static RenderStatsCalculator s_renderStatsCalculator {};

static std::counting_semaphore<NumMultiBuffers> s_fullSemaphore { 0 };
static std::counting_semaphore<NumMultiBuffers> s_freeSemaphore { NumMultiBuffers };

enum
{
    PRODUCER,
    CONSUMER
};

extern void CoreApi_UpdateGlobalConfig(const ConfigurationTable& mergeValues);

#pragma region Memory Pools

HYP_API Pool* g_renderPool;
HYP_API Pool* g_framePools[NumMultiBuffers];

#pragma endregion MemoryPools

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

extern void OnBindingChanged_Mesh(Mesh* mesh, uint32 prev, uint32 next);

// for setting texture only
extern void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next);

extern void OnBindingChanged_EnvProbe(EnvProbe* envProbe, uint32 prev, uint32 next);
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
        SparsePagedArray<uint32, 1024> bindingIndices;

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

    ResourceBindingAllocator<> meshBindingsAllocator;
    ResourceBinder<Mesh, &OnBindingChanged_Mesh> meshBinder { &meshBindingsAllocator };

    ResourceBindingAllocator<> cameraBindingsAllocator;
    ResourceBinder<Camera, &OnBindingChanged_Default<Camera>> cameraBinder { &cameraBindingsAllocator };

    // Shared index allocator for all envprobes

    ResourceBindingAllocator<> envProbeBindingsAllocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_EnvProbe> envProbeBinder { &envProbeBindingsAllocator };

    // reflection / sky probes need to allocate texture slots
    ResourceBindingAllocator<MaxBoundReflectionProbes> reflectionProbeTextureBindingsAllocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_ReflectionProbe> reflectionProbeTextureBinder { &reflectionProbeTextureBindingsAllocator };

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
        Threads::AssertOnThread(g_renderThread);
#endif

        AssertDebug(resource != nullptr);

        SubtypeResourceBindings& bindings = GetSubtypeBindings(resource->InstanceClass());

        ObjIdBase resourceId = resource->Id();
        AssertDebug(resourceId.IsValid());

        if (binding == ~0u)
        {
            bindings.bindingIndices.EraseAt(resourceId.ToIndex());

            return;
        }

        if (bindings.gpuBufferHolder != nullptr)
        {
            bindings.gpuBufferHolder->EnsureCapacity(binding);
        }

        bindings.bindingIndices.Emplace(resourceId.ToIndex(), binding);
    }

    uint32 Retrieve(const HypObjectBase* resource) const
    {
#ifdef HYP_DEBUG_MODE
        Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

        if (!resource)
        {
            return ~0u; // invalid resource
        }

        const SubtypeResourceBindings& bindings = const_cast<ResourceBindings*>(this)->GetSubtypeBindings(resource->InstanceClass());

        const ObjIdBase resourceId = resource->Id();

        const uint32* elem = bindings.bindingIndices.TryGet(resourceId.ToIndex());

        AssertDebug(elem != nullptr, "Failed to retrieve resource binding for resource with ID: {}", resourceId);

        return elem ? *elem : ~0u;
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
    static constexpr int MaxResourceBindersPerType = 3;

    TypeId typeId;

    // Map from id -> ResourceData
    SparsePagedArray<ResourceData, 256, RenderAllocator> data;

    Bitset indicesPendingDelete;
    Bitset indicesPendingUpdate;

    // reserve 1 extra for easier iteration without bounds checking
    ResourceBinderBase* resourceBinders[MaxResourceBindersPerType + 1];
    GpuBufferHolderBase* gpuBufferHolder;

    WriteBufferDataFunction writeBufferDataFn;

    // == optional render proxy data ==
    SparsePagedArray<IRenderProxy*, 1024, RenderAllocator> proxies;
    bool hasProxyData : 1;

    template <class ResourceType, class ProxyType, SizeType NumResourceBinders>
    ResourceSubtypeData(
        TypeWrapper<ResourceType>,
        TypeWrapper<ProxyType>,
        GpuBufferHolderBase* gpuBufferHolder = nullptr,
        FixedArray<ResourceBinderBase*, NumResourceBinders> resourceBinders = {},
        WriteBufferDataFunction writeBufferDataFn = nullptr)
        : typeId(TypeId::ForType<ResourceType>()),
          hasProxyData(false),
          gpuBufferHolder(gpuBufferHolder),
          resourceBinders { nullptr },
          writeBufferDataFn(writeBufferDataFn)
    {
        static_assert(NumResourceBinders <= MaxResourceBindersPerType,
            "Number of resource binders exceeds MaxResourceBindersPerType!");

        // copy resource binders
        for (SizeType i = 0; i < NumResourceBinders; i++)
        {
            this->resourceBinders[i] = resourceBinders[i];
        }

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

    SparsePagedArray<ResourceSubtypeData, 64, RenderAllocator> dataByType;
};

struct ResourceContainerFactoryRegistry
{
    Array<Proc<void(ResourceBindings&, ResourceContainer&)>> funcs;

    static ResourceContainerFactoryRegistry& GetInstance()
    {
        static ResourceContainerFactoryRegistry s_instance;
        return s_instance;
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
public:
    static const HypClass* GetResourceClass()
    {
        static const HypClass* s_resourceClass = GetClass<ResourceType>();
        return s_resourceClass;
    }

    template <class... ResourceBinderTypes>
    ResourceContainerFactory(
        GlobalRenderBuffer buf,
        WriteBufferDataFunction writeBufferDataFn,
        ResourceBinderTypes ResourceBindings::*... resourceBinderMembers)
    {
        ResourceContainerFactoryRegistry::GetInstance().funcs.PushBack(
            [=](ResourceBindings& resourceBindings, ResourceContainer& container)
            {
                const HypClass* resourceClass = GetResourceClass();
                AssertDebug(resourceClass != nullptr, "Class not found for resource type: {}", TypeName<ResourceType>().Data());

                const int staticIndex = resourceClass->GetStaticIndex();
                AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *resourceClass->GetName());

                GpuBufferHolderBase* gpuBufferHolder = buf < GRB_MAX ? g_renderGlobalState->gpuBuffers[buf] : nullptr;

                if (!resourceBindings.subtypeBindings.HasIndex(staticIndex))
                {
                    // add new ResourceSubtypeBindings slot for the given class
                    resourceBindings.subtypeBindings.Emplace(staticIndex, resourceClass, gpuBufferHolder);
                }

                AssertDebug(!container.dataByType.HasIndex(staticIndex),
                    "ResourceSubtypeData for resource class '{}' has already been registered!",
                    *resourceClass->GetName());

                container.dataByType.Emplace(
                    staticIndex,
                    TypeWrapper<ResourceType>(),
                    TypeWrapper<ProxyType>(),
                    gpuBufferHolder,
                    FixedArray<ResourceBinderBase*, sizeof...(ResourceBinderTypes)> {
                        static_cast<ResourceBinderBase*>(resourceBinderMembers != nullptr ? &(resourceBindings.*(resourceBinderMembers)) : nullptr)... },
                    writeBufferDataFn);

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

static FrameData g_frameData[NumMultiBuffers];
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
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    s_threadFrameIndex = &s_frameIndex[CONSUMER];

    Assert(g_appContext != nullptr, "AppContext must be initialized before RenderApi_Init!");
    Assert(g_renderBackend != nullptr);

    g_renderPool = new Pool(RenderPoolBlockSize);

    for (uint32 i = 0; i < NumMultiBuffers; i++)
    {
        g_framePools[i] = new Pool();
    }

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

            CoreApi_UpdateGlobalConfig(renderGlobalConfigOverrides);
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
    HYP_SCOPE;
    Threads::AssertOnThread(g_mainThread);

    for (uint32 i = 0; i < NumMultiBuffers; i++)
    {
        for (auto& it : g_frameData[i].viewFrameData)
        {
            delete it.second;
        }

        g_frameData[i].viewFrameData.Clear();

        delete g_framePools[i];
        g_framePools[i] = nullptr;
    }

    for (auto& it : g_viewData)
    {
        delete it.second;
    }

    g_viewData.Clear();

    delete g_renderGlobalState;
    g_renderGlobalState = nullptr;

    delete g_renderPool;
    g_renderPool = nullptr;

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
    if (!s_threadFrameIndex)
    {
        const int threadType = RenderApi_CurrentThreadType();
        Assert(threadType >= 0, "RenderApi_GetFrameIndex called from an invalid thread!");

        s_threadFrameIndex = &s_frameIndex[threadType];
    }

    return *s_threadFrameIndex;
}

uint32 RenderApi_GetFrameCounter()
{
    return (uint32)AtomicAdd(&s_frameCounter, 0);
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
    ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>* pDst,
    ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>* pSrc)
{
    AssertDebug(pDst != nullptr && pSrc != nullptr);

    auto& dst = *pDst;
    auto& src = *pSrc;

    dst.Advance();

    SyncResourcesImpl(dst, src.GetSubclassImpl(-1));

    for (Bitset::BitIndex subclassIndex : src.GetSubclassIndices())
    {
        SyncResourcesImpl(dst, src.GetSubclassImpl(int(subclassIndex)));
    }

    const ResourceTrackerDiff& diff = dst.GetDiff();

    if (!diff.NeedsUpdate())
    {
        return;
    }

    Array<ElementType*> removed;
    dst.GetRemoved(removed, false);

    Array<ElementType*> added;
    dst.GetAdded(added, false);

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
            ProxyType* pSrcProxy = src.GetProxy(resourceId);
            AssertDebug(pSrcProxy != nullptr);

            if (!pSrcProxy)
            {
                continue;
            }

            ProxyType* pDstProxy = dst.SetProxy(resourceId, *pSrcProxy);
            AssertDebug(pDstProxy != nullptr);

            CopyRenderProxy(subtypeData, resourceId, pDstProxy);
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
        dst.GetChanged(changed);

        if (changed.Any())
        {
            for (ElementType* pResource : changed)
            {
                ObjId<ElementType> resourceId = pResource->Id();

                ProxyType* pSrcProxy = src.GetProxy(resourceId);
                AssertDebug(pSrcProxy != nullptr);

                if (!pSrcProxy)
                {
                    continue;
                }

                ProxyType* pDstProxy = dst.SetProxy(resourceId, *pSrcProxy);
                AssertDebug(pDstProxy != nullptr);

                ResourceSubtypeData& subtypeData = g_resources.GetSubtypeData(pResource->InstanceClass());

                CopyRenderProxy(subtypeData, resourceId, pDstProxy);
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

    // Copy rpl -> vd.rplRender
    SyncResourcesT(vd.rplRender.resourceTrackers.Data(), rpl.resourceTrackers.Data(), std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());

    if (rpl.useOrdering)
    {
        vd.rplRender.meshEntityOrdering = rpl.meshEntityOrdering;
    }
}

RenderProxyList& RenderApi_GetProducerProxyList(View* view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    ViewFrameData* vd = GetViewFrameData(view, s_frameIndex[PRODUCER]);

    return *vd->rplShared;
}

RenderProxyList& RenderApi_GetConsumerProxyList(View* view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertDebug(view != nullptr);

    return GetViewData(view)->rplRender;
}

RenderCollector& RenderApi_GetRenderCollector(View* view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    return GetViewData(view)->renderCollector;
}

Array<Pair<View*, RenderCollector*>> RenderApi_GetAllRenderCollectors()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    Array<Pair<View*, RenderCollector*>> result;

    for (auto& it : g_viewData)
    {
        result.PushBack(Pair<View*, RenderCollector*>(it.first, &it.second->renderCollector));
    }

    return result;
}

IRenderProxy* RenderApi_GetRenderProxy(const HypObjectBase* resource)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

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
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    const ObjIdBase resourceId = resource->Id();

    ResourceSubtypeData& subtypeData = g_resources.GetSubtypeData(resource->InstanceClass());
    AssertDebug(resourceId.GetTypeId() == subtypeData.typeId);

    AssertDebug(subtypeData.gpuBufferHolder != nullptr,
        "Cannot update GPU data for type which does not have a GpuBufferHolder! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    AssertDebug(subtypeData.hasProxyData,
        "Cannot use UpdateGpuData() for type which does not have a RenderProxy! TypeId: {}, HypClass {}",
        subtypeData.typeId.Value(), *GetClass(subtypeData.typeId)->GetName());

    const uint32 bindingIndex = g_renderGlobalState->resourceBindings->Retrieve(resource);
    AssertDebug(bindingIndex != ~0u);

    const uint32 idx = resourceId.ToIndex();

    IRenderProxy* pProxy = subtypeData.proxies.Get(idx);
    AssertDebug(pProxy != nullptr);

    subtypeData.SetGpuElem(bindingIndex, pProxy);

    // set it as no longer needing update next frame since we updated immediately
    subtypeData.indicesPendingUpdate.Set(idx, false);
}

void RenderApi_AssignResourceBinding(HypObjectBase* resource, uint32 binding)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    g_renderGlobalState->resourceBindings->Assign(resource, binding);
}

uint32 RenderApi_RetrieveResourceBinding(const HypObjectBase* resource)
{
    HYP_SCOPE;
    // FIXME: Add better check to ensure it is from a render task thread.
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    return g_renderGlobalState->resourceBindings->Retrieve(resource);
}

WorldShaderData* RenderApi_GetWorldBufferData()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    return &g_frameData[*s_threadFrameIndex].worldBufferData;
}

Viewport& RenderApi_GetViewport(View* view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | g_renderThread);

    return GetViewFrameData(view, *s_threadFrameIndex)->viewport;
}

RenderStats* RenderApi_GetRenderStats()
{
    if (Threads::IsOnThread(g_renderThread))
    {
        return &s_renderStats;
    }

    Threads::AssertOnThread(g_gameThread);

    return &g_frameData[*s_threadFrameIndex].renderStats;
}

void RenderApi_AddRenderStats(const RenderStatsCounts& counts)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    s_renderStatsCalculator.AddCounts(counts);
}

void RenderApi_SuppressRenderStats()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    s_renderStatsCalculator.Suppress();
}

void RenderApi_UnsuppressRenderStats()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    s_renderStatsCalculator.Unsuppress();
}

void RenderApi_BeginFrame_GameThread()
{
    HYP_SCOPE;

    s_threadFrameIndex = &s_frameIndex[PRODUCER];

    s_freeSemaphore.acquire();
}

void RenderApi_EndFrame_GameThread()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    FrameData& frameData = g_frameData[s_frameIndex[PRODUCER]];

    s_frameIndex[PRODUCER] = (s_frameIndex[PRODUCER] + 1) % NumMultiBuffers;

    s_fullSemaphore.release();
}

void RenderApi_BeginFrame_RenderThread()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    s_fullSemaphore.acquire();

    const uint32 slot = s_frameIndex[CONSUMER];

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
        for (ResourceData& elem : subtypeData.data)
        {
            AssertDebug(elem.resource != nullptr);

            ResourceBinderBase** ppResourceBinder = &subtypeData.resourceBinders[0];

            while (*ppResourceBinder != nullptr)
            {
                (*ppResourceBinder)->Consider(elem.resource);
                ++ppResourceBinder;
            }
        }
    }

    // assign the actual bindings:
    /// TODO: This should be done in the ResourceBinder itself, not here.
    g_renderGlobalState->resourceBindings->meshEntityBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->meshBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->cameraBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->envProbeBinder.ApplyUpdates();
    g_renderGlobalState->resourceBindings->reflectionProbeTextureBinder.ApplyUpdates();
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
            Bitset currentBoundIndices;

            ResourceBinderBase** ppResourceBinder = &subtypeData.resourceBinders[0];
            while (*ppResourceBinder != nullptr)
            {
                currentBoundIndices |= (*ppResourceBinder)->GetBoundIndices(subtypeData.typeId);

                ++ppResourceBinder;
            }

            if (currentBoundIndices.Count() == 0)
            {
                // nothing is bound for this type, skip
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

                const uint32 bindingIndex = g_renderGlobalState->resourceBindings->Retrieve(resource);
                AssertDebug(bindingIndex != ~0u,
                    "Failed to retrieve binding for resource: {} in frame {}, but it is marked as bound (index: {})",
                    i, slot, i);

                IRenderProxy* pProxy = subtypeData.proxies.Get(i);
                AssertDebug(pProxy != nullptr);

                subtypeData.SetGpuElem(bindingIndex, pProxy);

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

    const uint32 slot = s_frameIndex[CONSUMER];

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
        if (++vd.framesSinceUsed == MaxFramesBeforeDiscard)
        {
            HYP_LOG(Rendering, Debug, "Discarding ViewData for view {} after {} frames",
                view->Id(), MaxFramesBeforeDiscard);

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

    int numCleanupCycles = FrameCleanupBudget;
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

            ResourceBinderBase** ppResourceBinder = &subtypeData.resourceBinders[0];

            while (*ppResourceBinder != nullptr)
            {
                (*ppResourceBinder)->Deconsider(rd.resource);
                ++ppResourceBinder;
            }

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
    s_renderStatsCalculator.Advance(s_renderStats);
    frameData.renderStats = s_renderStats;

    g_safeDeleter->Iterate();

    // deallocate all frame allocations for this frame
    g_framePools[slot]->Reset();

    s_frameIndex[CONSUMER] = (s_frameIndex[CONSUMER] + 1) % NumMultiBuffers;

    AtomicIncrement(&s_frameCounter);

    s_freeSemaphore.release();
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

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
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

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        SetDefaultDescriptorSetElements(frameIndex);
    }

    CreateSphereSamplesBuffer();
    CreateBlueNoiseBuffer();

    globalDescriptorTable->Create();

    mainRenderer = new DeferredRenderer();
    mainRenderer->Initialize();

    for (uint32 i = 0; i < GRT_MAX; i++)
    {
        globalRenderers[i] = Array<RendererBase*>();
    }

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

    const uint32 frameIndex = frame->GetFrameIndex();

    for (auto& it : gpuBufferHolders->GetItems())
    {
        it.second->UpdateBufferSize(frameIndex);
        it.second->UpdateBufferData(frame);
    }

    StagingBufferPool::GetInstance().Cleanup(frameIndex);
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
    blueNoiseBuffer->SetDebugName(NAME("BlueNoiseBuffer"));
    blueNoiseBuffer->SetRequireCpuAccessible(true);
    HYP_GFX_ASSERT(blueNoiseBuffer->Create());
    blueNoiseBuffer->Copy(sobol256spp256dOffset, sobol256spp256dSize, &BlueNoise::sobol256spp256d[0]);
    blueNoiseBuffer->Copy(scramblingTileOffset, scramblingTileSize, &BlueNoise::scramblingTile[0]);
    blueNoiseBuffer->Copy(rankingTileOffset, rankingTileSize, &BlueNoise::rankingTile[0]);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
            ->SetElement("BlueNoiseBuffer", blueNoiseBuffer);
    }
}

void RenderGlobalState::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;

    GpuBufferRef sphereSamplesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(Vec4f) * 4096);
    sphereSamplesBuffer->SetDebugName(NAME("SphereSamplesBuffer"));
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

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
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

    for (uint32 i = 0; i < MaxBoundReflectionProbes; i++)
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
        for (uint32 textureIndex = 0; textureIndex < MaxBindlessResources; textureIndex++)
        {
            globalDescriptorTable->GetDescriptorSet("Material", frameIndex)
                ->SetElement("Textures", textureIndex,
                    g_renderBackend->GetTextureImageView(placeholderData->defaultTexture2d));
        }
    }
    else
    {
        for (uint32 textureIndex = 0; textureIndex < MaxBoundTextures; textureIndex++)
        {
            globalDescriptorTable->GetDescriptorSet("Material", frameIndex)
                ->SetElement("Textures", textureIndex,
                    g_renderBackend->GetTextureImageView(placeholderData->defaultTexture2d));
        }
    }
}

#pragma endregion RenderGlobalState

DECLARE_RENDER_DATA_CONTAINER(Entity, RenderProxyMesh, GRB_ENTITIES, &WriteBufferData_MeshEntity, &ResourceBindings::meshEntityBinder);

DECLARE_RENDER_DATA_CONTAINER(Mesh, NullProxy, GRB_INVALID, nullptr, &ResourceBindings::meshBinder);

DECLARE_RENDER_DATA_CONTAINER(Camera, RenderProxyCamera, GRB_CAMERAS, nullptr, &ResourceBindings::cameraBinder);

DECLARE_RENDER_DATA_CONTAINER(EnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &WriteBufferData_EnvGrid, &ResourceBindings::envGridBinder);
DECLARE_RENDER_DATA_CONTAINER(LegacyEnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &WriteBufferData_EnvGrid, &ResourceBindings::envGridBinder);

// FIXME: Overlap with ambient probes / reflection and sky probes causing issues where indices are overlapping,
// due to using separate allocators but going into the same buffer. Need to either use a single allocator for all env probes,
// or have separate buffers for each type.
DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &WriteBufferData_EnvProbe, &ResourceBindings::envProbeBinder);
DECLARE_RENDER_DATA_CONTAINER(ReflectionProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &WriteBufferData_EnvProbe, &ResourceBindings::envProbeBinder, &ResourceBindings::reflectionProbeTextureBinder);
DECLARE_RENDER_DATA_CONTAINER(SkyProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &WriteBufferData_EnvProbe, &ResourceBindings::envProbeBinder, &ResourceBindings::reflectionProbeTextureBinder);

DECLARE_RENDER_DATA_CONTAINER(Light, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &ResourceBindings::lightBinder);
DECLARE_RENDER_DATA_CONTAINER(DirectionalLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &ResourceBindings::lightBinder);
DECLARE_RENDER_DATA_CONTAINER(PointLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &ResourceBindings::lightBinder);
DECLARE_RENDER_DATA_CONTAINER(AreaRectLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &ResourceBindings::lightBinder);
DECLARE_RENDER_DATA_CONTAINER(SpotLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &ResourceBindings::lightBinder);

DECLARE_RENDER_DATA_CONTAINER(LightmapVolume, RenderProxyLightmapVolume, GRB_LIGHTMAP_VOLUMES, nullptr, &ResourceBindings::lightmapVolumeBinder);

DECLARE_RENDER_DATA_CONTAINER(Material, RenderProxyMaterial, GRB_MATERIALS, nullptr, &ResourceBindings::materialBinder);

DECLARE_RENDER_DATA_CONTAINER(Texture, NullProxy, GRB_INVALID, nullptr, &ResourceBindings::textureBinder);

DECLARE_RENDER_DATA_CONTAINER(Skeleton, RenderProxySkeleton, GRB_SKELETONS, nullptr, &ResourceBindings::skeletonBinder);

} // namespace hyperion

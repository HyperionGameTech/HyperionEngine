/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/GpuBufferHolderMap.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderSwapchain.hpp>
#include <rendering/FinalPass.hpp>

#include <rendering/util/ResourceTracker.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/renderers/EnvGridRenderer.hpp>

#include <rendering/renderers/ShadowRenderer.hpp>

#include <rendering/renderers/ParticleVolumeRenderer.hpp>

#include <rendering/raytracing/DDGI.hpp>

#include <shadows/ShadowMapAllocator.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Light.hpp>

#include <scene/animation/Skeleton.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <particles/ParticleVolume.hpp>

#include <core/reflection/Class.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/threading/Semaphore.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

// for EnumToString
#include <core/reflection/Enum.hpp>
#include <core/reflection/Class.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/BlueNoise.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineStats.hpp>

#include <system/AppContext.hpp>

#include <HyperionEngine.hpp>

#include <rendering/util/ResourceBinder.hpp>

#include <semaphore>

#include <RenderGlobalState.generated.inl>

namespace hyperion {

static_assert(RingBufferDepth <= MinSafeDeleteCycles,
    "RingBufferDepth must be less than or equal to MinSafeDeleteCycles to ensure safe deletion of resources.");

static constexpr uint32 MaxFramesBeforeDiscard = 10; // number of frames before ViewData is discarded if not written to

// must be greater than or equal to MinSafeDeleteCycles so that
// we can ensure no active views hold pointers to deleted objects.
static_assert(MaxFramesBeforeDiscard >= MinSafeDeleteCycles,
    "MaxFramesBeforeDiscard must be greater than or equal to MinSafeDeleteCycles");

// iterations per frame for cleaning up unused resources for passes
static constexpr int FrameCleanupBudget = 16;

// size of render thread arena in bytes (reset every frame)
static constexpr SizeType RenderArenaSize = 1 * 1024 * 1024;

// thread-local frame index for the game and render threads
// @NOTE: thread local so initialized to 0 on each thread by default
static thread_local uint32* s_threadFrameIndex;

static volatile int64 s_frameCounter; // atomic
static uint32 s_frameIndex[2] = { 0 };

static std::counting_semaphore<RingBufferDepth> s_fullSemaphore { 0 };
static std::counting_semaphore<RingBufferDepth> s_freeSemaphore { RingBufferDepth };

EngineStatTimer g_renderCpuSyncTimer("Render/Sync");

enum
{
    PRODUCER,
    CONSUMER
};

extern void CoreApi_UpdateGlobalConfig(const ConfigurationTable& mergeValues);

#pragma region ResourceBindings

/// TODO: refactor to use mappings instead of idx (void* directly to element on cpu)
typedef void (*WriteBufferDataFunction)(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

template <class T>
static void OnBindingChanged_Default(T* resource, uint32 prev, uint32 next)
{
#ifdef HYP_DEBUG_MODE
    AssertOnThread(g_renderThread);
#endif

    AssertDebug(resource != nullptr);

    RenderApi::AssignResourceBinding(resource, next);
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

namespace RenderApi {

static ResourceBindingAllocator<> s_meshEntityBindingsAllocator;
static ResourceBinder<Entity, &OnBindingChanged_MeshEntity> s_meshEntityBinder { &s_meshEntityBindingsAllocator };
ResourceBinderBase* g_meshEntityBinder = &s_meshEntityBinder;

static ResourceBindingAllocator<> s_meshBindingsAllocator;
static ResourceBinder<Mesh, &OnBindingChanged_Mesh> s_meshBinder { &s_meshBindingsAllocator };
ResourceBinderBase* g_meshBinder = &s_meshBinder;

static ResourceBindingAllocator<> s_cameraBindingsAllocator;
static ResourceBinder<Camera, &OnBindingChanged_Default<Camera>> s_cameraBinder { &s_cameraBindingsAllocator };
ResourceBinderBase* g_cameraBinder = &s_cameraBinder;

// Shared index allocator for all envprobes

static ResourceBindingAllocator<> s_envProbeBindingsAllocator;
static ResourceBinder<EnvProbe, &OnBindingChanged_EnvProbe> s_envProbeBinder { &s_envProbeBindingsAllocator };
ResourceBinderBase* g_envProbeBinder = &s_envProbeBinder;

// reflection / sky probes need to allocate texture slots
static ResourceBindingAllocator<MaxBoundReflectionProbes> s_reflectionProbeTextureBindingsAllocator;
static ResourceBinder<EnvProbe, &OnBindingChanged_ReflectionProbe> s_reflectionProbeTextureBinder { &s_reflectionProbeTextureBindingsAllocator };
ResourceBinderBase* g_reflectionProbeTextureBinder = &s_reflectionProbeTextureBinder;

static ResourceBindingAllocator<MaxBoundEnvGrids> s_envGridBindingsAllocator;
static ResourceBinder<EnvGrid, &OnBindingChanged_EnvGrid> s_envGridBinder { &s_envGridBindingsAllocator };
ResourceBinderBase* g_envGridBinder = &s_envGridBinder;

static ResourceBindingAllocator<> s_lightBindingsAllocator;
static ResourceBinder<Light, &OnBindingChanged_Light> s_lightBinder { &s_lightBindingsAllocator };
ResourceBinderBase* g_lightBinder = &s_lightBinder;

static ResourceBindingAllocator<MaxBoundLightmapVolumes> s_lightmapVolumeBindingsAllocator;
static ResourceBinder<LightmapVolume, &OnBindingChanged_Default<LightmapVolume>> s_lightmapVolumeBinder {
    &s_lightmapVolumeBindingsAllocator
};
ResourceBinderBase* g_lightmapVolumeBinder = &s_lightmapVolumeBinder;

static ResourceBindingAllocator<> s_particleVolumeBindingsAllocator;
static ResourceBinder<ParticleVolume, &OnBindingChanged_Default<ParticleVolume>> s_particleVolumeBinder {
    &s_particleVolumeBindingsAllocator
};
ResourceBinderBase* g_particleVolumeBinder = &s_particleVolumeBinder;

static ResourceBindingAllocator<> s_materialBindingsAllocator;
static ResourceBinder<Material, &OnBindingChanged_Material> s_materialBinder { &s_materialBindingsAllocator };
ResourceBinderBase* g_materialBinder = &s_materialBinder;

static ResourceBindingAllocator<> s_textureBindingsAllocator;
static ResourceBinder<Texture, &OnBindingChanged_Texture> s_textureBinder { &s_textureBindingsAllocator };
ResourceBinderBase* g_textureBinder = &s_textureBinder;

static ResourceBindingAllocator<> s_skeletonBindingsAllocator;
static ResourceBinder<Skeleton, &OnBindingChanged_Default<Skeleton>> s_skeletonBinder { &s_skeletonBindingsAllocator };
ResourceBinderBase* g_skeletonBinder = &s_skeletonBinder;

static ResourceBinderBase* s_resourceBinders[] = {
    &s_meshEntityBinder,
    &s_meshBinder,
    &s_cameraBinder,
    &s_envProbeBinder,
    &s_reflectionProbeTextureBinder,
    &s_envGridBinder,
    &s_lightBinder,
    &s_lightmapVolumeBinder,
    &s_particleVolumeBinder,
    &s_materialBinder,
    &s_textureBinder,
    &s_skeletonBinder
};

struct SubtypeResourceBindings
{
    const Class* resourceClass;
    GpuBufferHolderBase* gpuBufferHolder;
    SparsePagedArray<uint32, 1024, RenderAllocator> bindingIndices;

    SubtypeResourceBindings(const Class* resourceClass, GpuBufferHolderBase* gpuBufferHolder)
        : resourceClass(resourceClass),
          gpuBufferHolder(gpuBufferHolder)
    {
        AssertDebug(resourceClass != nullptr);
    }
};

static SparsePagedArray<SubtypeResourceBindings, 64> s_subtypeBindings;

static inline SubtypeResourceBindings& ResourceBinding_GetSubtypeBindings(const Class* cls)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertDebug(cls != nullptr);

    int staticIndex = cls->GetStaticIndex();
    AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *cls->GetName());

    SubtypeResourceBindings* bindings = s_subtypeBindings.TryGet(staticIndex);
    AssertDebug(bindings != nullptr, "No SubtypeBindings container found for {}", cls->GetName());

    return *bindings;
}

static void ResourceBinding_Assign(ObjectBase* resource, uint32 binding)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    SubtypeResourceBindings& bindings = ResourceBinding_GetSubtypeBindings(resource->InstanceClass());

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

static uint32 ResourceBinding_Retrieve(const ObjectBase* resource)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!resource)
    {
        return ~0u; // invalid resource
    }

    const SubtypeResourceBindings& bindings = ResourceBinding_GetSubtypeBindings(resource->InstanceClass());

    const ObjIdBase resourceId = resource->Id();

    const uint32* elem = bindings.bindingIndices.TryGet(resourceId.ToIndex());

    AssertDebug(elem != nullptr, "Failed to retrieve resource binding for resource with ID: {}", resourceId);

    return elem ? *elem : ~0u;
}

#pragma endregion ResourceBindings

#pragma region ResourceContainer

struct ResourceData final
{
    ObjectBase* resource;
    uint32 useCount;

    ResourceData(ObjectBase* resource)
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

    const TypeInfo* typeInfo;

    // Map from id -> ResourceData
    SparsePagedArray<ResourceData, 1024, RenderAllocator> data;

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
        : typeInfo(&TypeInfo::ForType<ResourceType>()),
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
    ResourceSubtypeData& GetSubtypeData(const Class* cls)
    {
        AssertDebug(cls != nullptr);

        int staticIndex = cls->GetStaticIndex();
        AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *cls->GetName());

        AssertDebug(dataByType.HasIndex(staticIndex), "Missing resource data for {}", *cls->GetName());

        return dataByType.Get(staticIndex);
    }

    SparsePagedArray<ResourceSubtypeData, 64, RenderAllocator> dataByType;
};

struct ResourceContainerFactoryRegistry
{
    Array<Proc<void(ResourceContainer&)>> funcs;

    static ResourceContainerFactoryRegistry& GetInstance()
    {
        static ResourceContainerFactoryRegistry s_instance;
        return s_instance;
    }

    void InvokeAll(ResourceContainer& resourceContainer)
    {
        for (auto& func : funcs)
        {
            func(resourceContainer);
        }
    }
};

template <class ResourceType, class ProxyType>
struct ResourceContainerFactory
{
public:
    static const Class* GetResourceClass()
    {
        return ResourceType::StaticClass();
    }

    template <class... ResourceBinderTypes>
    ResourceContainerFactory(
        GlobalRenderBuffer buf,
        WriteBufferDataFunction writeBufferDataFn,
        ResourceBinderTypes*... resourceBinders)
    {
        ResourceContainerFactoryRegistry::GetInstance().funcs.PushBack(
            [=](ResourceContainer& container)
            {
                const Class* resourceClass = GetResourceClass();
                AssertDebug(resourceClass != nullptr);

                const int staticIndex = resourceClass->GetStaticIndex();
                AssertDebug(staticIndex >= 0, "Invalid class: '{}' has no assigned static index!", *resourceClass->GetName());

                GpuBufferHolderBase* gpuBufferHolder = buf < GRB_MAX ? g_renderGlobalState->gpuBuffers[buf] : nullptr;

                if (!s_subtypeBindings.HasIndex(staticIndex))
                {
                    // add new ResourceSubtypeBindings slot for the given class
                    s_subtypeBindings.Emplace(staticIndex, resourceClass, gpuBufferHolder);
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
                        static_cast<ResourceBinderBase*>(resourceBinders)... },
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
    RenderProxyList rplRender { g_renderPool, /* isShared */ false, /* useRefCounting */ false };
    RenderCollector renderCollector;
    uint32 framesSinceUsed = 0;
    uint32 numRefs = 0; // number of ViewFrameData holding refs to this

    void AddRef()
    {
        AssertDebug(view != nullptr);

        ++numRefs;
        view->GetObjectHeader_Internal()->IncRefStrong();
    }

    uint32 ReleaseRef()
    {
        AssertDebug(view != nullptr && numRefs > 0);

        if (--numRefs == 0)
        {
            Handle<View> viewHandle;
            viewHandle.ptr = view;
            SafeDelete(std::move(viewHandle));

            view = nullptr;
        }
        else
        {
            view->GetObjectHeader_Internal()->DecRefStrong();
        }

        return numRefs;
    }
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
};

static FrameData s_frameData[RingBufferDepth];
static HashMap<View*, ViewData*> s_viewData;
static ResourceContainer* s_resources;

static ViewData* GetViewData(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(view != nullptr);

    auto viewDataIt = s_viewData.Find(view);
    if (viewDataIt == s_viewData.End())
    {
        HYP_LOG(Rendering, Debug, "Allocating new ViewData for View {}", view->Id());

        ViewData* vd = PoolNew<ViewData>(*g_renderPool);
        vd->view = view;

        if (view->GetViewDesc().entityBatchClass != nullptr)
        {
            vd->renderCollector.batchAllocator = GetOrCreateEntityBatchAllocator(view->GetViewDesc().entityBatchClass->GetTypeId());
        }
        else
        {
            vd->renderCollector.batchAllocator = GetOrCreateEntityBatchAllocator<EntityInstanceBatch>();
        }

        AssertDebug(vd->renderCollector.batchAllocator != nullptr);

        viewDataIt = s_viewData.Insert(view, vd).first;
    }

    AssertDebug(viewDataIt->second != nullptr);

    viewDataIt->second->framesSinceUsed = 0;

    return viewDataIt->second;
}

static ViewFrameData* GetViewFrameData(View* view, uint32 slot)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    ViewFrameData*& vfd = s_frameData[slot].viewFrameData[view];

    if (!vfd)
    {
        vfd = PoolNew<ViewFrameData>(*g_framePools[slot]);
        vfd->view = view;

        vfd->rplShared = view->GetRenderProxyList(slot);
        AssertDebug(vfd->rplShared != nullptr);
        AssertDebug(vfd->rplShared->isShared, "Expected isShared to be true to ensure multiple threads don't access the list concurrently");
    }

    return vfd;
}

template <class ElementType, class ProxyType>
static HYP_FORCE_INLINE void CopyRenderProxy(ResourceSubtypeData& subtypeData, const ObjId<ElementType>& id, ProxyType* pNewProxy)
{
    AssertDebug(pNewProxy != nullptr);

    const uint32 idx = id.ToIndex();

    AssertDebug(subtypeData.typeInfo->id == id.GetTypeId(),
        "Attempting to use ID for type {} as index into proxy collection that requires index type {}",
        LookupTypeName(id.GetTypeId()),
        subtypeData.typeInfo->name);

    subtypeData.proxies.Set(idx, static_cast<IRenderProxy*>(pNewProxy));
    subtypeData.indicesPendingUpdate.Set(idx, true);
}

template <class AllocatorType, class ElementType, class ProxyType>
static HYP_FORCE_INLINE void SyncResourcesImpl(
    ResourceTracker<AllocatorType, ObjId<ElementType>, ElementType*, ProxyType>& resourceTracker,
    const typename ResourceTracker<AllocatorType, ObjId<ElementType>, ElementType*, ProxyType>::Impl& impl)
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

template <class AllocatorType, class ElementType, class ProxyType>
static void SyncResources(
    ResourceTracker<AllocatorType, ObjId<ElementType>, ElementType*, ProxyType>& dst,
    const ResourceTracker<AllocatorType, ObjId<ElementType>, ElementType*, ProxyType>& src)
{
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

        ResourceSubtypeData& subtypeData = s_resources->GetSubtypeData(pResource->InstanceClass());
        AssertDebug(resourceId.GetTypeId() == subtypeData.typeInfo->id);

        ResourceData* rd = subtypeData.data.TryGet(resourceId.ToIndex());

        if (!rd)
        {
            rd = &*subtypeData.data.Emplace(resourceId.ToIndex(), pResource);
        }

        subtypeData.indicesPendingDelete.Set(resourceId.ToIndex(), false);

        ++rd->useCount;

        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            const ProxyType* pSrcProxy = src.GetProxy(resourceId);
            AssertDebug(pSrcProxy != nullptr);

            ProxyType* pDstProxy = dst.SetProxy(resourceId, *pSrcProxy);
            CopyRenderProxy(subtypeData, resourceId, pDstProxy);
        }
    }

    for (ElementType* pResource : removed)
    {
        AssertDebug(pResource != nullptr);

        const ObjId<ElementType> resourceId = pResource->Id();
        AssertDebug(resourceId.IsValid());

        ResourceSubtypeData& subtypeData = s_resources->GetSubtypeData(pResource->InstanceClass());
        AssertDebug(resourceId.GetTypeId() == subtypeData.typeInfo->id);

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

                const ProxyType* pSrcProxy = src.GetProxy(resourceId);
                AssertDebug(pSrcProxy != nullptr);

                ResourceSubtypeData& subtypeData = s_resources->GetSubtypeData(pResource->InstanceClass());

                ProxyType* pDstProxy = dst.SetProxy(resourceId, *pSrcProxy);
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

template <class AllocatorType, SizeType... Indices>
static HYP_FORCE_INLINE void SyncResourcesT(
    ResourceTrackerBase<AllocatorType>** dstResourceTrackers,
    ResourceTrackerBase<AllocatorType>** srcResourceTrackers,
    std::index_sequence<Indices...>)
{
    (SyncResources(
         static_cast<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type&>(*dstResourceTrackers[Indices]),
         static_cast<const typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type&>(*srcResourceTrackers[Indices])),
        ...);
}

static HYP_FORCE_INLINE void CopyDependencies(RenderProxyList& dst, RenderProxyList& src)
{
    AssertDebug(dst.resourceTrackers.Size() == TupleSize<RenderProxyList::ResourceTrackerTypes>::value);
    AssertDebug(src.resourceTrackers.Size() == TupleSize<RenderProxyList::ResourceTrackerTypes>::value);

    // Copy src -> dst
    SyncResourcesT(dst.resourceTrackers.Data(), src.resourceTrackers.Data(), std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());

    if (src.useOrdering)
    {
        dst.meshEntityOrdering = src.meshEntityOrdering;
    }
}

void Init()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(g_renderArena == nullptr);
    g_renderArena = new TArena<RenderAllocator>(RenderArenaSize);

    s_resources = PoolNew<ResourceContainer>(*g_renderPool);

    s_threadFrameIndex = &s_frameIndex[CONSUMER];

    Assert(g_appContext != nullptr, "AppContext must be initialized before Init!");
    Assert(g_renderBackend != nullptr);

    for (ResourceBinderBase* resourceBinder : s_resourceBinders)
    {
        resourceBinder->Initialize();
    }

    RendererResult result = g_renderBackend->Initialize();
    Assert(result, "Failed to initialize rendering backend: {}", result.GetError().GetMessage());

    { // override global config after renderer initialize
        ConfigurationTable renderGlobalConfigOverrides;

        // if ray tracing is not supported, we need to update the configuration
        if (!g_renderBackend->GetRenderConfig().raytracing)
        {
            renderGlobalConfigOverrides.Set("Rendering.RayTracing.Enabled", false);
            renderGlobalConfigOverrides.Set("Rendering.RayTracing.Reflections.Enabled", false);
            renderGlobalConfigOverrides.Set("Rendering.RayTracing.GI.Enabled", false);
            renderGlobalConfigOverrides.Set("Rendering.RayTracing.PathTracing.Enabled", false);

            CoreApi_UpdateGlobalConfig(renderGlobalConfigOverrides);
        }
    }

    g_renderGlobalState = PoolNew<RenderGlobalState>(*g_renderPool);
    g_renderGlobalState->materialDescriptorSetManager->CreateFallbackMaterialDescriptorSet();

    g_renderGlobalState->finalPass = PoolNew<FinalPass>(*g_renderPool, MakeStrongRef(g_renderBackend->GetSwapchain()));
    g_renderGlobalState->finalPass->Create();

    ResourceContainerFactoryRegistry& registry = ResourceContainerFactoryRegistry::GetInstance();
    registry.InvokeAll(*s_resources);

    registry.funcs.Clear();
}

void Shutdown()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    for (uint32 i = 0; i < RingBufferDepth; i++)
    {
        for (auto& it : s_frameData[i].viewFrameData)
        {
            PoolDelete(*g_framePools[i], it.second);
        }

        s_frameData[i].viewFrameData.Clear();
    }

    for (auto& it : s_viewData)
    {
        ViewData* vd = it.second;

        if (!vd)
        {
            continue;
        }

        PoolDelete(*g_renderPool, vd);
    }

    s_viewData.Clear();

    PoolDelete(*g_renderPool, s_resources);
    s_resources = nullptr;

    PoolDelete(*g_renderPool, g_renderGlobalState);
    g_renderGlobalState = nullptr;

    Assert(g_renderBackend->Destroy());
}

static inline int CurrentThreadType()
{
    const ThreadId& threadId = CurrentThreadId();

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

uint32 GetRingIndex()
{
    if (HYP_UNLIKELY(!s_threadFrameIndex))
    {
        const int threadType = CurrentThreadType();
        Assert(threadType >= 0, "GetRingIndex called from an invalid thread!");

        s_threadFrameIndex = &s_frameIndex[threadType];
    }

    return *s_threadFrameIndex;
}

uint32 GetFrameCounter()
{
    return (uint32)AtomicAdd(&s_frameCounter, 0);
}

RenderProxyList& GetProducerProxyList(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    ViewFrameData* vd = GetViewFrameData(view, s_frameIndex[PRODUCER]);

    return *vd->rplShared;
}

RenderProxyList& GetConsumerProxyList(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(view != nullptr);

    return GetViewData(view)->rplRender;
}

RenderCollector& GetRenderCollector(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    return GetViewData(view)->renderCollector;
}

Array<Pair<View*, RenderCollector*>> GetAllRenderCollectors()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Array<Pair<View*, RenderCollector*>> result;

    for (auto& it : s_viewData)
    {
        result.PushBack(Pair<View*, RenderCollector*>(it.first, &it.second->renderCollector));
    }

    return result;
}

IRenderProxy* GetRenderProxy(const ObjectBase* resource)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    ResourceSubtypeData& subtypeData = s_resources->GetSubtypeData(resource->InstanceClass());
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

    IRenderProxy* pProxy = subtypeData.proxies.Get(resourceId.ToIndex());
    AssertDebug(pProxy != nullptr);

    return pProxy;
}

void UpdateGpuData(const ObjectBase* resource)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    const ObjIdBase resourceId = resource->Id();

    ResourceSubtypeData& subtypeData = s_resources->GetSubtypeData(resource->InstanceClass());
    AssertDebug(resourceId.GetTypeId() == subtypeData.typeInfo->id);

    AssertDebug(subtypeData.gpuBufferHolder != nullptr,
        "Cannot update GPU data for type which does not have a GpuBufferHolder! Type: {}",
        subtypeData.typeInfo->name);

    AssertDebug(subtypeData.hasProxyData,
        "Cannot use UpdateGpuData() for type which does not have a RenderProxy! Type: {}",
        subtypeData.typeInfo->name);

    const uint32 bindingIndex = ResourceBinding_Retrieve(resource);
    AssertDebug(bindingIndex != ~0u);

    const uint32 idx = resourceId.ToIndex();

    IRenderProxy* pProxy = subtypeData.proxies.Get(idx);
    AssertDebug(pProxy != nullptr);

    subtypeData.SetGpuElem(bindingIndex, pProxy);

    // set it as no longer needing update next frame since we updated immediately
    subtypeData.indicesPendingUpdate.Set(idx, false);
}

void AssignResourceBinding(ObjectBase* resource, uint32 binding)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    ResourceBinding_Assign(resource, binding);
}

uint32 RetrieveResourceBinding(const ObjectBase* resource)
{
    HYP_SCOPE;
    // FIXME: Add better check to ensure it is from a render task thread.
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    return ResourceBinding_Retrieve(resource);
}

WorldShaderData* GetWorldBufferData()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread | g_renderThread);

    return &s_frameData[*s_threadFrameIndex].worldBufferData;
}

Viewport& GetViewport(View* view)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread | g_renderThread);

    return GetViewFrameData(view, *s_threadFrameIndex)->viewport;
}

void BeginFrame_GameThread()
{
    HYP_SCOPE;

    s_threadFrameIndex = &s_frameIndex[PRODUCER];

    s_freeSemaphore.acquire();
}

void EndFrame_GameThread()
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);

    const uint32 slot = s_frameIndex[PRODUCER];
    FrameData& frameData = s_frameData[slot];

    g_sceneArena->Reset();

    s_frameIndex[PRODUCER] = (s_frameIndex[PRODUCER] + 1) % RingBufferDepth;

    s_fullSemaphore.release();
}

void BeginFrame_RenderThread()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    {
        ENGINE_STAT_SCOPE(&g_renderCpuSyncTimer);

        s_fullSemaphore.acquire();
    }

    const uint32 slot = s_frameIndex[CONSUMER];
    FrameData& fd = s_frameData[slot];

    g_engineStatsRecorder->Prepare();

    HYP_GFX_ASSERT(RenderCommands::Flush());

    for (auto it = fd.viewFrameData.Begin(); it != fd.viewFrameData.End(); ++it)
    {
        ViewFrameData& vfd = *it->second;
        AssertDebug(vfd.rplShared != nullptr);

        if (!vfd.viewData)
        {
            vfd.viewData = GetViewData(vfd.view);
            vfd.viewData->AddRef();
        }

        bool readLockAcquired = false;
        vfd.rplShared->BeginRead(&readLockAcquired);

        if (!readLockAcquired)
        {
            HYP_LOG(Rendering, Warning, "Read lock for RenderProxyList could not be acquired, may result in invalid resource bindings or stale pointers!!!");

            continue;
        }

#ifdef HYP_DEBUG_MODE
        vfd.rplShared->debugIsSynced = true;
#endif

        AssertDebug(vfd.rplShared->debugIsDestroyed == false, "RenderProxyList for view {} has been destroyed!", vfd.view->Id());

        // copy dependencies from shared to ViewData
        CopyDependencies(vfd.viewData->rplRender, *vfd.rplShared);

        vfd.rplShared->EndRead();
    }

    {
        HYP_NAMED_SCOPE("Resource bindings - select candidates");

        for (ResourceSubtypeData& subtypeData : s_resources->dataByType)
        {
            for (ResourceData& elem : subtypeData.data)
            {
                AssertDebug(elem.resource != nullptr);

                bool forceRebind = false;
                IRenderProxy** ppProxy = nullptr;

                if (subtypeData.hasProxyData && subtypeData.indicesPendingUpdate.Test(elem.resource->Id().ToIndex()))
                {
                    ppProxy = subtypeData.proxies.TryGet(elem.resource->Id().ToIndex());
                    AssertDebug(ppProxy != nullptr);

                    forceRebind = (*ppProxy)->forceRebind;

                    if (forceRebind)
                    {
                        (*ppProxy)->forceRebind = false; // swap
                    }
                }

                ResourceBinderBase** ppResourceBinder = &subtypeData.resourceBinders[0];

                while (*ppResourceBinder != nullptr)
                {
                    ResourceBinderBase* pResourceBinder = *ppResourceBinder;
                    pResourceBinder->Consider(elem.resource, forceRebind);

                    ++ppResourceBinder;
                }
            }
        }
    }

    // assign the actual bindings:
    for (ResourceBinderBase* resourceBinder : s_resourceBinders)
    {
        resourceBinder->ApplyUpdates();
    }

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

    for (ResourceSubtypeData& subtypeData : s_resources->dataByType)
    {
        if (subtypeData.indicesPendingUpdate.Count() != 0)
        {
            Bitset currentBoundIndices;

            ResourceBinderBase** ppResourceBinder = &subtypeData.resourceBinders[0];
            while (*ppResourceBinder != nullptr)
            {
                currentBoundIndices |= (*ppResourceBinder)->GetBoundIndices(subtypeData.typeInfo->id);

                ++ppResourceBinder;
            }

            if (currentBoundIndices.Count() == 0)
            {
                // nothing is bound for this type, skip
                continue;
            }

            if (!subtypeData.gpuBufferHolder)
            {
                // in the loop below we only do anything if we have gpu data to update.
                // short circuit here and just clear the bits without doing anything if we don't have a gpu buffer holder set.
                subtypeData.indicesPendingUpdate.Clear();

                continue;
            }

            // Handle proxies that were updated on game thread
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

                const uint32 bindingIndex = ResourceBinding_Retrieve(resource);
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

void EndFrame_RenderThread()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 slot = s_frameIndex[CONSUMER];

    FrameData& frameData = s_frameData[slot];

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
        if (++vd.framesSinceUsed >= MaxFramesBeforeDiscard)
        {
            // Decrement ref count on the ViewData,
            // if we hit zero there are no more ViewFrameData holding refs to the ViewData so we delete it
            AssertDebug(vd.numRefs > 0);

            if (vd.ReleaseRef() == 0)
            {
                HYP_LOG(Rendering, Debug, "Discarding ViewData for view {}", view->Id());

                auto viewDataIt = s_viewData.Find(view);
                AssertDebug(viewDataIt != s_viewData.End() && viewDataIt->second == &vd);

                s_viewData.Erase(viewDataIt);

                PoolDelete(*g_renderPool, &vd);
            }

#ifdef HYP_DEBUG_MODE
            vfd.rplShared->debugIsSynced = false;
#endif

            PoolDelete(*g_framePools[slot], &vfd);

            it = frameData.viewFrameData.Erase(it);

            continue;
        }

        ++it;
    }

    int numCleanupCycles = FrameCleanupBudget;
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

    for (ResourceSubtypeData& subtypeData : s_resources->dataByType)
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

            subtypeData.data.EraseAt(i);

            if (subtypeData.hasProxyData)
            {
                AssertDebug(subtypeData.proxies.HasIndex(i), "Proxy missing for resource {}", rd.resource->Id());

                IRenderProxy* pProxy = subtypeData.proxies.Get(i);
                AssertDebug(pProxy != nullptr);

                subtypeData.proxies.EraseAt(i);
            }
        }

        subtypeData.indicesPendingDelete.Clear();
    }

    g_safeDeleter->UpdateEntryListQueue();

    g_engineStatsRecorder->Advance();

    g_safeDeleter->Iterate();

    g_renderArena->Reset();

    s_frameIndex[CONSUMER] = (s_frameIndex[CONSUMER] + 1) % RingBufferDepth;

    AtomicIncrement(&s_frameCounter);

    s_freeSemaphore.release();
}

} // namespace RenderApi

#pragma region RenderGlobalState

RenderGlobalState::RenderGlobalState()
    : shadowMapAllocator(PoolNew<ShadowMapAllocator>(*g_renderPool)),
      gpuBufferHolders(PoolNew<GpuBufferHolderMap>(*g_renderPool)),
      placeholderData(PoolNew<PlaceholderData>(*g_renderPool)),
      materialDescriptorSetManager(PoolNew<MaterialDescriptorSetManager>(*g_renderPool)),
      graphicsPipelineCache(PoolNew<GraphicsPipelineCache>(*g_renderPool)),
      bindlessStorage(PoolNew<BindlessStorage>(*g_renderPool)),
      finalPass(nullptr)
{
    AssertOnThread(g_renderThread);

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

    for (uint32 i = 0; i < GRT_MAX; i++)
    {
        globalRenderers[i] = Array<RendererBase*>();
    }

    globalRenderers[GRT_MAIN].PushBack(new DeferredRenderer);
    globalRenderers[GRT_MAIN][0]->Initialize();

    globalRenderers[GRT_ENV_PROBE].ResizeZeroed(EPT_MAX);
    globalRenderers[GRT_ENV_PROBE][EPT_REFLECTION] = new ReflectionProbeRenderer;
    globalRenderers[GRT_ENV_PROBE][EPT_SKY] = new ReflectionProbeRenderer;

    globalRenderers[GRT_ENV_GRID].PushBack(new EnvGridRenderer);

    globalRenderers[GRT_SHADOW_MAP].ResizeZeroed(LT_MAX); // 1 ShadowMapRenderer per LightType
    globalRenderers[GRT_SHADOW_MAP][LT_POINT] = new PointShadowRenderer;
    globalRenderers[GRT_SHADOW_MAP][LT_DIRECTIONAL] = new DirectionalShadowRenderer;

    // one global particle volume renderer
    globalRenderers[GRT_PARTICLE_VOLUME].ResizeZeroed(1);
    globalRenderers[GRT_PARTICLE_VOLUME][0] = new ParticleVolumeRenderer;
}

RenderGlobalState::~RenderGlobalState()
{
    bindlessStorage->UnsetAllResources();
    PoolDelete(*g_renderPool, bindlessStorage);
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

    PoolDelete(*g_renderPool, finalPass);
    finalPass = nullptr;

    PoolDelete(*g_renderPool, shadowMapAllocator);
    shadowMapAllocator = nullptr;

    PoolDelete(*g_renderPool, gpuBufferHolders);
    gpuBufferHolders = nullptr;

    PoolDelete(*g_renderPool, placeholderData);
    placeholderData = nullptr;

    PoolDelete(*g_renderPool, materialDescriptorSetManager);
    materialDescriptorSetManager = nullptr;

    PoolDelete(*g_renderPool, graphicsPipelineCache);
    graphicsPipelineCache = nullptr;
}

void RenderGlobalState::UpdateBuffers(FrameBase* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

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
    AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(!globalRenderers[globalRendererType].Contains(renderer));

    globalRenderers[globalRendererType].PushBack(renderer);
}

void RenderGlobalState::RemoveRenderer(GlobalRendererType globalRendererType, RendererBase* renderer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(globalRendererType != GRT_NONE && globalRendererType < GRT_MAX);

    AssertDebug(renderer != nullptr);
    AssertDebug(globalRenderers[globalRendererType].Contains(renderer));

    PoolDelete(*g_renderPool, renderer);

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
    AssertOnThread(g_renderThread);

    // Global
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("WorldsBuffer", gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("LightsBuffer", gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("CurrentLight", gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Global", frameIndex)
        ->SetElement("EntitiesBuffer", gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
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

    // Entity
    globalDescriptorTable->GetDescriptorSet("Entity", frameIndex)
        ->SetElement("CurrentEntity", gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Entity", frameIndex)
        ->SetElement("MaterialsBuffer", gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Entity", frameIndex)
        ->SetElement("SkeletonsBuffer", gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex));
    globalDescriptorTable->GetDescriptorSet("Entity", frameIndex)
        ->SetElement("LightmapVolumeIrradianceTexture", placeholderData->GetImageView2D1x1R8());
    globalDescriptorTable->GetDescriptorSet("Entity", frameIndex)
        ->SetElement("LightmapVolumeRadianceTexture", placeholderData->GetImageView2D1x1R8());

    // Material
    if (g_renderBackend->GetRenderConfig().bindlessTextures)
    {
        for (uint32 textureIndex = 0; textureIndex < MaxBindlessResources; textureIndex++)
        {
            globalDescriptorTable->GetDescriptorSet("Material", frameIndex)
                ->SetElement("Textures", textureIndex, g_renderBackend->GetTextureImageView(placeholderData->defaultTexture2d));
        }
    }
    else
    {
        for (Name textureName : Material::s_textureNames)
        {
            globalDescriptorTable->GetDescriptorSet("Material", frameIndex)
                ->SetElement(textureName, g_renderBackend->GetTextureImageView(placeholderData->defaultTexture2d));
        }
    }
}

#pragma endregion RenderGlobalState

namespace RenderApi {

DECLARE_RENDER_DATA_CONTAINER(Entity, RenderProxyMesh, GRB_ENTITIES, &WriteBufferData_MeshEntity, &s_meshEntityBinder);

DECLARE_RENDER_DATA_CONTAINER(Mesh, NullProxy, GRB_INVALID, nullptr, &s_meshBinder);

DECLARE_RENDER_DATA_CONTAINER(Camera, RenderProxyCamera, GRB_CAMERAS, nullptr, &s_cameraBinder);

DECLARE_RENDER_DATA_CONTAINER(EnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &WriteBufferData_EnvGrid, &s_envGridBinder);
DECLARE_RENDER_DATA_CONTAINER(LegacyEnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &WriteBufferData_EnvGrid, &s_envGridBinder);

DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &WriteBufferData_EnvProbe, &s_envProbeBinder);
DECLARE_RENDER_DATA_CONTAINER(ReflectionProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &WriteBufferData_EnvProbe, &s_envProbeBinder, &s_reflectionProbeTextureBinder);
DECLARE_RENDER_DATA_CONTAINER(SkyProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &WriteBufferData_EnvProbe, &s_envProbeBinder, &s_reflectionProbeTextureBinder);

DECLARE_RENDER_DATA_CONTAINER(Light, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(DirectionalLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(PointLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(AreaRectLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &s_lightBinder);
DECLARE_RENDER_DATA_CONTAINER(SpotLight, RenderProxyLight, GRB_LIGHTS, &WriteBufferData_Light, &s_lightBinder);

DECLARE_RENDER_DATA_CONTAINER(LightmapVolume, RenderProxyLightmapVolume, GRB_LIGHTMAP_VOLUMES, nullptr, &s_lightmapVolumeBinder);

DECLARE_RENDER_DATA_CONTAINER(ParticleVolume, RenderProxyParticleVolume, GRB_INVALID, nullptr, &s_particleVolumeBinder);

DECLARE_RENDER_DATA_CONTAINER(Material, RenderProxyMaterial, GRB_MATERIALS, nullptr, &s_materialBinder);

DECLARE_RENDER_DATA_CONTAINER(Texture, NullProxy, GRB_INVALID, nullptr, &s_textureBinder);

DECLARE_RENDER_DATA_CONTAINER(Skeleton, RenderProxySkeleton, GRB_SKELETONS, nullptr, &s_skeletonBinder);

} // namespace RenderApi

} // namespace hyperion

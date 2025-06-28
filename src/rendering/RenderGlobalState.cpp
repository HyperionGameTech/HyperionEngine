/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderSkeleton.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/GPUBufferHolderMap.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <rendering/rt/DDGI.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RenderingAPI.hpp>

#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>

#include <core/object/HypClass.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/threading/Semaphore.hpp>
#include <core/threading/Threads.hpp>

#include <core/memory/MemoryPool.hpp>
#include <core/memory/pool/MemoryPool2.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/BlueNoise.hpp>

#include <EngineGlobals.hpp>

#include <semaphore>

namespace hyperion {

static constexpr uint32 num_frames = 3; // number of frames in the ring buffer
static constexpr uint32 max_views_per_frame = 16;
static constexpr uint32 max_frames_before_discard = 4; // number of frames before ViewData is discarded if not written to

// global ring buffer for the game and render threads to write/read data from
static std::atomic_uint g_producer_index { 0 };                             // where the game will write next
static std::atomic_uint g_consumer_index { 0 };                             // what the renderer is *about* to draw
static std::atomic_uint g_frame_counter { 0 };                              // logical frame number
static std::counting_semaphore<num_frames> g_full_semaphore { 0 };          // renderer waits here
static std::counting_semaphore<num_frames> g_free_semaphore { num_frames }; // game waits here when ring is full

// Shared allocator for reflection probes and sky probes.
static ResourceBindingAllocator<16> g_envprobe_bindings_allocator;

// globally lock resources from being destroyed while the frame is rendering
struct RenderResourceLock_Impl
{
    AtomicSemaphore semaphore;
};

#pragma region ResourceContainer

class NullProxyType : public IRenderProxy
{
};

/*! \brief Holds points to used resources and use count (for rendering).
 *  When the use count reaches zero, it is marked for deferred deletion from
 *  the render thread to ensure no frames are currently using it for rendering as it gets deleted */
struct ResourceData final
{
    HypObjectBase* resource;
    AtomicVar<uint32> count;

    ResourceData(HypObjectBase* resource)
        : resource(resource),
          count(0)
    {
        AssertDebug(resource);

        resource->GetObjectHeader_Internal()->IncRefStrong();
    }

    ResourceData(const ResourceData& other) = delete;
    ResourceData& operator=(const ResourceData& other) = delete;

    ResourceData(ResourceData&& other) noexcept = delete;
    ResourceData& operator=(ResourceData&& other) noexcept = delete;

    ~ResourceData()
    {
        if (resource)
        {
            resource->GetObjectHeader_Internal()->DecRefStrong();
        }
    }
};

struct ResourceSubtypeData final
{
    static constexpr uint32 pool_page_size = 1024;

    // subtype TypeID (ID should map to the same TypeID have it as a base class)
    TypeID type_id;

    // Map from ID -> ResourceData holding ptr to resource and use count.
    SparsePagedArray<ResourceData, 256> data;

    // Indices marked for deletion from the render thread, to be removed from the resource container and SafeRelease()'d
    Bitset indices_pending_delete;

    // Indices marked for update from the game thread, for the render thread to perform GPU buffer updates to
    Bitset indices_pending_buffer_update;

    // == optional render proxy data ==
    // a pool for allocating render proxies:
    ValueStorage<Pool<pool_page_size>> proxy_pool;
    // and a map (sparse array) from ID -> proxy ptr (allocated from that pool)
    SparsePagedArray<IRenderProxy*, 256> proxies;
    void (*proxy_ctor)(IRenderProxy* proxy);
    bool has_proxy_pool : 1;

    template <class ResourceType, class ProxyType>
    ResourceSubtypeData(TypeWrapper<ResourceType>, TypeWrapper<ProxyType>)
        : type_id(TypeID::ForType<ResourceType>()),
          has_proxy_pool(!std::is_same_v<ProxyType, NullProxyType>), // if ProxyType != NullProxyType then we setup proxy pool
          proxy_ctor(nullptr)
    {
        if (has_proxy_pool)
        {
            proxy_pool.Construct(sizeof(ProxyType), alignof(ProxyType));
            proxy_ctor = [](IRenderProxy* proxy)
            {
                new (proxy) ProxyType;
            };
        }
    }

    ResourceSubtypeData(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData& operator=(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData(ResourceSubtypeData&& other) noexcept = default;
    ResourceSubtypeData& operator=(ResourceSubtypeData&& other) noexcept = default;

    ~ResourceSubtypeData()
    {
        if (has_proxy_pool)
        {
            auto* pool = &proxy_pool.Get();
            for (IRenderProxy* proxy : proxies)
            {
                // pool doesnt call destructors so invoke manually
                proxy->~IRenderProxy();
                pool->Free(proxy);
            }

            proxies.Clear();

            AssertThrowMsg(pool->Size() == 0, "Memory leak detected: render proxy pool not empty upon deletion!");

            proxy_pool.Destruct();
        }

        AssertDebug(proxies.Empty());
    }
};

struct ResourceContainer
{
    ResourceSubtypeData& GetSubtypeData(TypeID type_id)
    {
        auto cache_it = cache.Find(type_id);
        if (cache_it != cache.End())
        {
            return *cache_it->second;
        }

        const HypClass* hyp_class = GetClass(type_id);
        AssertDebugMsg(hyp_class != nullptr, "TypeID %u does not have a HypClass!", type_id.Value());

        int static_index = -1;

        do
        {
            static_index = hyp_class->GetStaticIndex();
            AssertDebugMsg(static_index >= 0, "Invalid class to use with render resource lock: '%s' has no assigned static index!", *hyp_class->GetName());

            if (ResourceSubtypeData* subtype_data = data_by_type.TryGet(static_index))
            {
                // found the subtype data for this type_id - cache it for O(1) retrieval next time
                cache[type_id] = subtype_data;

                return *subtype_data;
            }

            hyp_class = hyp_class->GetParent();
        }
        while (hyp_class);

        HYP_FAIL("No SubtypeData container found for TypeID %u (HypClass: %s)! Missing DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?", type_id.Value(), *GetClass(type_id)->GetName());
    }

    SparsePagedArray<ResourceSubtypeData, 16> data_by_type;
    HashMap<TypeID, ResourceSubtypeData*> cache;
};

struct ResourceContainerFactoryRegistry
{
    Array<void (*)(ResourceContainer&)> funcs;

    static ResourceContainerFactoryRegistry& GetInstance()
    {
        static ResourceContainerFactoryRegistry instance;
        return instance;
    }

    void InvokeAll(ResourceContainer& resource_container)
    {
        for (auto&& func : funcs)
        {
            func(resource_container);
        }
    }
};

template <class ResourceType, class ProxyType = NullProxyType>
struct ResourceContainerFactory
{
    ResourceContainerFactory()
    {
        ResourceContainerFactoryRegistry::GetInstance().funcs.PushBack([](ResourceContainer& container)
            {
                static constexpr TypeID type_id = TypeID::ForType<ResourceType>();

                const HypClass* hyp_class = GetClass(type_id);
                AssertDebugMsg(hyp_class != nullptr, "TypeID %u does not have a HypClass!", type_id.Value());

                const int static_index = hyp_class->GetStaticIndex();
                AssertDebugMsg(static_index >= 0, "Invalid class to use with render resource lock: '%s' has no assigned static index!", *hyp_class->GetName());

                AssertDebugMsg(!container.data_by_type.HasIndex(static_index),
                    "SubtypeData container already exists for TypeID %u (HypClass: %s)! Duplicate DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?",
                    type_id.Value(), *GetClass(type_id)->GetName());

                container.data_by_type.Emplace(static_index, TypeWrapper<ResourceType>(), TypeWrapper<ProxyType>());
            });
    }
};

#define DECLARE_RENDER_DATA_CONTAINER(resource_type, ...) \
    static ResourceContainerFactory<class resource_type, ##__VA_ARGS__> g_##resource_type##_container_factory {};

#pragma endregion ResourceContainer

struct ViewData
{
    RenderProxyList render_proxy_list;
    uint32 frames_since_write : 4; // frames_since_write of this view in the current frame, used to determine if the view is still alive if not reset to zero.
};

struct FrameData
{
    LinkedList<ViewData> per_view_data;
    HashMap<View*, ViewData*> views;
    ResourceContainer resources;
};

static FrameData g_frame_data[num_frames];

HYP_API void RendererAPI_InitResourceContainers()
{
    Threads::AssertOnThread(g_main_thread);

    ResourceContainerFactoryRegistry& registry = ResourceContainerFactoryRegistry::GetInstance();

    for (uint32 i = 0; i < num_frames; i++)
    {
        registry.InvokeAll(g_frame_data[i].resources);
    }

    registry.funcs.Clear();
}

HYP_API uint32 RendererAPI_GetFrameIndex_RenderThread()
{
    return g_consumer_index.load(std::memory_order_relaxed);
}

HYP_API uint32 RendererAPI_GetFrameIndex_GameThread()
{
    return g_producer_index.load(std::memory_order_relaxed);
}

HYP_API RenderProxyList& RendererAPI_GetProducerProxyList(View* view)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    ViewData*& vd = g_frame_data[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frame_data[slot].per_view_data.EmplaceBack();
        vd->render_proxy_list.state = RenderProxyList::CS_WRITING;
    }

    // reset the frames_since_write counter
    vd->frames_since_write = 0;

    return vd->render_proxy_list;
}

HYP_API RenderProxyList& RendererAPI_GetConsumerProxyList(View* view)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);

    const uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    ViewData*& vd = g_frame_data[slot].views[view];

    if (!vd)
    {
        // create a new pool for this view
        vd = &g_frame_data[slot].per_view_data.EmplaceBack();
        vd->render_proxy_list.state = RenderProxyList::CS_READING;
    }

    return vd->render_proxy_list;
}

HYP_API void RendererAPI_AddRef(HypObjectBase* resource)
{
    if (!resource)
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    const IDBase resource_id = resource->GetID();

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& fd = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = fd.resources.GetSubtypeData(resource_id.GetTypeID());
    ResourceData* rd = subtype_data.data.TryGet(resource_id.ToIndex());

    if (!rd)
    {
        rd = &*subtype_data.data.Emplace(resource_id.ToIndex(), resource);
    }

    rd->count.Increment(1, MemoryOrder::RELAXED);

    subtype_data.indices_pending_delete.Set(resource_id.ToIndex(), false);
}

HYP_API void RendererAPI_ReleaseRef(IDBase id)
{
    if (!id.IsValid())
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& fd = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = fd.resources.GetSubtypeData(id.GetTypeID());
    ResourceData* rd = subtype_data.data.TryGet(id.ToIndex());

    if (!rd)
    {
        return; // no ref count for this resource
    }

    if (rd->count.Decrement(1, MemoryOrder::RELAXED) == 1)
    {
        subtype_data.indices_pending_delete.Set(id.ToIndex(), true);
    }
}

HYP_API void RendererAPI_UpdateRenderProxy(IDBase id)
{
    if (!id.IsValid())
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& fd = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = fd.resources.GetSubtypeData(id.GetTypeID());
    ResourceData& data = subtype_data.data.Get(id.ToIndex());

    AssertDebugMsg(subtype_data.has_proxy_pool, "Cannot use UpdateResource() for type which does not have a RenderProxy! TypeID: %u, HypClass %s",
        subtype_data.type_id.Value(), *GetClass(subtype_data.type_id)->GetName());

    IRenderProxy* proxy;

    if (IRenderProxy** p = subtype_data.proxies.TryGet(id.ToIndex()))
    {
        proxy = *p;
    }
    else
    {
        proxy = static_cast<IRenderProxy*>(subtype_data.proxy_pool.Get().Alloc());
        AssertThrowMsg(proxy != nullptr, "Failed to allocate render proxy!");

        // construct proxy object
        AssertDebug(subtype_data.proxy_ctor != nullptr);
        subtype_data.proxy_ctor(proxy);

        subtype_data.proxies.Emplace(id.ToIndex(), proxy);
    }

    AssertDebug(proxy);
    AssertDebug(data.resource);

    if (IsInstanceOfHypClass(Entity::Class(), data.resource->InstanceClass()))
    {
        Entity* entity = static_cast<Entity*>(data.resource);
        entity->UpdateRenderProxy(proxy);
    }
    else
    {
        HYP_LOG(Rendering, Warning, "UpdateRenderProxy called for resource ID {} of type {} which is not an Entity! Skipping proxy update.\n",
            id, GetClass(subtype_data.type_id)->GetName());
    }

    // mark for buffer data update from render thread
    subtype_data.indices_pending_buffer_update.Set(id.ToIndex(), true);
}

HYP_API void RendererAPI_UpdateRenderProxy(IDBase id, IRenderProxy* proxy)
{
    if (!id.IsValid())
    {
        return;
    }

    AssertDebug(proxy != nullptr);

    Threads::AssertOnThread(g_game_thread);

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& fd = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = fd.resources.GetSubtypeData(id.GetTypeID());
    ResourceData& data = subtype_data.data.Get(id.ToIndex());

    AssertDebugMsg(subtype_data.has_proxy_pool, "Cannot use UpdateResource() for type which does not have a RenderProxy! TypeID: %u, HypClass %s",
        subtype_data.type_id.Value(), *GetClass(subtype_data.type_id)->GetName());

    // update proxy
    subtype_data.proxies.Set(id.ToIndex(), proxy);

    HYP_LOG(Rendering, Debug, "UPDATING NOT IMPLEMENTED!!! FIXME!!!");

    // AssertDebugMsg(IsInstanceOfHypClass(Entity::Class(), GetClass(subtype_data.type_id)), "Cannot use UpdateResource() with class that is not a subtype of Entity! TypeID: %u, HypClass %s",
    //     subtype_data.type_id.Value(), *GetClass(subtype_data.type_id)->GetName());

    // Entity* entity = static_cast<Entity*>(data.resource);
    // AssertDebug(entity);

    // entity->UpdateRenderProxy(proxy);

    // mark for buffer data update from render thread!
    subtype_data.indices_pending_buffer_update.Set(id.ToIndex(), true);
}

HYP_API IRenderProxy* RendererAPI_GetRenderProxy(IDBase id)
{
    AssertDebug(id.IsValid());
    AssertDebug(id.GetTypeID() != TypeID::Void());

    Threads::AssertOnThread(g_render_thread);

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = frame_data.resources.GetSubtypeData(id.GetTypeID());
    AssertDebugMsg(subtype_data.has_proxy_pool, "Cannot use GetRenderProxy() for type which does not have a RenderProxy! TypeID: %u, HypClass %s",
        subtype_data.type_id.Value(), *GetClass(subtype_data.type_id)->GetName());

    if (!subtype_data.proxies.HasIndex(id.ToIndex()))
    {
        HYP_LOG(Rendering, Warning, "No render proxy found for resource ID {} of type {} in frame {}!\n",
            id, GetClass(subtype_data.type_id)->GetName(), slot);

        return nullptr; // no proxy for this resource
    }

    IRenderProxy* proxy = subtype_data.proxies.Get(id.ToIndex());
    AssertDebug(proxy != nullptr);

    return proxy;
}

HYP_API void RendererAPI_BeginFrame_GameThread()
{
    HYP_SCOPE;

    g_free_semaphore.acquire();

    uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    // frame_data.resource_locks.ResetAllLocks();

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;
        AssertDebug(vd.render_proxy_list.state != RenderProxyList::CS_READING);
        vd.render_proxy_list.state = RenderProxyList::CS_WRITING;
    }
}

HYP_API void RendererAPI_EndFrame_GameThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_game_thread);
#endif

    uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;
        AssertDebug(vd.render_proxy_list.state == RenderProxyList::CS_WRITING);
        vd.render_proxy_list.state = RenderProxyList::CS_WRITTEN;
    }

    g_producer_index.store((g_producer_index.load(std::memory_order_relaxed) + 1) % num_frames, std::memory_order_relaxed);
    g_frame_counter.fetch_add(1, std::memory_order_release); // publish the new frame #

    g_full_semaphore.release(); // a frame is ready for the renderer
}

HYP_API void RendererAPI_BeginFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_render_thread);
#endif

    g_full_semaphore.acquire();

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;
        vd.render_proxy_list.state = RenderProxyList::CS_READING;
    }

    // Reset binding allocators at the end of the frame
    g_envprobe_bindings_allocator.Reset();
}

HYP_API void RendererAPI_EndFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_render_thread);
#endif

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    // cull ViewData that hasn't been written to for a while
    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End();)
    {
        ViewData& vd = *it;
        AssertDebug(vd.render_proxy_list.state == RenderProxyList::CS_READING);
        vd.render_proxy_list.state = RenderProxyList::CS_DONE;

        vd.render_proxy_list.RemoveEmptyRenderGroups();

        // Clear out data for views that haven't been written to for a while
        if (vd.frames_since_write == max_frames_before_discard)
        {
            DebugLog(LogType::Debug, "Clearing draw collection for view %p in frame %u\n", &vd.render_proxy_list, slot);

            auto view_it = frame_data.views.FindIf([&vd](const auto& pair)
                {
                    return pair.second == &vd;
                });

            AssertDebug(view_it != frame_data.views.End());

            frame_data.views.Erase(view_it);

            it = frame_data.per_view_data.Erase(it);

            /// TODO: Clear tracked resources - DecRef needs to be called on all resources in the render_proxy_list

            continue;
        }

        ++it;
    }

    for (ResourceSubtypeData& subtype_data : frame_data.resources.data_by_type)
    {
        // Handle proxies that were updated on game thread
        for (Bitset::BitIndex bit : subtype_data.indices_pending_buffer_update)
        {
            // sanity check:
            ResourceData& rd = subtype_data.data.Get(bit);
            AssertDebug(rd.count.Get(MemoryOrder::RELAXED) > 0);

            IRenderProxy* proxy = subtype_data.proxies.Get(bit);
            AssertDebug(proxy != nullptr);

            /// @TODO Handle buffer update into global GPU buffer data...
            HYP_LOG(Rendering, Debug, "Updating resource of type {} with ID {} on GPU thread..\n",
                *GetClass(subtype_data.type_id)->GetName(), IDBase { subtype_data.type_id, uint32(bit) + 1 }.Value());
        }

        subtype_data.indices_pending_buffer_update.Clear();

        // @TODO: for deletion, have a fixed number to iterate over per frame so we don't spend too much time on it.
        // Remove resources pending deletion via SafeDelete() for indices marked for deletion from the game thread
        for (Bitset::BitIndex bit : subtype_data.indices_pending_delete)
        {
            ResourceData& rd = subtype_data.data.Get(bit);
            AssertDebug(rd.count.Get(MemoryOrder::RELAXED) == 0);

            HYP_LOG(Rendering, Debug, "Deleting resource of type {} with ID {}\n",
                *GetClass(subtype_data.type_id)->GetName(), IDBase { subtype_data.type_id, uint32(bit) + 1 }.Value());

            // Swap refcount owner over to the Handle
            AnyHandle resource { rd.resource };
            subtype_data.data.EraseAt(bit);

            if (subtype_data.has_proxy_pool)
            {
                AssertDebugMsg(subtype_data.proxies.HasIndex(bit), "proxy missing at index: %u", bit);

                IRenderProxy* proxy = subtype_data.proxies.Get(bit);
                AssertDebug(proxy);

                subtype_data.proxies.EraseAt(bit);

                // safely release the proxy's resources before we destruct it:
                proxy->SafeRelease();

                // pool doesnt call destructors so invoke manually
                proxy->~IRenderProxy();

#ifdef HYP_DEBUG_MODE
                // :)
                proxy->state = 0xDEAD;
#endif

                subtype_data.proxy_pool.Get().Free(proxy);
            }

            // safely release all the held resources:
            if (resource.IsValid())
            {
                g_safe_deleter->SafeRelease(std::move(resource));
            }
        }

        subtype_data.indices_pending_delete.Clear();
    }

    g_consumer_index.store((slot + 1) % num_frames, std::memory_order_relaxed);

    g_free_semaphore.release();
}

#pragma region RenderGlobalState

RenderGlobalState::RenderGlobalState()
    : ResourceBinders { nullptr },
      EnvProbeBinder(this, &g_envprobe_bindings_allocator),
      ShadowMapAllocator(MakeUnique<class ShadowMapAllocator>()),
      GPUBufferHolderMap(MakeUnique<class GPUBufferHolderMap>()),
      PlaceholderData(MakeUnique<class PlaceholderData>())
{
    Worlds = GPUBufferHolderMap->GetOrCreate<WorldShaderData, GPUBufferType::CBUFF>();
    Cameras = GPUBufferHolderMap->GetOrCreate<CameraShaderData, GPUBufferType::CBUFF>();
    Lights = GPUBufferHolderMap->GetOrCreate<LightShaderData, GPUBufferType::SSBO>();
    Entities = GPUBufferHolderMap->GetOrCreate<EntityShaderData, GPUBufferType::SSBO>();
    Materials = GPUBufferHolderMap->GetOrCreate<MaterialShaderData, GPUBufferType::SSBO>();
    Skeletons = GPUBufferHolderMap->GetOrCreate<SkeletonShaderData, GPUBufferType::SSBO>();
    ShadowMaps = GPUBufferHolderMap->GetOrCreate<ShadowMapShaderData, GPUBufferType::SSBO>();
    EnvProbes = GPUBufferHolderMap->GetOrCreate<EnvProbeShaderData, GPUBufferType::SSBO>();
    EnvGrids = GPUBufferHolderMap->GetOrCreate<EnvGridShaderData, GPUBufferType::CBUFF>();
    LightmapVolumes = GPUBufferHolderMap->GetOrCreate<LightmapVolumeShaderData, GPUBufferType::SSBO>();

    GlobalDescriptorTable = g_rendering_api->MakeDescriptorTable(&GetStaticDescriptorTableDeclaration());

    PlaceholderData->Create();
    ShadowMapAllocator->Initialize();

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        SetDefaultDescriptorSetElements(frame_index);
    }

    CreateSphereSamplesBuffer();
    CreateBlueNoiseBuffer();

    GlobalDescriptorTable->Create();

    BindlessTextures.Create();

    Renderer = new DeferredRenderer();
    Renderer->Initialize();

    EnvProbeRenderers = new EnvProbeRenderer*[EPT_MAX];
    Memory::MemSet(EnvProbeRenderers, 0, sizeof(EnvProbeRenderer*) * EPT_MAX);

    EnvProbeRenderers[EPT_REFLECTION] = new ReflectionProbeRenderer();
    EnvProbeRenderers[EPT_SKY] = new ReflectionProbeRenderer();
}

RenderGlobalState::~RenderGlobalState()
{
    BindlessTextures.Destroy();
    ShadowMapAllocator->Destroy();
    GlobalDescriptorTable->Destroy();
    PlaceholderData->Destroy();

    for (uint32 i = 0; i < EPT_MAX; i++)
    {
        if (EnvProbeRenderers[i])
        {
            EnvProbeRenderers[i]->Shutdown();
            delete EnvProbeRenderers[i];
        }
    }

    delete[] EnvProbeRenderers;

    Renderer->Shutdown();
    delete Renderer;
    Renderer = nullptr;
}

void RenderGlobalState::UpdateBuffers(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (auto& it : GPUBufferHolderMap->GetItems())
    {
        it.second->UpdateBufferSize(frame->GetFrameIndex());
        it.second->UpdateBufferData(frame->GetFrameIndex());
    }
}

void RenderGlobalState::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    static_assert(sizeof(BlueNoiseBuffer::sobol_256spp_256d) == sizeof(BlueNoise::sobol_256spp_256d));
    static_assert(sizeof(BlueNoiseBuffer::scrambling_tile) == sizeof(BlueNoise::scrambling_tile));
    static_assert(sizeof(BlueNoiseBuffer::ranking_tile) == sizeof(BlueNoise::ranking_tile));

    constexpr SizeType blue_noise_buffer_size = sizeof(BlueNoiseBuffer);

    constexpr SizeType sobol_256spp_256d_offset = offsetof(BlueNoiseBuffer, sobol_256spp_256d);
    constexpr SizeType sobol_256spp_256d_size = sizeof(BlueNoise::sobol_256spp_256d);
    constexpr SizeType scrambling_tile_offset = offsetof(BlueNoiseBuffer, scrambling_tile);
    constexpr SizeType scrambling_tile_size = sizeof(BlueNoise::scrambling_tile);
    constexpr SizeType ranking_tile_offset = offsetof(BlueNoiseBuffer, ranking_tile);
    constexpr SizeType ranking_tile_size = sizeof(BlueNoise::ranking_tile);

    static_assert(blue_noise_buffer_size == (sobol_256spp_256d_offset + sobol_256spp_256d_size) + ((scrambling_tile_offset - (sobol_256spp_256d_offset + sobol_256spp_256d_size)) + scrambling_tile_size) + ((ranking_tile_offset - (scrambling_tile_offset + scrambling_tile_size)) + ranking_tile_size));

    GPUBufferRef blue_noise_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::SSBO, sizeof(BlueNoiseBuffer));
    HYPERION_ASSERT_RESULT(blue_noise_buffer->Create());
    blue_noise_buffer->Copy(sobol_256spp_256d_offset, sobol_256spp_256d_size, &BlueNoise::sobol_256spp_256d[0]);
    blue_noise_buffer->Copy(scrambling_tile_offset, scrambling_tile_size, &BlueNoise::scrambling_tile[0]);
    blue_noise_buffer->Copy(ranking_tile_offset, ranking_tile_size, &BlueNoise::ranking_tile[0]);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("BlueNoiseBuffer"), blue_noise_buffer);
    }
}

void RenderGlobalState::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;

    GPUBufferRef sphere_samples_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CBUFF, sizeof(Vec4f) * 4096);
    HYPERION_ASSERT_RESULT(sphere_samples_buffer->Create());

    Vec4f* sphere_samples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++)
    {
        Vec3f sample = MathUtil::RandomInSphere(Vec3f {
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed) });

        sphere_samples[i] = Vec4f(sample, 0.0f);
    }

    sphere_samples_buffer->Copy(sizeof(Vec4f) * 4096, sphere_samples);

    delete[] sphere_samples;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("SphereSamplesBuffer"), sphere_samples_buffer);
    }
}

void RenderGlobalState::SetDefaultDescriptorSetElements(uint32 frame_index)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // Global
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("WorldsBuffer"), Worlds->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightsBuffer"), Lights->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentLight"), Lights->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ObjectsBuffer"), Entities->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CamerasBuffer"), Cameras->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridsBuffer"), EnvGrids->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbesBuffer"), EnvProbes->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentEnvProbe"), EnvProbes->GetBuffer(frame_index));

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("VoxelGridTexture"), PlaceholderData->GetImageView3D1x1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldColorTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldDepthTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("BlueNoiseBuffer"), GPUBufferRef::Null());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SphereSamplesBuffer"), GPUBufferRef::Null());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), PlaceholderData->GetImageView2D1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), PlaceholderData->GetImageViewCube1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsBuffer"), ShadowMaps->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightmapVolumesBuffer"), LightmapVolumes->GetBuffer(frame_index));

    for (uint32 i = 0; i < max_bound_reflection_probes; i++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), i, PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView());
    }

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIUniforms"), PlaceholderData->GetOrCreateBuffer(GPUBufferType::CBUFF, sizeof(DDGIUniforms), true /* exact size */));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIIrradianceTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIDepthTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("RTRadianceResultTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerNearest"), PlaceholderData->GetSamplerNearest());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SamplerLinear"), PlaceholderData->GetSamplerLinearMipmap());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("UITexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("FinalOutputTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), ShadowMapAllocator->GetAtlasImageView());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), ShadowMapAllocator->GetPointLightShadowMapImageView());

    // Object
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("CurrentObject"), Entities->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("MaterialsBuffer"), Materials->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("SkeletonsBuffer"), Skeletons->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeIrradianceTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeRadianceTexture"), PlaceholderData->GetImageView2D1x1R8());

    // Material
    if (g_rendering_api->GetRenderConfig().IsBindlessSupported())
    {
        for (uint32 texture_index = 0; texture_index < max_bindless_resources; texture_index++)
        {
            GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), texture_index, PlaceholderData->GetImageView2D1x1R8());
        }
    }
}

void RenderGlobalState::OnEnvProbeBindingChanged(EnvProbe* env_probe, uint32 prev, uint32 next)
{
    AssertDebug(env_probe != nullptr);
    AssertDebug(env_probe->IsReady());

    if (!env_probe->GetPrefilteredEnvMap().IsValid())
    {
        HYP_LOG(Rendering, Error, "EnvProbe {} (class: {}) has no prefiltered env map set!\n", env_probe->GetID(),
            env_probe->InstanceClass()->GetName());

        return;
    }

    DebugLog(LogType::Debug, "EnvProbe %u (class: %s) binding changed from %u to %u\n", env_probe->GetID().Value(),
        *env_probe->InstanceClass()->GetName(),
        prev, next);

    if (prev != ~0u)
    {
        HYP_LOG(Rendering, Debug, "UN setting env probe texture at index: {}", prev);
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), prev, g_render_global_state->PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView());
        }
    }
    else
    {
        env_probe->GetRenderResource().IncRef();
        env_probe->GetPrefilteredEnvMap()->GetRenderResource().IncRef();
    }

    // temp shit
    env_probe->GetRenderResource().SetTextureSlot(next);

    if (next != ~0u)
    {
        AssertDebug(env_probe->GetPrefilteredEnvMap().IsValid());
        AssertDebug(env_probe->GetPrefilteredEnvMap()->IsReady());

        HYP_LOG(Rendering, Debug, "Setting env probe texture at index: {} to tex with ID: {}", next, env_probe->GetPrefilteredEnvMap().GetID());
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), next, env_probe->GetPrefilteredEnvMap()->GetRenderResource().GetImageView());
        }
    }
    else
    {
        env_probe->GetRenderResource().DecRef();
        env_probe->GetPrefilteredEnvMap()->GetRenderResource().DecRef();
    }
}

#pragma endregion RenderGlobalState

// @NOTE: Not declaring for proxy Entity atm, since we are allocating them with new rather than
// the pool.. come back and FIXME
DECLARE_RENDER_DATA_CONTAINER(Entity); // RenderProxy);
DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe);
DECLARE_RENDER_DATA_CONTAINER(Light, RenderProxyLight);

} // namespace hyperion

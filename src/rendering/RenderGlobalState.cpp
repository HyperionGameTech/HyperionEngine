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
#include <rendering/GpuBufferHolderMap.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <rendering/rt/DDGI.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RenderBackend.hpp>

#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>

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
static std::atomic_uint g_producer_index { 0 }; // where the game will write next
static std::atomic_uint g_consumer_index { 0 }; // what the renderer is *about* to draw
static std::atomic_uint g_frame_counter { 0 };  // logical frame number

// thread-local frame index for the game and render threads
// @NOTE: thread local so initialized to 0 on each thread by default
thread_local std::atomic_uint* g_thread_frame_index;

static std::counting_semaphore<num_frames> g_full_semaphore { 0 };          // renderer waits here
static std::counting_semaphore<num_frames> g_free_semaphore { num_frames }; // game waits here when ring is full

#pragma region ResourceBindings

typedef void (*WriteBufferDataFunction)(GpuBufferHolderBase* gpu_buffer_holder, uint32 idx, void* buffer_data, SizeType size);

extern void OnBindingChanged_ReflectionProbe(EnvProbe* env_probe, uint32 prev, uint32 next);
extern void OnBindingChanged_AmbientProbe(EnvProbe* env_probe, uint32 prev, uint32 next);
extern void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpu_buffer_holder, uint32 idx, void* buffer_data, SizeType size);

extern void OnBindingChanged_EnvGrid(EnvGrid* env_grid, uint32 prev, uint32 next);

extern void OnBindingChanged_Light(Light* light, uint32 prev, uint32 next);
extern void OnBindingChanged_LightmapVolume(LightmapVolume* lightmap_volume, uint32 prev, uint32 next);

struct ResourceBindings
{
    struct SubtypeResourceBindings
    {
        // Map Id -> binding value
        SparsePagedArray<uint32, 1024> data;
        // Map Id -> resource reference count
        SparsePagedArray<AtomicVar<uint32>, 1024> ref_counts;
    };

    SparsePagedArray<SubtypeResourceBindings, 16> subtype_bindings;
    HashMap<TypeId, SubtypeResourceBindings*> cache;

    // Shared index allocator for reflection probes and sky probes.
    ResourceBindingAllocator<max_bound_reflection_probes> reflection_probe_bindings_allocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_ReflectionProbe> reflection_probe_binder { &reflection_probe_bindings_allocator };

    // ambient probes
    ResourceBindingAllocator<> ambient_probe_bindings_allocator;
    ResourceBinder<EnvProbe, &OnBindingChanged_AmbientProbe> ambient_probe_binder { &ambient_probe_bindings_allocator };

    ResourceBindingAllocator<16> env_grid_bindings_allocator;
    ResourceBinder<EnvGrid, &OnBindingChanged_EnvGrid> env_grid_binder { &env_grid_bindings_allocator };

    ResourceBindingAllocator<> light_bindings_allocator;
    ResourceBinder<Light, &OnBindingChanged_Light> light_binder { &light_bindings_allocator };

    ResourceBindingAllocator<> lightmap_volume_bindings_allocator;
    ResourceBinder<LightmapVolume, &OnBindingChanged_LightmapVolume> lightmap_volume_binder { &lightmap_volume_bindings_allocator };

    void Assign(HypObjectBase* resource, uint32 binding)
    {
        AssertDebug(resource != nullptr);

        ObjIdBase id = resource->Id();
        AssertDebug(id.IsValid());

        SubtypeResourceBindings& bindings = GetSubtypeBindings(id.GetTypeId());

        if (binding == ~0u)
        {
            bindings.data.EraseAt(id.ToIndex());
            bindings.ref_counts.EraseAt(id.ToIndex());
        }
        else
        {
            bindings.data.Set(id.ToIndex(), binding);
            bindings.ref_counts.Emplace(id.ToIndex(), 0);
        }
    }

    HYP_FORCE_INLINE uint32 Retrieve(const HypObjectBase* resource) const
    {
        return resource ? Retrieve(resource->Id()) : ~0u;
    }

    HYP_FORCE_INLINE uint32 Retrieve(ObjIdBase id) const
    {
        if (!id.IsValid())
        {
            return ~0u; // invalid resource
        }

        const SubtypeResourceBindings& bindings = const_cast<ResourceBindings*>(this)->GetSubtypeBindings(id.GetTypeId());

        const uint32* binding = bindings.data.TryGet(id.ToIndex());

        return binding ? *binding : ~0u;
    }

    void AddRef(ObjIdBase id)
    {
        AssertDebug(id.IsValid());

        SubtypeResourceBindings& bindings = GetSubtypeBindings(id.GetTypeId());

        AtomicVar<uint32>* ref_count = bindings.ref_counts.TryGet(id.ToIndex());
        AssertDebug(ref_count != nullptr, "No ref count for resource with id %u!", id.Value());

        ref_count->Increment(1, MemoryOrder::RELEASE);
    }

    void ReleaseRef(ObjIdBase id)
    {
        AssertDebug(id.IsValid());

        SubtypeResourceBindings& bindings = GetSubtypeBindings(id.GetTypeId());

        AtomicVar<uint32>* ref_count = bindings.ref_counts.TryGet(id.ToIndex());
        AssertDebug(ref_count != nullptr, "No ref count for resource with id %u!", id.Value());

        ref_count->Decrement(1, MemoryOrder::RELEASE);
    }

    SubtypeResourceBindings& GetSubtypeBindings(TypeId type_id)
    {
        auto cache_it = cache.Find(type_id);
        if (cache_it != cache.End())
        {
            return *cache_it->second;
        }

        const HypClass* hyp_class = GetClass(type_id);
        AssertDebug(hyp_class != nullptr, "TypeId %u does not have a HypClass!", type_id.Value());

        int static_index = -1;

        do
        {
            static_index = hyp_class->GetStaticIndex();
            AssertDebugMsg(static_index >= 0, "Invalid class: '%s' has no assigned static index!", *hyp_class->GetName());

            if (SubtypeResourceBindings* bindings = subtype_bindings.TryGet(static_index))
            {
                cache[type_id] = bindings;

                return *bindings;
            }

            hyp_class = hyp_class->GetParent();
        }
        while (hyp_class);

        HYP_FAIL("No SubtypeBindings container found for TypeId %u (HypClass: %s)! Missing DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?", type_id.Value(), *GetClass(type_id)->GetName());
    }
};

#pragma endregion ResourceBindings

#pragma region ResourceContainer

class NullProxy : public IRenderProxy
{
};

struct RenderProxyAllocator
{
    SizeType class_size = 0;
    SizeType class_alignment = 0;
    DynamicAllocator allocator;

    void* Alloc()
    {
        if (class_size == 0 || class_alignment == 0)
        {
            return nullptr;
        }

        return allocator.Allocate(class_size, class_alignment);
    }

    void Free(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        allocator.Free(ptr);
    }
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

HYP_DISABLE_OPTIMIZATION;

struct ResourceSubtypeData final
{
    static constexpr uint32 pool_page_size = 1024;

    // subtype TypeId (Id should map to the same TypeId have it as a base class)
    TypeId type_id;

    // Map from Id -> ResourceData holding ptr to resource and use count.
    SparsePagedArray<ResourceData, 256> data;

    // Indices marked for deletion from the render thread, to be removed from the resource container and SafeRelease()'d
    Bitset indices_pending_delete;

    // Indices marked for update from the game thread, for the render thread to perform GPU buffer updates to
    Bitset indices_pending_update;

    ResourceBinderBase* resource_binder = nullptr;
    GpuBufferHolderBase* gpu_buffer_holder = nullptr;
    WriteBufferDataFunction write_buffer_data_fn = nullptr;

    // == optional render proxy data ==
    // a pool for allocating render proxies:
    RenderProxyAllocator proxy_allocator;

    // and a map (sparse array) from Id -> proxy ptr (allocated from that pool)
    SparsePagedArray<IRenderProxy*, 256> proxies;
    IRenderProxy* (*proxy_ctor)(void* ptr);
    void (*set_gpu_elem)(ResourceSubtypeData* _this, uint32 idx, IRenderProxy* proxy);
    bool has_proxy_data : 1;

    template <class ResourceType, class ProxyType>
    ResourceSubtypeData(
        TypeWrapper<ResourceType>,
        TypeWrapper<ProxyType>,
        GpuBufferHolderBase* gpu_buffer_holder = nullptr,
        ResourceBinderBase* resource_binder = nullptr,
        WriteBufferDataFunction write_buffer_data_fn = &GpuBufferHolderBase::WriteBufferData_Static)
        : type_id(TypeId::ForType<ResourceType>()),
          has_proxy_data(false),
          proxy_ctor(nullptr),
          set_gpu_elem(nullptr),
          gpu_buffer_holder(gpu_buffer_holder),
          resource_binder(resource_binder),
          write_buffer_data_fn(write_buffer_data_fn)
    {
        // if ProxyType != NullProxy then we setup proxy pool
        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            has_proxy_data = true;

            proxy_allocator.class_size = sizeof(ProxyType);
            proxy_allocator.class_alignment = alignof(ProxyType);

            proxy_ctor = [](void* ptr) -> IRenderProxy*
            {
                return new (ptr) ProxyType;
            };

            set_gpu_elem = [](ResourceSubtypeData* _this, uint32 idx, IRenderProxy* proxy)
            {
                AssertDebug(idx != ~0u);

                ProxyType* proxy_casted = static_cast<ProxyType*>(proxy);

                _this->write_buffer_data_fn(_this->gpu_buffer_holder, idx, &proxy_casted->buffer_data, sizeof(ProxyType::buffer_data));
            };
        }
    }

    ResourceSubtypeData(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData& operator=(const ResourceSubtypeData& other) = delete;
    ResourceSubtypeData(ResourceSubtypeData&& other) noexcept = default;
    ResourceSubtypeData& operator=(ResourceSubtypeData&& other) noexcept = default;

    ~ResourceSubtypeData()
    {
        if (has_proxy_data)
        {
            for (IRenderProxy* proxy : proxies)
            {
                proxy->~IRenderProxy();

                proxy_allocator.Free(proxy);
            }

            proxies.Clear();
        }

        AssertDebug(proxies.Empty());
    }
};

HYP_ENABLE_OPTIMIZATION;

struct ResourceContainer
{
    ResourceSubtypeData& GetSubtypeData(TypeId type_id)
    {
        auto cache_it = cache.Find(type_id);
        if (cache_it != cache.End())
        {
            return *cache_it->second;
        }

        const HypClass* hyp_class = GetClass(type_id);
        AssertDebug(hyp_class != nullptr, "TypeId %u does not have a HypClass!", type_id.Value());

        int static_index = -1;

        do
        {
            static_index = hyp_class->GetStaticIndex();
            AssertDebugMsg(static_index >= 0, "Invalid class: '%s' has no assigned static index!", *hyp_class->GetName());

            if (ResourceSubtypeData* subtype_data = data_by_type.TryGet(static_index))
            {
                // found the subtype data for this type_id - cache it for O(1) retrieval next time
                cache[type_id] = subtype_data;

                return *subtype_data;
            }

            hyp_class = hyp_class->GetParent();
        }
        while (hyp_class);

        HYP_FAIL("No SubtypeData container found for TypeId %u (HypClass: %s)! Missing DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?", type_id.Value(), *GetClass(type_id)->GetName());
    }

    SparsePagedArray<ResourceSubtypeData, 16> data_by_type;
    HashMap<TypeId, ResourceSubtypeData*> cache;
};

struct ResourceContainerFactoryRegistry
{
    Array<Proc<void(ResourceBindings&, ResourceContainer&)>> funcs;

    static ResourceContainerFactoryRegistry& GetInstance()
    {
        static ResourceContainerFactoryRegistry instance;
        return instance;
    }

    void InvokeAll(ResourceBindings& resource_bindings, ResourceContainer& resource_container)
    {
        for (auto& func : funcs)
        {
            func(resource_bindings, resource_container);
        }
    }
};

enum DefaultCtor
{
    DEFAULT_CTOR
};

HYP_DISABLE_OPTIMIZATION;
template <class ResourceType, class ProxyType>
struct ResourceContainerFactory
{
    static constexpr TypeId type_id = TypeId::ForType<ResourceType>();

    static int GetStaticIndexOrFail()
    {
        const HypClass* hyp_class = GetClass(type_id);
        AssertDebugMsg(hyp_class != nullptr, "TypeId %u does not have a HypClass!", type_id.Value());

        const int static_index = hyp_class->GetStaticIndex();
        AssertDebugMsg(static_index >= 0, "Invalid class: '%s' has no assigned static index!", *hyp_class->GetName());

        return static_index;
    }

    ResourceContainerFactory()
    {
        ResourceContainerFactoryRegistry::GetInstance()
            .funcs.PushBack([](ResourceBindings& resource_bindings, ResourceContainer& container)
                {
                    const int static_index = GetStaticIndexOrFail();

                    if (!resource_bindings.subtype_bindings.HasIndex(static_index))
                    {
                        resource_bindings.subtype_bindings.Emplace(static_index);
                    }

                    AssertDebugMsg(!container.data_by_type.HasIndex(static_index),
                        "SubtypeData container already exists for TypeId %u (HypClass: %s)! Duplicate DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?",
                        type_id.Value(), *GetClass(type_id)->GetName());

                    container.data_by_type.Emplace(
                        static_index,
                        TypeWrapper<ResourceType>(),
                        TypeWrapper<ProxyType>());
                });
    }

    template <class ResourceBinderType>
    ResourceContainerFactory(
        GlobalRenderBuffer buf = GRB_INVALID,
        ResourceBinderType ResourceBindings::* mem_resource_binder = nullptr,
        WriteBufferDataFunction write_buffer_data_fn = &GpuBufferHolderBase::WriteBufferData_Static)
    {
        ResourceContainerFactoryRegistry::GetInstance()
            .funcs.PushBack([=](ResourceBindings& resource_bindings, ResourceContainer& container)
                {
                    const int static_index = GetStaticIndexOrFail();

                    if (!resource_bindings.subtype_bindings.HasIndex(static_index))
                    {
                        resource_bindings.subtype_bindings.Emplace(static_index);
                    }

                    AssertDebugMsg(!container.data_by_type.HasIndex(static_index),
                        "SubtypeData container already exists for TypeId %u (HypClass: %s)! Duplicate DECLARE_RENDER_DATA_CONTAINER() macro invocation for type?",
                        type_id.Value(), *GetClass(type_id)->GetName());

                    GpuBufferHolderBase* gpu_buffer_holder = nullptr;

                    if (buf != GRB_INVALID)
                    {
                        gpu_buffer_holder = g_render_global_state->gpu_buffers[buf];
                        AssertDebug(gpu_buffer_holder != nullptr, "GlobalRenderBuffer %u is not initialized");
                    }

                    ResourceBinderType* resource_binder = mem_resource_binder
                        ? &(resource_bindings.*mem_resource_binder)
                        : nullptr;

                    container.data_by_type.Emplace(
                        static_index,
                        TypeWrapper<ResourceType>(),
                        TypeWrapper<ProxyType>(),
                        gpu_buffer_holder,
                        resource_binder,
                        write_buffer_data_fn);
                });
    }
};

HYP_ENABLE_OPTIMIZATION;

#define DECLARE_RENDER_DATA_CONTAINER(resource_type, proxy_type, ...) \
    static ResourceContainerFactory<class resource_type, class proxy_type> g_##resource_type##_container_factory { __VA_ARGS__ };

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

HYP_API void RenderApi_InitResourceContainers()
{
    Threads::AssertOnThread(g_main_thread);

    ResourceContainerFactoryRegistry& registry = ResourceContainerFactoryRegistry::GetInstance();

    for (uint32 i = 0; i < num_frames; i++)
    {
        registry.InvokeAll(*g_render_global_state->resource_bindings, g_frame_data[i].resources);
    }

    registry.funcs.Clear();
}

HYP_API uint32 RenderApi_GetFrameIndex_RenderThread()
{
    return g_consumer_index.load(std::memory_order_relaxed);
}

HYP_API uint32 RenderApi_GetFrameIndex_GameThread()
{
    return g_producer_index.load(std::memory_order_relaxed);
}

HYP_API RenderProxyList& RenderApi_GetProducerProxyList(View* view)
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

HYP_API RenderProxyList& RenderApi_GetConsumerProxyList(View* view)
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

HYP_API void RenderApi_AddRef(HypObjectBase* resource)
{
    if (!resource)
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    const ObjIdBase resource_id = resource->Id();

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& fd = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = fd.resources.GetSubtypeData(resource_id.GetTypeId());
    ResourceData* rd = subtype_data.data.TryGet(resource_id.ToIndex());

    if (!rd)
    {
        rd = &*subtype_data.data.Emplace(resource_id.ToIndex(), resource);
    }

    rd->count.Increment(1, MemoryOrder::RELAXED);

    subtype_data.indices_pending_delete.Set(resource_id.ToIndex(), false);
}

HYP_API void RenderApi_ReleaseRef(ObjIdBase id)
{
    if (!id.IsValid())
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& fd = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = fd.resources.GetSubtypeData(id.GetTypeId());
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

HYP_API void RenderApi_UpdateRenderProxy(ObjIdBase id)
{
    if (!id.IsValid())
    {
        return;
    }

    Threads::AssertOnThread(g_game_thread);

    const uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& fd = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = fd.resources.GetSubtypeData(id.GetTypeId());
    ResourceData& data = subtype_data.data.Get(id.ToIndex());

    AssertDebug(data.count.Get(MemoryOrder::RELAXED), "expected ref count to be > 0 when calling UpdateRenderProxy()");
    AssertDebug(!subtype_data.indices_pending_delete.Test(id.ToIndex()), "Why is it marked for delete?");

    AssertDebug(subtype_data.has_proxy_data, "Cannot use UpdateResource() for type which does not have proxy data! TypeId: %u, HypClass %s",
        subtype_data.type_id.Value(), *GetClass(subtype_data.type_id)->GetName());

    IRenderProxy* proxy;

    if (IRenderProxy** p = subtype_data.proxies.TryGet(id.ToIndex()))
    {
        proxy = *p;
    }
    else
    {
        void* ptr = subtype_data.proxy_allocator.Alloc();
        AssertDebug(ptr != nullptr, "Failed to allocate render proxy!");

        // construct proxy object
        AssertDebug(subtype_data.proxy_ctor != nullptr);
        proxy = subtype_data.proxy_ctor(ptr);

        subtype_data.proxies.Emplace(id.ToIndex(), proxy);
    }

    AssertDebug(proxy);
    AssertDebug(data.resource);

    if (IsInstanceOfHypClass(Entity::Class(), data.resource->InstanceClass()))
    {
        Entity* entity = static_cast<Entity*>(data.resource);
        entity->UpdateRenderProxy(proxy);

        // mark for buffer data update from render thread
        subtype_data.indices_pending_update.Set(id.ToIndex(), true);
    }
    else
    {
        HYP_LOG(Rendering, Warning, "UpdateRenderProxy called for resource id {} of type {} which is not an Entity! Skipping proxy update.\n",
            id, GetClass(subtype_data.type_id)->GetName());
    }
}

HYP_API IRenderProxy* RenderApi_GetRenderProxy(ObjIdBase id)
{
    AssertDebug(id.IsValid());
    AssertDebug(id.GetTypeId() != TypeId::Void());

    Threads::AssertOnThread(g_render_thread);

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    ResourceSubtypeData& subtype_data = frame_data.resources.GetSubtypeData(id.GetTypeId());
    AssertDebugMsg(subtype_data.has_proxy_data, "Cannot use GetRenderProxy() for type which does not have a RenderProxy! TypeId: %u, HypClass %s",
        subtype_data.type_id.Value(), *GetClass(subtype_data.type_id)->GetName());

    if (!subtype_data.proxies.HasIndex(id.ToIndex()))
    {
        HYP_LOG(Rendering, Warning, "No render proxy found for resource: {} in frame {}", id, slot);

        return nullptr; // no proxy for this resource
    }

    IRenderProxy* proxy = subtype_data.proxies.Get(id.ToIndex());
    AssertDebug(proxy != nullptr);

    return proxy;
}

HYP_API void RenderApi_AssignResourceBinding(HypObjectBase* resource, uint32 binding)
{
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_render_thread);
#endif

    g_render_global_state->resource_bindings->Assign(resource, binding);
}

HYP_API uint32 RenderApi_RetrieveResourceBinding(const HypObjectBase* resource)
{
#ifdef HYP_DEBUG_MODE
    // FIXME: Add better check to ensure it is from a render task thread.
    Threads::AssertOnThread(g_render_thread | ThreadCategory::THREAD_CATEGORY_TASK);
#endif

    return g_render_global_state->resource_bindings->Retrieve(resource);
}

HYP_API void RenderApi_BeginFrame_GameThread()
{
    HYP_SCOPE;

    g_thread_frame_index = &g_producer_index;

    g_free_semaphore.acquire();

    uint32 slot = g_producer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;
        AssertDebug(vd.render_proxy_list.state != RenderProxyList::CS_READING);
        vd.render_proxy_list.state = RenderProxyList::CS_WRITING;
    }
}

HYP_API void RenderApi_EndFrame_GameThread()
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

static void RenderApi_UpdateBoundResources()
{
}

HYP_API void RenderApi_BeginFrame_RenderThread()
{
    HYP_SCOPE;
#ifdef HYP_DEBUG_MODE
    Threads::AssertOnThread(g_render_thread);
#endif

    g_thread_frame_index = &g_consumer_index;

    g_full_semaphore.acquire();

    uint32 slot = g_consumer_index.load(std::memory_order_relaxed);

    FrameData& frame_data = g_frame_data[slot];

    HYPERION_ASSERT_RESULT(RenderCommands::Flush());

    for (auto it = frame_data.per_view_data.Begin(); it != frame_data.per_view_data.End(); ++it)
    {
        ViewData& vd = *it;
        vd.render_proxy_list.state = RenderProxyList::CS_READING;
    }

    for (ResourceSubtypeData& subtype_data : frame_data.resources.data_by_type)
    {
        if (subtype_data.resource_binder)
        {
            for (ResourceData& elem : subtype_data.data)
            {
                AssertDebug(elem.resource != nullptr);
                subtype_data.resource_binder->Consider(elem.resource);
            }
        }
    }

    // assign the actual bindings:
    /// TODO: This should be done in the ResourceBinder itself, not here.
    g_render_global_state->resource_bindings->ambient_probe_binder.ApplyUpdates();
    g_render_global_state->resource_bindings->reflection_probe_binder.ApplyUpdates();
    g_render_global_state->resource_bindings->env_grid_binder.ApplyUpdates();
    g_render_global_state->resource_bindings->light_binder.ApplyUpdates();
    g_render_global_state->resource_bindings->lightmap_volume_binder.ApplyUpdates();

    for (ResourceSubtypeData& subtype_data : frame_data.resources.data_by_type)
    {
        if (subtype_data.indices_pending_update.Count() != 0)
        {
            AssertDebug(subtype_data.resource_binder != nullptr);

            // auto& subtype_bindings = g_render_global_state->resource_bindings->GetSubtypeBindings(subtype_data.type_id);

            // if (subtype_bindings.data.Empty())
            // {
            //     // early out; nothing is bound.
            //     continue;
            // }

            const Bitset& current_bound_indices = subtype_data.resource_binder->GetBoundIndices(subtype_data.type_id);

            if (current_bound_indices.Count() == 0)
            {
                // early out; nothing is bound.
                continue;
            }

            // Handle proxies that were updated on game thread
            for (Bitset::BitIndex i = subtype_data.indices_pending_update.FirstSetBitIndex();
                i != Bitset::not_found;
                i = subtype_data.indices_pending_update.NextSetBitIndex(i + 1))
            {
                // if the bit is not set, we failed to bind the resource to a slot and cannot update it
                // skip updating the data this time -- maybe next frame a slot will be freed
                // if (const uint32* p = subtype_bindings.data.TryGet(i); !p || *p == ~0u)
                // {
                //     continue;
                // }

                if (!current_bound_indices.Test(i))
                {
                    continue;
                }

                const ObjIdBase resource_id = ObjIdBase(subtype_data.type_id, uint32(i + 1));

                const uint32 bound_index = g_render_global_state->resource_bindings->Retrieve(resource_id);
                AssertDebug(bound_index != ~0u, "Failed to retrieve binding for resource: %u of type %s in frame %u!",
                    resource_id.Value(), *GetClass(resource_id.GetTypeId())->GetName(), slot);

                IRenderProxy* proxy = subtype_data.proxies.Get(i);
                AssertDebug(proxy != nullptr);

                AssertDebug(subtype_data.write_buffer_data_fn != nullptr, "write_buffer_data_fn is not set for type %s!",
                    *GetClass(subtype_data.type_id)->GetName());

                subtype_data.set_gpu_elem(&subtype_data, bound_index, proxy);
                subtype_data.indices_pending_update.Set(i, false);
            }
        }
    }
}

HYP_API void RenderApi_EndFrame_RenderThread()
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
            auto view_it = frame_data.views.FindIf([&vd](const auto& pair)
                {
                    return pair.second == &vd;
                });

            AssertDebug(view_it != frame_data.views.End());

            frame_data.views.Erase(view_it);

            it = frame_data.per_view_data.Erase(it);

            continue;
        }

        ++it;
    }

    for (ResourceSubtypeData& subtype_data : frame_data.resources.data_by_type)
    {
        if (subtype_data.resource_binder)
        {
            for (ResourceData& elem : subtype_data.data)
            {
                AssertDebug(elem.resource != nullptr);

                subtype_data.resource_binder->Deconsider(elem.resource);
            }
        }

        // @TODO: for deletion, have a fixed number to iterate over per frame so we don't spend too much time on it.
        // Remove resources pending deletion via SafeDelete() for indices marked for deletion from the game thread
        for (Bitset::BitIndex i : subtype_data.indices_pending_delete)
        {
            ResourceData& rd = subtype_data.data.Get(i);
            AssertDebug(rd.resource != nullptr);
            AssertDebug(rd.count.Get(MemoryOrder::RELAXED) == 0, "Ref count should be 0 before deletion");

            // if we delete it, we want to make sure it is not in marked for update state (don't want to iterate over dead items)
            subtype_data.indices_pending_update.Set(i, false);

            // Swap refcount owner over to the Handle
            AnyHandle resource { rd.resource };
            subtype_data.data.EraseAt(i);

            if (subtype_data.has_proxy_data)
            {
                AssertDebugMsg(subtype_data.proxies.HasIndex(i), "proxy missing at index: %u", i);

                IRenderProxy* proxy = subtype_data.proxies.Get(i);
                AssertDebug(proxy);

                subtype_data.proxies.EraseAt(i);

                // safely release the proxy's resources before we destruct it:
                proxy->SafeRelease();

                // pool doesnt call destructors so invoke manually
                proxy->~IRenderProxy();

#ifdef HYP_DEBUG_MODE
                // :)
                proxy->state = 0xDEAD;
#endif

                subtype_data.proxy_allocator.Free(proxy);
            }

            // safely release all the held resources:
            if (resource.IsValid())
            {
                g_safe_deleter->SafeRelease(std::move(resource));
            }
        }

        subtype_data.indices_pending_delete.Clear();
    }

    // Resource counters for binding allocators
    /// TODO: This should be done in the ResourceBinder itself, not here.
    g_render_global_state->resource_bindings->ambient_probe_bindings_allocator.ResetStat();
    g_render_global_state->resource_bindings->reflection_probe_bindings_allocator.ResetStat();
    g_render_global_state->resource_bindings->env_grid_bindings_allocator.ResetStat();
    g_render_global_state->resource_bindings->light_bindings_allocator.ResetStat();
    g_render_global_state->resource_bindings->lightmap_volume_bindings_allocator.ResetStat();

    g_consumer_index.store((slot + 1) % num_frames, std::memory_order_relaxed);

    g_free_semaphore.release();
}

#pragma region RenderGlobalState

RenderGlobalState::RenderGlobalState()
    : ShadowMapAllocator(MakeUnique<class ShadowMapAllocator>()),
      GpuBufferHolderMap(MakeUnique<class GpuBufferHolderMap>()),
      PlaceholderData(MakeUnique<class PlaceholderData>()),
      resource_bindings(new ResourceBindings)
{
    gpu_buffers.buffers[GRB_WORLDS] = GpuBufferHolderMap->GetOrCreate<WorldShaderData, GpuBufferType::CBUFF>();
    gpu_buffers.buffers[GRB_CAMERAS] = GpuBufferHolderMap->GetOrCreate<CameraShaderData, GpuBufferType::CBUFF>();
    gpu_buffers.buffers[GRB_LIGHTS] = GpuBufferHolderMap->GetOrCreate<LightShaderData, GpuBufferType::SSBO>();
    gpu_buffers.buffers[GRB_ENTITIES] = GpuBufferHolderMap->GetOrCreate<EntityShaderData, GpuBufferType::SSBO>();
    gpu_buffers.buffers[GRB_MATERIALS] = GpuBufferHolderMap->GetOrCreate<MaterialShaderData, GpuBufferType::SSBO>();
    gpu_buffers.buffers[GRB_SKELETONS] = GpuBufferHolderMap->GetOrCreate<SkeletonShaderData, GpuBufferType::SSBO>();
    gpu_buffers.buffers[GRB_SHADOW_MAPS] = GpuBufferHolderMap->GetOrCreate<ShadowMapShaderData, GpuBufferType::SSBO>();
    gpu_buffers.buffers[GRB_ENV_PROBES] = GpuBufferHolderMap->GetOrCreate<EnvProbeShaderData, GpuBufferType::SSBO>();
    gpu_buffers.buffers[GRB_ENV_GRIDS] = GpuBufferHolderMap->GetOrCreate<EnvGridShaderData, GpuBufferType::CBUFF>();
    gpu_buffers.buffers[GRB_LIGHTMAP_VOLUMES] = GpuBufferHolderMap->GetOrCreate<LightmapVolumeShaderData, GpuBufferType::SSBO>();

    GlobalDescriptorTable = g_render_backend->MakeDescriptorTable(&GetStaticDescriptorTableDeclaration());

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

    EnvGridRenderer = new class EnvGridRenderer;
}

RenderGlobalState::~RenderGlobalState()
{
    delete resource_bindings;

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

    delete EnvGridRenderer;

    Renderer->Shutdown();
    delete Renderer;
    Renderer = nullptr;
}

void RenderGlobalState::UpdateBuffers(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (auto& it : GpuBufferHolderMap->GetItems())
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

    GpuBufferRef blue_noise_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(BlueNoiseBuffer));
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

    GpuBufferRef sphere_samples_buffer = g_render_backend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(Vec4f) * 4096);
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
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("WorldsBuffer"), gpu_buffers[GRB_WORLDS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightsBuffer"), gpu_buffers[GRB_LIGHTS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentLight"), gpu_buffers[GRB_LIGHTS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ObjectsBuffer"), gpu_buffers[GRB_ENTITIES]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CamerasBuffer"), gpu_buffers[GRB_CAMERAS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvGridsBuffer"), gpu_buffers[GRB_ENV_GRIDS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbesBuffer"), gpu_buffers[GRB_ENV_PROBES]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("CurrentEnvProbe"), gpu_buffers[GRB_ENV_PROBES]->GetBuffer(frame_index));

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("VoxelGridTexture"), PlaceholderData->GetImageView3D1x1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldColorTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightFieldDepthTexture"), PlaceholderData->GetImageView2D1x1R8());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("BlueNoiseBuffer"), GpuBufferRef::Null());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("SphereSamplesBuffer"), GpuBufferRef::Null());

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsTextureArray"), PlaceholderData->GetImageView2D1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("PointLightShadowMapsTextureArray"), PlaceholderData->GetImageViewCube1x1R8Array());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("ShadowMapsBuffer"), gpu_buffers[GRB_SHADOW_MAPS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("LightmapVolumesBuffer"), gpu_buffers[GRB_LIGHTMAP_VOLUMES]->GetBuffer(frame_index));

    for (uint32 i = 0; i < max_bound_reflection_probes; i++)
    {
        GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), i, PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView());
    }

    GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("DDGIUniforms"), PlaceholderData->GetOrCreateBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms), true /* exact size */));
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
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("CurrentObject"), gpu_buffers[GRB_ENTITIES]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("MaterialsBuffer"), gpu_buffers[GRB_MATERIALS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("SkeletonsBuffer"), gpu_buffers[GRB_SKELETONS]->GetBuffer(frame_index));
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeIrradianceTexture"), PlaceholderData->GetImageView2D1x1R8());
    GlobalDescriptorTable->GetDescriptorSet(NAME("Object"), frame_index)->SetElement(NAME("LightmapVolumeRadianceTexture"), PlaceholderData->GetImageView2D1x1R8());

    // Material
    if (g_render_backend->GetRenderConfig().IsBindlessSupported())
    {
        for (uint32 texture_index = 0; texture_index < max_bindless_resources; texture_index++)
        {
            GlobalDescriptorTable->GetDescriptorSet(NAME("Material"), frame_index)
                ->SetElement(NAME("Textures"), texture_index, PlaceholderData->GetImageView2D1x1R8());
        }
    }
}

#pragma endregion RenderGlobalState

DECLARE_RENDER_DATA_CONTAINER(Entity, NullProxy);
DECLARE_RENDER_DATA_CONTAINER(EnvGrid, RenderProxyEnvGrid, GRB_ENV_GRIDS, &ResourceBindings::env_grid_binder);
DECLARE_RENDER_DATA_CONTAINER(ReflectionProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::reflection_probe_binder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(SkyProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::reflection_probe_binder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(EnvProbe, RenderProxyEnvProbe, GRB_ENV_PROBES, &ResourceBindings::ambient_probe_binder, &WriteBufferData_EnvProbe);
DECLARE_RENDER_DATA_CONTAINER(Light, RenderProxyLight, GRB_LIGHTS, &ResourceBindings::light_binder);
DECLARE_RENDER_DATA_CONTAINER(LightmapVolume, RenderProxyLightmapVolume, GRB_LIGHTMAP_VOLUMES, &ResourceBindings::lightmap_volume_binder);

} // namespace hyperion

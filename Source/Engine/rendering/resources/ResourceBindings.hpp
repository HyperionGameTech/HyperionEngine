#pragma once

#include <Core/Defines.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/Class.hpp>

#include <Core/containers/SparsePagedArray.hpp>
#include <Core/containers/StridedBuffer.hpp>

#include <Core/threading/Thread.hpp>

namespace Hyperion {

class StructuredBuffer;

namespace Resources {

#pragma region ResourceBindings

#include <rendering/resources/ResourceBindings.inl>

struct SubtypeResourceBindings
{
    const Class* resourceClass;
    StructuredBuffer* sbuffer;
    SparsePagedArray<uint32, 1024, RenderAllocator> bindingIndices;

    SubtypeResourceBindings(const Class* resourceClass, StructuredBuffer* sbuffer)
        : resourceClass(resourceClass),
          sbuffer(sbuffer)
    {
        AssertDebug(resourceClass != nullptr);
    }
};

static SparsePagedArray<SubtypeResourceBindings, 64> s_subtypeBindings;

void ClearSubtypeBindings()
{
    s_subtypeBindings.Clear(/* freeMemory */ true);
}

static inline SubtypeResourceBindings& GetSubtypeBindings(const Class* cls)
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

void SetBinding(ObjectBase* resource, uint32 binding)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(resource != nullptr);

    SubtypeResourceBindings& bindings = GetSubtypeBindings(resource->InstanceClass());

    ObjIdBase resourceId = resource->Id();
    AssertDebug(resourceId.IsValid());

    if (binding == UINT32_MAX)
    {
        bindings.bindingIndices.EraseAt(resourceId.ToIndex());

        return;
    }

    bindings.bindingIndices.Emplace(resourceId.ToIndex(), binding);
}

uint32 GetBinding(const ObjectBase* resource)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!resource)
    {
        return UINT32_MAX; // invalid resource
    }

    const SubtypeResourceBindings& bindings = GetSubtypeBindings(resource->InstanceClass());

    const ObjIdBase resourceId = resource->Id();

    const uint32* elem = bindings.bindingIndices.TryGet(resourceId.ToIndex());

    return elem ? *elem : UINT32_MAX;
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

    StructuredBuffer* sbuffer;

    WriteBufferDataFunction writeBufferDataFn;

    // == optional render proxy data ==
    StridedBuffer<RenderAllocator> proxies;
    void (*proxyDtor)(void*);
    bool hasProxyData : 1;

    template <class ResourceType, class ProxyType, size_t NumResourceBinders>
    ResourceSubtypeData(
        TypeWrapper<ResourceType>,
        TypeWrapper<ProxyType>,
        StructuredBuffer* sbuffer = nullptr,
        FixedArray<ResourceBinderBase*, NumResourceBinders> resourceBinders = {},
        WriteBufferDataFunction writeBufferDataFn = nullptr)
        : typeInfo(&TypeInfo::ForType<ResourceType>()),
          proxies(sizeof(ProxyType), alignof(ProxyType), /* blocksPerSlab */ 256),
          proxyDtor(&Memory::Destruct<ProxyType>),
          hasProxyData(false),
          sbuffer(sbuffer),
          resourceBinders { nullptr },
          writeBufferDataFn(writeBufferDataFn)
    {
        static_assert(NumResourceBinders <= MaxResourceBindersPerType,
            "Number of resource binders exceeds MaxResourceBindersPerType!");

        // copy resource binders
        for (size_t i = 0; i < NumResourceBinders; i++)
        {
            this->resourceBinders[i] = resourceBinders[i];

            if (resourceBinders[i])
            {
                resourceBinders[i]->Initialize();
            }
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

    ResourceSubtypeData(ResourceSubtypeData&& other) noexcept = delete;
    ResourceSubtypeData& operator=(ResourceSubtypeData&& other) noexcept = delete;

    ~ResourceSubtypeData()
    {
        proxies.Clear(proxyDtor);
    }

    HYP_FORCE_INLINE void SetGpuElem(uint32 idx, IRenderProxy* proxy)
    {
        AssertDebug(writeBufferDataFn != nullptr && sbuffer != nullptr && idx != UINT32_MAX);

        writeBufferDataFn(*sbuffer, idx, proxy);
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
        uint8 bufferId,
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

                StructuredBuffer* sbuffer = bufferId != NamedBuffer::Invalid
                    ? &g_renderInterface->namedBuffers[bufferId]
                    : nullptr;

                if (!s_subtypeBindings.HasIndex(staticIndex))
                {
                    // add new ResourceSubtypeBindings slot for the given class
                    s_subtypeBindings.Emplace(staticIndex, resourceClass, sbuffer);
                }

                AssertDebug(!container.dataByType.HasIndex(staticIndex),
                    "ResourceSubtypeData for resource class '{}' has already been registered!",
                    *resourceClass->GetName());

                container.dataByType.Emplace(
                    staticIndex,
                    TypeWrapper<ResourceType>(),
                    TypeWrapper<ProxyType>(),
                    sbuffer,
                    FixedArray<ResourceBinderBase*, sizeof...(ResourceBinderTypes)> { static_cast<ResourceBinderBase*>(resourceBinders)... },
                    writeBufferDataFn);

                HYP_LOG(Rendering, Verbose, "Registered resource container for resource class '{}'",
                    *resourceClass->GetName());
            });
    }
};

#define DECLARE_RENDER_DATA_CONTAINER(ResourceType, ProxyType, ...)                                           \
    static ResourceContainerFactory<class ResourceType, class ProxyType> g_##ResourceType##ContainerFactory { \
        __VA_ARGS__                                                                                           \
    };

#pragma endregion ResourceContainer

template <class ElementType, class ProxyType>
static HYP_FORCE_INLINE void CopyRenderProxy(ResourceSubtypeData& subtypeData, const ObjId<ElementType>& id, ProxyType* newProxy)
{
    AssertDebug(newProxy != nullptr);

    const uint32 idx = id.ToIndex();

    AssertDebug(subtypeData.typeInfo->id == id.GetTypeId(),
        "Attempting to use ID for type {} as index into proxy collection that requires index type {}",
        LookupTypeName(id.GetTypeId()),
        subtypeData.typeInfo->name);

    subtypeData.proxies.SetElement(idx, *newProxy);
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
    ResourceContainer& resources,
    ResourceTracker<AllocatorType, ObjId<ElementType>, ElementType*, ProxyType>& dst,
    const ResourceTracker<AllocatorType, ObjId<ElementType>, ElementType*, ProxyType>& src)
{
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

    Array<ElementType*, RenderAllocator> removed;
    dst.GetRemoved(removed, false);

    Array<ElementType*, RenderAllocator> added;
    dst.GetAdded(added, false);

    for (ElementType* pResource : added)
    {
        AssertDebug(pResource != nullptr);

        const ObjId<ElementType> resourceId = pResource->Id();
        AssertDebug(resourceId.IsValid());

        ResourceSubtypeData& subtypeData = resources.GetSubtypeData(pResource->InstanceClass());
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

        ResourceSubtypeData& subtypeData = resources.GetSubtypeData(pResource->InstanceClass());
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

    Array<ElementType*, RenderAllocator> changed;

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

                ResourceSubtypeData& subtypeData = resources.GetSubtypeData(pResource->InstanceClass());

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

template <class AllocatorType, size_t... Indices>
static inline void SyncResourcesT(
    ResourceContainer& resources,
    ResourceTrackerBase<AllocatorType>** dstResourceTrackers,
    ResourceTrackerBase<AllocatorType>** srcResourceTrackers,
    std::index_sequence<Indices...>)
{
    (SyncResources(
         resources,
         static_cast<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type&>(*dstResourceTrackers[Indices]),
         static_cast<const typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type&>(*srcResourceTrackers[Indices])),
        ...);
}

static inline void CopyDependencies(
    ResourceContainer& resources,
    RenderProxyList& dst,
    RenderProxyList& src)
{
    AssertDebug(dst.resourceTrackers.Size() == TupleSize<RenderProxyList::ResourceTrackerTypes>::value);
    AssertDebug(src.resourceTrackers.Size() == TupleSize<RenderProxyList::ResourceTrackerTypes>::value);

    // Copy src -> dst
    SyncResourcesT(
        resources,
        dst.resourceTrackers.Data(),
        src.resourceTrackers.Data(),
        std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());

    if (src.useOrdering)
    {
        dst.meshEntityOrdering = src.meshEntityOrdering;
    }
}

} // namespace Resources

} // namespace Hyperion
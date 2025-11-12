/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/Scene.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <scene/camera/Camera.hpp>

#include <scene/animation/Skeleton.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/Util.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>
#include <engine/EngineMemory.hpp>

namespace hyperion {

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

static constexpr uint32 AllBucketsMask = (1u << RB_MAX) - 1;

// DrawCallCollectionMapping::~DrawCallCollectionMapping()
//{
//     Threads::AssertOnThread(g_renderThread);
// }

#pragma region ParallelRenderingState

// Holds shared data for ParallelRenderingState instances to reduce memory usage
struct ParallelRenderingState_Shared
{
    static constexpr uint32 MaxBatches = ParallelRenderingState::MaxBatches;
    static constexpr SizeType LocalQueueArenaSize = 1 * 1024 * 1024;

    using LocalQueue = ParallelRenderingState::LocalQueue;

    FixedArray<TArena<RenderAllocator>*, MaxBatches> arenas;
    FixedArray<LocalQueue*, MaxBatches> localQueues;

    ParallelRenderingState_Shared()
        : arenas {},
          localQueues {}
    {
        Threads::AssertOnThread(g_renderThread);

        for (uint32 i = 0; i < MaxBatches; i++)
        {
            TArena<RenderAllocator>* arena = PoolNew<TArena<RenderAllocator>>(*g_renderPool, LocalQueueArenaSize);
            arenas[i] = arena;

            localQueues[i] = PoolNew<LocalQueue>(*g_renderPool, arena);
        }
    }

    ~ParallelRenderingState_Shared()
    {
        Threads::AssertOnThread(g_renderThread);

        for (uint32 i = 0; i < MaxBatches; i++)
        {
            if (localQueues[i])
            {
                PoolDelete(*g_renderPool, localQueues[i]);
            }

            if (arenas[i])
            {
                PoolDelete(*g_renderPool, arenas[i]);
            }
        }
    }
};

ParallelRenderingState::ParallelRenderingState(ParallelRenderingState_Shared* sharedData)
    : sharedData(sharedData)
{
    Assert(sharedData != nullptr);

    for (uint32 i = 0; i < MaxBatches; i++)
    {
        localQueues[i] = sharedData->localQueues[i];
    }
}

ParallelRenderingState::~ParallelRenderingState()
{
    if (sharedData)
    {
        PoolDelete(*g_renderPool, sharedData);
    }
}

#pragma endregion ParallelRenderingState

#pragma region RenderProxyList

namespace GeometryPass {

namespace PropNames {

static const Name s_nameInstancing = NAME("INSTANCING");
static const Name s_nameShadingType = NAME("SHADING_TYPE");
static const Name s_nameDeferred = NAME("DEFERRED");
static const Name s_nameForward = NAME("FORWARD");
static const Name s_nameLightmapped = NAME("LIGHTMAPPED");
static const Name s_nameAlphaDiscard = NAME("ALPHA_DISCARD");
static const Name s_nameSkinning = NAME("SKINNING");

static const Name s_nameHasAlbedoMap = NAME("HAS_ALBEDO_MAP");
static const Name s_nameHasNormalMap = NAME("HAS_NORMAL_MAP");
static const Name s_nameHasParallaxMap = NAME("HAS_PARALLAX_MAP");
static const Name s_nameHasMetalnessMap = NAME("HAS_METALNESS_MAP");
static const Name s_nameHasRoughnessMap = NAME("HAS_ROUGHNESS_MAP");
static const Name s_nameHasAoMap = NAME("HAS_AO_MAP");

} // namespace PropNames

constexpr uint8 LightmapStencilMask = (1u << LightmapVolume::MaxAtlases) - 1;

// Get the stencil reference value to set for a lightmapped object,
// based on its associated atlas index.
static constexpr inline uint8 GetLightmapStencilValue(LightmapElement::Id lightmapElementId)
{
    if (lightmapElementId == ~0u)
    {
        return 0; // invalid element
    }

    uint16 atlasIndex = 0;
    uint16 elementIndex = 0;
    LightmapElement::GetAtlasAndElementIndex(lightmapElementId, atlasIndex, elementIndex);

    uint8 value = 0;
    value |= ((atlasIndex + 1) & LightmapStencilMask);

    return value;
}

/// Set attributes, used to decide what shader variant + pipeline to use for rendering the given proxy.
static void BuildAttributes(const RenderProxyMesh& proxy, RenderableAttributeSet& attributes, const RenderableAttributeSet* overrideAttributes = nullptr)
{
    HYP_SCOPE;

    Mesh* mesh = proxy.mesh;
    AssertDebug(mesh != nullptr);

    Material* material = proxy.material;
    AssertDebug(material != nullptr);

    // shading mode
    static const ShaderProperty s_propShadingTypeDeferredDeferred = ShaderProperty(Name(PropNames::s_nameShadingType), Name(PropNames::s_nameDeferred));
    static const ShaderProperty s_propShadingTypeForward = ShaderProperty(Name(PropNames::s_nameShadingType), Name(PropNames::s_nameForward));
    static const ShaderProperty s_propShadingTypeLightmapped = ShaderProperty(Name(PropNames::s_nameShadingType), Name(PropNames::s_nameLightmapped));

    // textures
    static const ShaderProperty s_propHasAlbedoMap = ShaderProperty(PropNames::s_nameHasAlbedoMap);
    static const ShaderProperty s_propHasNormalMap = ShaderProperty(PropNames::s_nameHasNormalMap);
    static const ShaderProperty s_propHasParallaxMap = ShaderProperty(PropNames::s_nameHasParallaxMap);
    static const ShaderProperty s_propHasMetalnessMap = ShaderProperty(PropNames::s_nameHasMetalnessMap);
    static const ShaderProperty s_propHasRoughnessMap = ShaderProperty(PropNames::s_nameHasRoughnessMap);
    static const ShaderProperty s_propHasAoMap = ShaderProperty(PropNames::s_nameHasAoMap);

    static const Pair<MaterialTextureKey, ShaderProperty> s_textureProperties[] = {
        { MaterialTextureKey::ALBEDO_MAP, ShaderProperty(PropNames::s_nameHasAlbedoMap) },
        { MaterialTextureKey::NORMAL_MAP, ShaderProperty(PropNames::s_nameHasNormalMap) },
        { MaterialTextureKey::PARALLAX_MAP, ShaderProperty(PropNames::s_nameHasParallaxMap) },
        { MaterialTextureKey::METALNESS_MAP, ShaderProperty(PropNames::s_nameHasMetalnessMap) },
        { MaterialTextureKey::ROUGHNESS_MAP, ShaderProperty(PropNames::s_nameHasRoughnessMap) },
        { MaterialTextureKey::AO_MAP, ShaderProperty(PropNames::s_nameHasAoMap) }
    };

    // set base attributes from mesh and material
    attributes = RenderableAttributeSet { mesh->GetMeshAttributes(), material->GetRenderAttributes() };

    if (overrideAttributes)
    {
        if (const ShaderDefinition& overrideShaderDefinition = overrideAttributes->GetShaderDefinition())
        {
            attributes.SetShaderDefinition(overrideShaderDefinition);
        }

        const ShaderDefinition& shaderDefinition = overrideAttributes->GetShaderDefinition().IsValid()
            ? overrideAttributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet meshVertexAttributes = attributes.GetMeshAttributes().vertexAttributes;

        MaterialAttributes newMaterialAttributes = overrideAttributes->GetMaterialAttributes();
        newMaterialAttributes.shaderDefinition = shaderDefinition;

        if (meshVertexAttributes != newMaterialAttributes.shaderDefinition.GetProperties().GetRequiredVertexAttributes())
        {
            newMaterialAttributes.shaderDefinition.properties.SetRequiredVertexAttributes(meshVertexAttributes);
        }

        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(newMaterialAttributes);
    }

    const bool hasInstancing = proxy.instanceData.enableAutoInstancing || proxy.instanceData.numInstances > 1;
    const bool hasForwardLighting = attributes.GetMaterialAttributes().bucket == RB_TRANSLUCENT;
    const bool hasLightmaps = attributes.GetMaterialAttributes().bucket == RB_LIGHTMAP;
    const bool hasDeferredLighting = !hasForwardLighting && !hasLightmaps;
    const bool hasAlphaDiscard = bool(attributes.GetMaterialAttributes().flags & MAF_ALPHA_DISCARD);
    const bool hasSkinning = proxy.skeleton != nullptr && proxy.skeleton->GetRootBone() != nullptr;

    // if lightmap volume is set we need stencil testing
    if (hasLightmaps)
    {
        const uint8 stencilReferenceValue = GetLightmapStencilValue(proxy.lightmapElementId) & LightmapStencilMask;

        if (stencilReferenceValue != (attributes.GetMaterialAttributes().stencilReference & LightmapStencilMask))
        {
            attributes.GetMaterialAttributes().stencilReference &= ~LightmapStencilMask;
            attributes.GetMaterialAttributes().stencilReference |= stencilReferenceValue;
            attributes.Invalidate();
        }
    }
    else if (attributes.GetMaterialAttributes().stencilReference & LightmapStencilMask)
    {
        attributes.GetMaterialAttributes().stencilReference &= ~LightmapStencilMask;
        attributes.Invalidate();
    }

    const ShaderDefinition& currentShaderDefinition = attributes.GetShaderDefinition();
    const ShaderProperties& currentProperties = currentShaderDefinition.GetProperties();

    ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();
    bool shaderDefinitionChanged = false;

    if (hasInstancing != shaderDefinition.GetProperties().Has(PropNames::s_nameInstancing))
    {
        shaderDefinition.GetProperties().Set(PropNames::s_nameInstancing, hasInstancing);
        shaderDefinitionChanged = true;
    }

    {
        auto shadingTypeIt = currentProperties.Find(PropNames::s_nameShadingType);

        if (hasDeferredLighting != (shadingTypeIt != currentProperties.End() && shadingTypeIt->cachedHashCode.value == PropNames::s_nameDeferred.hashCode))
        {
            shaderDefinition.GetProperties().Set(s_propShadingTypeDeferredDeferred, hasDeferredLighting);
            shaderDefinitionChanged = true;
        }

        if (hasForwardLighting != (shadingTypeIt != currentProperties.End() && shadingTypeIt->cachedHashCode.value == PropNames::s_nameForward.hashCode))
        {
            shaderDefinition.GetProperties().Set(s_propShadingTypeForward, hasForwardLighting);
            shaderDefinitionChanged = true;
        }

        if (hasLightmaps != (shadingTypeIt != currentProperties.End() && shadingTypeIt->cachedHashCode.value == PropNames::s_nameLightmapped.hashCode))
        {
            shaderDefinition.GetProperties().Set(s_propShadingTypeLightmapped, hasLightmaps);
            shaderDefinitionChanged = true;
        }
    }

    if (hasAlphaDiscard != currentProperties.Has(PropNames::s_nameAlphaDiscard))
    {
        shaderDefinition.GetProperties().Set(PropNames::s_nameAlphaDiscard, hasAlphaDiscard);
        shaderDefinitionChanged = true;
    }

    if (hasSkinning != currentProperties.Has(PropNames::s_nameSkinning))
    {
        shaderDefinition.GetProperties().Set(PropNames::s_nameSkinning, hasSkinning);
        shaderDefinitionChanged = true;
    }

    // update shader properties to reflect texture presence based on texture mask
    for (const auto& [textureKey, property] : s_textureProperties)
    {
        const bool presence = bool(attributes.GetMaterialAttributes().textureMask & uint32(textureKey));

        if (presence != currentProperties.Has(property))
        {
            shaderDefinition.GetProperties().Set(property, presence);
            shaderDefinitionChanged = true;
        }
    }

    if (shaderDefinitionChanged)
    {
        // Update the shader definition in the attributes
        attributes.SetShaderDefinition(shaderDefinition);
    }
}

} // namespace GeometryPass

static Handle<RenderGroup> CreateRenderGroup(RenderCollector* renderCollector, DrawCallCollectionMapping& mapping, const RenderableAttributeSet& attributes)
{
    EnumFlags<RenderGroupFlags> renderGroupFlags = RenderGroupFlags::DEFAULT;

    // Disable occlusion culling for translucent objects
    const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

    if (rb == RB_TRANSLUCENT || rb == RB_DEBUG)
    {
        renderGroupFlags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
    }

    ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();

    ShaderRef shader = g_shaderManager->GetOrCreate(shaderDefinition);

    if (!shader.IsValid())
    {
        HYP_LOG(Rendering, Error, "Failed to create shader for RenderProxy");

        return Handle<RenderGroup>::empty;
    }

    // Create RenderGroup
    Handle<RenderGroup> rg = CreateObject<RenderGroup>(shader, attributes, renderGroupFlags);

    if (renderGroupFlags & RenderGroupFlags::INDIRECT_RENDERING)
    {
        AssertDebug(mapping.indirectRenderer == nullptr, "Indirect renderer already exists on mapping");

        mapping.indirectRenderer = PoolNew<IndirectRenderer>(*g_renderPool);
        mapping.indirectRenderer->Create(renderCollector->batchAllocator);
    }

    mapping.drawCallCollection.batchAllocator = renderCollector->batchAllocator;

    InitObject(rg);

    return rg;
}

template <class AllocatorType, class Functor, SizeType... Indices>
static inline void ForEachResourceTrackerType_Impl(Span<ResourceTrackerBase<AllocatorType>*> resourceTrackers, const Functor& functor, std::index_sequence<Indices...>)
{
    (functor(TypeWrapper<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type>(), resourceTrackers[Indices], Indices), ...);
}

template <class AllocatorType, class Functor>
static inline void ForEachResourceTrackerType(Span<ResourceTrackerBase<AllocatorType>*> resourceTrackers, const Functor& functor)
{
    ForEachResourceTrackerType_Impl(resourceTrackers, functor, std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());
}

template <class T>
static constexpr SizeType GetTrackedResourceTypeIndex()
{
    return FindTypeElementIndex<T, RenderProxyList::TrackedResourceTypes>::value;
}

template <class AllocatorType, class Functor, SizeType... Indices>
static inline void ForEachResourceTracker_Impl(Span<ResourceTrackerBase<AllocatorType>*> resourceTrackers, const Functor& functor, std::index_sequence<Indices...>)
{
    (functor(static_cast<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type&>(*resourceTrackers[Indices])), ...);
}

template <class AllocatorType, class Functor>
static inline void ForEachResourceTracker(Span<ResourceTrackerBase<AllocatorType>*> resourceTrackers, const Functor& functor)
{
    ForEachResourceTracker_Impl(resourceTrackers, functor, std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());
}

template <class AllocatorType, class ElementType, class ProxyType>
static inline void UpdateRefs_Impl(ResourceTracker<AllocatorType, ObjId<ElementType>, ElementType*, ProxyType>& resourceTracker)
{
    auto diff = resourceTracker.GetDiff();
    if (!diff.NeedsUpdate())
    {
        return;
    }

    Array<ElementType*> removed;
    resourceTracker.GetRemoved(removed, false);

    Array<ElementType*> added;
    resourceTracker.GetAdded(added, false);

    for (ElementType* resource : added)
    {
        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            ProxyType* pProxy = resourceTracker.GetProxy(ObjId<ElementType>(resource->Id()));

            if (!pProxy)
            {
                pProxy = resourceTracker.SetProxy(ObjId<ElementType>(resource->Id()), ProxyType());
            }

            AssertDebug(pProxy != nullptr);

            if constexpr (HYP_HAS_METHOD(ElementType, UpdateRenderProxy))
            {
                resource->UpdateRenderProxy(pProxy);
            }
        }
    }

    for (ElementType* resource : removed)
    {
        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            resourceTracker.RemoveProxy(ObjId<ElementType>(resource->Id()));
        }
    }

    if constexpr (!std::is_same_v<ProxyType, NullProxy> && HYP_HAS_METHOD(ElementType, UpdateRenderProxy))
    {
        Array<ObjId<ElementType>> changedIds;
        resourceTracker.GetChanged(changedIds);

        for (const ObjId<ElementType>& id : changedIds)
        {
            ElementType** ppResource = resourceTracker.GetElement(id);
            AssertDebug(ppResource && *ppResource);

            ElementType& resource = **ppResource;

            ProxyType* pProxy = resourceTracker.GetProxy(id);
            AssertDebug(pProxy != nullptr);

            resource.UpdateRenderProxy(pProxy);
        }
    }
}

// template hackery to allow usage of undefined types
template <class T>
static inline void UpdateRefs(T& renderProxyList)
{
    AssertDebug(renderProxyList.useRefCounting);

    ForEachResourceTracker(renderProxyList.resourceTrackers.ToSpan(), []<class... Args>(Args&&... args)
        {
            UpdateRefs_Impl(std::forward<Args>(args)...);
        });
}

RenderProxyList::RenderProxyList(AllocatorType* pAllocator, bool isShared, bool useRefCounting)
    : isShared(isShared),
      useRefCounting(useRefCounting),
      viewport(Viewport { Vec2u::One(), Vec2i::Zero() }),
      priority(0),
      resourceTrackers {},
      releaseRefsFunctions {}
{
    AssertDebug(pAllocator != nullptr);

    // initialize the resource trackers
    ForEachResourceTrackerType(resourceTrackers.ToSpan(), [this, pAllocator]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>, ResourceTrackerBase<AllocatorType>*& pResourceTracker, SizeType idx)
        {
            AssertDebug(!pResourceTracker);

            pResourceTracker = new ResourceTrackerType(pAllocator);
        });
}

RenderProxyList::~RenderProxyList()
{
#ifdef HYP_DEBUG_MODE
    debugIsDestroyed = true;
#endif

#ifdef HYP_DEBUG_MODE
    int numRenderProxies = 0;

    ForEachResourceTracker(resourceTrackers.ToSpan(), [&](auto&& resourceTracker)
        {
            for (Bitset::BitIndex bit : resourceTracker.GetSubclassIndices())
            {
                auto&& impl = resourceTracker.GetSubclassImpl(int(bit));
                numRenderProxies += impl.proxies.Count();
            }
        });

    if (numRenderProxies > 0)
        HYP_LOG(Rendering, Debug, "RenderProxyList destroyed with {} render proxies still in it", numRenderProxies);
#endif

    for (SizeType i = 0; i < resourceTrackers.Size(); i++)
    {
        ResourceTrackerBase<AllocatorType>* resourceTracker = resourceTrackers[i];

        delete resourceTracker;
    }

    resourceTrackers = {};
}

void RenderProxyList::BeginWrite()
{
    if (isShared)
    {
        uint64 rwMarkerState = rwMarker.BitOr(WriteFlag, MemoryOrder::ACQUIRE);
        while (HYP_UNLIKELY(rwMarkerState & ReadMask))
        {
            HYP_LOG_TEMP("Busy waiting for read marker to be released! "
                         "If this is occuring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

            rwMarkerState = rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }
    }

    AssertDebug(state != CS_READING);
    state = CS_WRITING;

    // advance all trackers to the next state before we write into them.
    // this clears their 'next' bits and sets their 'previous' bits so we can tell what changed.
    ForEachResourceTracker(resourceTrackers.ToSpan(), [](auto&& resourceTracker)
        {
            resourceTracker.Advance();
        });
}

void RenderProxyList::EndWrite()
{
    AssertDebug(state == CS_WRITING);

    if (useRefCounting)
    {
        UpdateRefs(*this);
    }

    state = CS_WRITTEN;

    if (isShared)
    {
        rwMarker.BitAnd(~WriteFlag, MemoryOrder::RELEASE);
    }
}

void RenderProxyList::BeginRead()
{
    if (isShared)
    {
        uint64 rwMarkerState;

        do
        {
            rwMarkerState = rwMarker.Increment(2, MemoryOrder::ACQUIRE);

            if (HYP_UNLIKELY(rwMarkerState & WriteFlag))
            {
                HYP_LOG_TEMP("Waiting for write marker to be released. "
                             "If this is occurring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

                rwMarker.Decrement(2, MemoryOrder::RELAXED);

                // spin to wait for write flag to be released
                HYP_WAIT_IDLE();
            }
        }
        while (HYP_UNLIKELY(rwMarkerState & WriteFlag));
    }
    else
    {
        ++readDepth;
    }

    AssertDebug(state != CS_WRITING);
    state = CS_READING;
}

void RenderProxyList::EndRead()
{
    AssertDebug(state == CS_READING);

    if (isShared)
    {
        uint64 rwMarkerState = rwMarker.Decrement(2, MemoryOrder::ACQUIRE_RELEASE);
        AssertDebug(rwMarkerState & ReadMask, "Invalid state, expected read mask to be set when calling EndRead()");

        /// FIXME: If BeginRead() is called on other thread between the check and setting state to CS_DONE,
        /// we could set state to done when it isn't actually.
        if (((rwMarkerState - 2) & ReadMask) == 0)
        {
            state = CS_DONE;
        }
    }
    else
    {
        if (!--readDepth)
        {
            state = CS_DONE;
        }
    }
}

#pragma endregion RenderProxyList

#pragma region RenderCollector

/*! \brief Force function to be called on render thread. If we're currently on the render thread, executes inline. Otherwise, will enqueue custom deleter to be called on the render thread.  */
template <class Func>
static inline void DeleteOnRenderThread(Func&& function)
{
    if (Threads::IsOnThread(g_renderThread))
    {
        function();
        return;
    }

    using Payload = Proc<void()>;

    Mutex::Guard* pGuard = nullptr;
    HYP_DEFER({ if (pGuard) delete pGuard; });

    Payload** ppPayload = GetSafeDeleterInstance()->AllocCustom<Payload*>([](void* ptr)
        {
            Threads::AssertOnThread(g_renderThread);

            Payload* pPayload = *reinterpret_cast<Payload**>(ptr);
            AssertDebug(pPayload != nullptr && pPayload->IsValid());

            (*pPayload)();

            delete pPayload;
        },
        &pGuard);

    *ppPayload = new Payload(std::forward<Func>(function));
}

RenderCollector::RenderCollector()
    : parallelRenderingStateHead(nullptr),
      parallelRenderingStateTail(nullptr),
      batchAllocator(GetOrCreateEntityBatchAllocator<EntityInstanceBatch>()),
      renderGroupFlags(RenderGroupFlags::DEFAULT)
{
}

RenderCollector::~RenderCollector()
{
    HYP_SCOPE;

    DeleteOnRenderThread([attrs = std::move(previousAttributes),
                             m = std::move(mappingsByBucket),
                             prsHead = parallelRenderingStateHead]() mutable
        {
            attrs.Clear(/* freeMemory */ true);

            if (prsHead)
            {
                ParallelRenderingState* state = prsHead;

                while (state != nullptr)
                {
                    if (state->taskBatch != nullptr)
                    {
                        state->taskBatch->AwaitCompletion();

                        delete state->taskBatch;
                    }

                    ParallelRenderingState* nextState = state->next;

                    PoolDelete(*g_renderPool, state);

                    state = nextState;
                }
            }

            for (auto& mappings : m)
            {
                for (auto& it : mappings)
                {
                    DrawCallCollectionMapping& mapping = it.second;
                    mapping.meshProxies.Clear(/* freeMemory */ true);

                    if (mapping.indirectRenderer)
                    {
                        PoolDelete(*g_renderPool, mapping.indirectRenderer);
                        mapping.indirectRenderer = nullptr;
                    }

                    SafeDelete(std::move(mapping.renderGroup));
                }

                mappings.Clear();
            }
        });

    parallelRenderingStateHead = nullptr;
    parallelRenderingStateTail = nullptr;
}

#ifdef HYP_DEBUG_MODE
SizeType RenderCollector::NumDrawCallsCollected() const
{
    SizeType numDrawCalls = 0;

    for (const auto& mappings : mappingsByBucket)
    {
        for (const KeyValuePair<RenderableAttributeSet, DrawCallCollectionMapping>& it : mappings)
        {
            const DrawCallCollectionMapping& mapping = it.second;

            numDrawCalls += mapping.drawCallCollection.drawCalls.Size()
                + mapping.drawCallCollection.instancedDrawCalls.Size();
        }
    }

    return numDrawCalls;
}
#endif

void RenderCollector::Clear(bool freeMemory)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    for (auto& mappings : mappingsByBucket)
    {
        for (auto& it : mappings)
        {
            DrawCallCollectionMapping& mapping = it.second;
            mapping.meshProxies.Clear(/* freeMemory */ freeMemory);

            if (freeMemory)
            {
                if (mapping.indirectRenderer)
                {
                    PoolDelete(*g_renderPool, mapping.indirectRenderer);
                    mapping.indirectRenderer = nullptr;
                }

                SafeDelete(std::move(mapping.renderGroup));
            }
        }

        if (freeMemory)
        {
            mappings.Clear();
        }
    }
}

ParallelRenderingState* RenderCollector::AcquireNextParallelRenderingState()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    ParallelRenderingState* curr = parallelRenderingStateTail;

    if (!curr)
    {
        if (!parallelRenderingStateHead)
        {
            ParallelRenderingState_Shared* sharedData = PoolNew<ParallelRenderingState_Shared>(*g_renderPool);

            parallelRenderingStateHead = PoolNew<ParallelRenderingState>(*g_renderPool, sharedData);

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* taskBatch = new TaskBatch;
            taskBatch->pool = &pool;

            parallelRenderingStateHead->taskBatch = taskBatch;
            parallelRenderingStateHead->numBatches = ParallelRenderingState::MaxBatches;
        }

        curr = parallelRenderingStateHead;
    }
    else
    {
        ParallelRenderingState*& next = curr->next;

        if (!next)
        {
            ParallelRenderingState_Shared* sharedData = PoolNew<ParallelRenderingState_Shared>(*g_renderPool); // temp

            ParallelRenderingState* newParallelRenderingState = PoolNew<ParallelRenderingState>(*g_renderPool, sharedData);

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* taskBatch = new TaskBatch;
            taskBatch->pool = &pool;

            newParallelRenderingState->taskBatch = taskBatch;
            newParallelRenderingState->numBatches = ParallelRenderingState::MaxBatches;

            next = newParallelRenderingState;
        }

        curr = next;
    }

    parallelRenderingStateTail = curr;

    AssertDebug(curr != nullptr);
    AssertDebug(curr->taskBatch != nullptr);
    AssertDebug(curr->taskBatch->IsCompleted());

    return curr;
}

void RenderCollector::CommitParallelRenderingState(RenderQueue& renderQueue)
{
    HYP_SCOPE;

    ParallelRenderingState* state = parallelRenderingStateHead;

    while (state)
    {
        AssertDebug(state->taskBatch != nullptr);

        state->taskBatch->AwaitCompletion();

        renderQueue.Concat(state->rootQueue);
        state->rootQueue.Clear();

        for (uint32 i = 0; i < ParallelRenderingState::MaxBatches; i++)
        {
            renderQueue.Concat(*state->localQueues[i]);
            state->localQueues[i]->Clear();
        }

        // Add render stats counts to the engine's render stats
        for (EngineStatsValueSet& valueSet : state->statValues)
        {
            g_engineStatsRecorder->RecordValueSet(valueSet);

            valueSet = {}; // Reset counts after adding for next use
        }

        for (uint32 i = 0; i < ParallelRenderingState::MaxBatches; i++)
        {
            state->sharedData->arenas[i]->Reset();
        }

        state->drawCalls.Clear();
        state->drawCallProcs.Clear();
        state->instancedDrawCalls.Clear();
        state->instancedDrawCallProcs.Clear();

        state->taskBatch->ResetState();

        state = state->next;
    }

    parallelRenderingStateTail = nullptr;
}

void RenderCollector::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& renderSetup, uint32 bucketBits)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView(), "RenderSetup must have a View attached");
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData to perform occlusion culling");

    static const bool isIndirectRenderingEnabled = g_renderBackend->GetRenderConfig().indirectRendering;
    const bool performOcclusionCulling = isIndirectRenderingEnabled && renderSetup.passData->cullData.depthPyramidImageView != nullptr;

    if (performOcclusionCulling)
    {
        FOR_EACH_BIT(bucketBits, bitIndex)
        {
            AssertDebug(bitIndex < mappingsByBucket.Size());

            auto& mappings = mappingsByBucket[bitIndex];

            if (mappings.Empty())
            {
                continue;
            }

            for (auto& it : mappings)
            {
                DrawCallCollectionMapping& mapping = it.second;
                AssertDebug(mapping.IsValid());

                const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
                AssertDebug(renderGroup.IsValid());

                DrawCallCollection& drawCallCollection = mapping.drawCallCollection;
                IndirectRenderer* indirectRenderer = mapping.indirectRenderer;

                if (renderGroup->GetFlags() & RenderGroupFlags::OCCLUSION_CULLING)
                {
                    AssertDebug((renderGroup->GetFlags() & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));
                    AssertDebug(indirectRenderer != nullptr);

                    indirectRenderer->GetDrawState().ResetDrawState();

                    indirectRenderer->PushDrawCallsToIndirectState(drawCallCollection);
                    indirectRenderer->ExecuteCullShaderInBatches(frame, renderSetup);
                }
            }
        }
    }
}

void RenderCollector::ExecuteDrawCalls(
    FrameBase* frame,
    const RenderSetup& renderSetup,
    uint32 bucketBits,
    bool commit)
{
    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView(), "RenderSetup must have a View attached");
    AssertDebug(renderSetup.view->IsReady());

    if (renderSetup.view->GetFlags() & ViewFlags::GBUFFER)
    {
        // Pass NULL framebuffer for GBuffer rendering, as it will be handled by DeferredRenderer outside of this scope.
        ExecuteDrawCalls(frame, renderSetup, FramebufferRef::Null(), bucketBits, commit);
    }
    else
    {
        const FramebufferRef& framebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer();
        AssertDebug(framebuffer != nullptr, "Must have a valid framebuffer for rendering");

        ExecuteDrawCalls(frame, renderSetup, framebuffer, bucketBits, commit);
    }
}

void RenderCollector::ExecuteDrawCalls(
    FrameBase* frame,
    const RenderSetup& renderSetup,
    const FramebufferRef& framebuffer,
    uint32 bucketBits,
    bool commit)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    Span<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>> groupsView;

    if (bucketBits == 0)
    {
        bucketBits = AllBucketsMask;
    }

    // If only one bit is set, we can skip the loop by directly accessing the RenderGroup
    if (ByteUtil::BitCount(bucketBits) == 1)
    {
        const RenderBucket rb = RenderBucket(MathUtil::FastLog2_Pow2(bucketBits));

        auto& mappings = mappingsByBucket[rb];

        if (mappings.Empty())
        {
            return;
        }

        groupsView = { &mappings, 1 };
    }
    else
    {
        bool allEmpty = true;

        for (const auto& mappings : mappingsByBucket)
        {
            if (mappings.Any())
            {
                if (AnyOf(mappings, [&bucketBits](const auto& it)
                        {
                            return (bucketBits & (1u << uint32(it.first.GetMaterialAttributes().bucket))) != 0;
                        }))
                {
                    allEmpty = false;

                    break;
                }
            }
        }

        if (allEmpty)
        {
            return;
        }

        groupsView = mappingsByBucket.ToSpan();
    }

    if (framebuffer)
    {
        frame->renderQueue << BeginFramebuffer(framebuffer);
    }

    for (const auto& mappings : groupsView)
    {
        for (const auto& it : mappings)
        {
            const RenderableAttributeSet& attributes = it.first;

            const DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

            if (!(bucketBits & (1u << uint32(rb))))
            {
                continue;
            }

            const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
            AssertDebug(renderGroup != nullptr);

            const DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

            IndirectRenderer* indirectRenderer = mapping.indirectRenderer;

            ParallelRenderingState* parallelRenderingState = nullptr;

            if (renderGroup->GetFlags() & RenderGroupFlags::PARALLEL_RENDERING)
            {
                parallelRenderingState = AcquireNextParallelRenderingState();
            }

            renderGroup->PerformRendering(frame, renderSetup, drawCallCollection, indirectRenderer, parallelRenderingState);

            if (parallelRenderingState != nullptr)
            {
                AssertDebug(parallelRenderingState->taskBatch != nullptr);

                TaskSystem::GetInstance().EnqueueBatch(parallelRenderingState->taskBatch);
            }
        }
    }

    if (commit)
    {
        // Wait for all parallel rendering tasks to finish
        CommitParallelRenderingState(frame->renderQueue);
    }

    if (framebuffer)
    {
        frame->renderQueue << EndFramebuffer(framebuffer);
    }
}

void RenderCollector::RemoveEmptyRenderGroups()
{
    HYP_SCOPE;

    for (auto& mappings : mappingsByBucket)
    {
        for (auto it = mappings.Begin(); it != mappings.End();)
        {
            DrawCallCollectionMapping& mapping = it->second;
            AssertDebug(mapping.IsValid());

            if (mapping.meshProxies.Any())
            {
                ++it; // skip non-empty

                continue;
            }

            if (mapping.indirectRenderer)
            {
                PoolDelete(*g_renderPool, mapping.indirectRenderer);
                mapping.indirectRenderer = nullptr;
            }

            SafeDelete(std::move(mapping.renderGroup));

            it = mappings.Erase(it);
        }
    }
}

uint32 RenderCollector::NumRenderGroups() const
{
    HYP_SCOPE;

    uint32 count = 0;

    for (const auto& mappings : mappingsByBucket)
    {
        for (const auto& it : mappings)
        {
            const DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            if (mapping.IsValid())
            {
                ++count;
            }
        }
    }

    return count;
}

void RenderCollector::BuildRenderGroups(View* view, RenderProxyList& renderProxyList)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);
    AssertDebug(renderProxyList.state == RenderProxyList::CS_READING);

    const RenderableAttributeSet* overrideAttributes = view->GetOverrideAttributes().TryGet();

    auto diff = renderProxyList.GetMeshEntities().GetDiff();

    if (!diff.NeedsUpdate())
    {
        return;
    }

    Array<ObjId<Entity>, RenderTempAllocator> changedIds;
    renderProxyList.GetMeshEntities().GetChanged(changedIds);

    if (changedIds.Any())
    {
        for (const ObjId<Entity>& id : changedIds)
        {
#ifdef HYP_DEBUG_MODE
            // type check - cannot be a subclass of Entity, indices would get messed up
            static const TypeId s_entityTypeId = TypeId::ForType<Entity>();
            Assert(id.GetTypeId() == s_entityTypeId, "Cannot include instance of Entity subclass in RenderGroup: {}", LookupTypeName(id.GetTypeId()));
#endif

            const uint32 idx = id.ToIndex();

            RenderableAttributeSet* cachedAttributes = previousAttributes.TryGet(id.ToIndex());
            AssertDebug(cachedAttributes != nullptr);

            // remove from prev
            auto& prevMappings = mappingsByBucket[cachedAttributes->GetMaterialAttributes().bucket];

            auto it = prevMappings.Find(*cachedAttributes);
            Assert(it != prevMappings.End());

            DrawCallCollectionMapping& prevMapping = it->second;

            RenderProxyMesh* meshProxy = prevMapping.meshProxies.Get(idx);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet newAttributes;
            GeometryPass::BuildAttributes(*meshProxy, newAttributes, overrideAttributes);

            AssertDebug(newAttributes.GetMeshAttributes().vertexAttributes != 0);

            if (newAttributes == *cachedAttributes)
            {
                // not changed, skip
                continue;
            }

            // Add proxy to group
            DrawCallCollectionMapping& newMapping = mappingsByBucket[newAttributes.GetMaterialAttributes().bucket][newAttributes];
            AssertDebug(&newMapping != &prevMapping);

            Handle<RenderGroup>& rg = newMapping.renderGroup;

            if (!rg.IsValid())
            {
                rg = CreateRenderGroup(this, newMapping, newAttributes);
            }

            AssertDebug(meshProxy->mesh != nullptr && meshProxy->material != nullptr);

            prevMapping.meshProxies.EraseAt(idx);
            newMapping.meshProxies.Set(idx, meshProxy);

            *cachedAttributes = newAttributes;
        }
    }

    Array<ObjId<Entity>, RenderTempAllocator> removed;
    renderProxyList.GetMeshEntities().GetRemoved(removed, false /* includeChanged */);

    Array<ObjId<Entity>, RenderTempAllocator> added;
    renderProxyList.GetMeshEntities().GetAdded(added, false /* includeChanged */);

    if (removed.Any())
    {
        for (const ObjId<Entity>& id : removed)
        {
#ifdef HYP_DEBUG_MODE
            // type check - cannot be a subclass of Entity, indices would get messed up
            Assert(id.GetTypeId() == TypeId::ForType<Entity>());
#endif

            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            if (!meshProxy)
            {
                continue;
            }

            const uint32 idx = id.ToIndex();

            AssertDebug(previousAttributes.HasIndex(idx));

            const RenderableAttributeSet& attributes = previousAttributes.Get(idx);

            auto& mappings = mappingsByBucket[attributes.GetMaterialAttributes().bucket];

            auto it = mappings.Find(attributes);
            Assert(it != mappings.End());

            DrawCallCollectionMapping& mapping = it->second;
            Assert(mapping.IsValid());

            AssertDebug(mapping.meshProxies.HasIndex(idx));
            mapping.meshProxies.EraseAt(idx);

            previousAttributes.EraseAt(idx);
        }
    }

    if (added.Any())
    {
        for (const ObjId<Entity>& id : added)
        {
#ifdef HYP_DEBUG_MODE
            // type check - cannot be a subclass of Entity, indices would get messed up
            static const TypeId s_entityTypeId = TypeId::ForType<Entity>();
            Assert(id.GetTypeId() == s_entityTypeId, "Cannot include instance of Entity subclass in RenderGroup: {}", LookupTypeName(id.GetTypeId()));
#endif

            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet attributes;
            GeometryPass::BuildAttributes(*meshProxy, attributes, overrideAttributes);

            // Add proxy to group
            DrawCallCollectionMapping& mapping = mappingsByBucket[attributes.GetMaterialAttributes().bucket][attributes];
            Handle<RenderGroup>& rg = mapping.renderGroup;

            if (!rg.IsValid())
            {
                rg = CreateRenderGroup(this, mapping, attributes);
            }

            const uint32 idx = id.ToIndex();

            mapping.meshProxies.Set(idx, const_cast<RenderProxyMesh*>(meshProxy));
            previousAttributes.Set(idx, attributes);
        }
    }
}

// Called at start of frame on render thread
void RenderCollector::BuildDrawCalls(uint32 bucketBits)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    static const bool uniquePerMaterial = g_renderBackend->GetRenderConfig().uniqueDrawCallPerMaterial;

    if (bucketBits == 0)
    {
        bucketBits = AllBucketsMask;
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::Iterator;
    Array<IteratorType> iterators;

    FOR_EACH_BIT(bucketBits, bitIndex)
    {
        AssertDebug(bitIndex < mappingsByBucket.Size());

        auto& mappings = mappingsByBucket[bitIndex];

        if (mappings.Empty())
        {
            continue;
        }

        for (auto& it : mappings)
        {
            iterators.PushBack(&it);
        }
    }

    if (iterators.Empty())
    {
        return;
    }

    // std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
    //     {
    //         return int(lhs->first.GetLayerIndex()) < int(rhs->first.GetLayerIndex());
    //     });

    for (IteratorType it : iterators)
    {
        const RenderableAttributeSet& attributes = it->first;

        DrawCallCollectionMapping& mapping = it->second;
        AssertDebug(mapping.IsValid());

        DrawCallCollection previousDrawState = std::move(mapping.drawCallCollection);

        DrawCallCollection& drawCallCollection = mapping.drawCallCollection;
        drawCallCollection.batchAllocator = batchAllocator;
        drawCallCollection.renderGroup = mapping.renderGroup;

        for (RenderProxyMesh* meshProxy : mapping.meshProxies)
        {
            AssertDebug(meshProxy->mesh != nullptr && meshProxy->mesh->IsReady());
            AssertDebug(meshProxy->material != nullptr && meshProxy->material->IsReady());

            if (meshProxy->instanceData.numInstances == 0)
            {
                continue;
            }

            DrawCallID drawCallId;

            if (uniquePerMaterial)
            {
                drawCallId = DrawCallID(meshProxy->mesh->Id(), meshProxy->material->Id());
            }
            else
            {
                drawCallId = DrawCallID(meshProxy->mesh->Id());
            }

            if (!meshProxy->instanceData.enableAutoInstancing && meshProxy->instanceData.numInstances == 1)
            {
                drawCallCollection.PushRenderProxy(drawCallId, *meshProxy); // NOLINT(bugprone-use-after-move)

                continue;
            }

            EntityInstanceBatch* batch = nullptr;

            if (previousDrawState.IsValid())
            {
                // take a batch for reuse if a draw call was using one
                if ((batch = previousDrawState.TakeDrawCallBatch(drawCallId)) != nullptr)
                {
                    const uint32 batchIndex = batch->batchIndex;
                    AssertDebug(batchIndex != ~0u);

                    // Reset it
                    *batch = EntityInstanceBatch { batchIndex };

                    // drawCallCollection.batchAllocator->GetGpuBufferHolder()->MarkDirty(batch->batchIndex);
                }
            }

            drawCallCollection.PushRenderProxyInstanced(batch, drawCallId, *meshProxy);
        }

        if (previousDrawState.IsValid())
        {
            // Any draw calls that were not reused from the previous state, clear them out and release batch indices.
            previousDrawState.ResetDrawCalls();
        }
    }
}

#pragma endregion RenderCollector

} // namespace hyperion

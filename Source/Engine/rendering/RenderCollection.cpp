/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Frame.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <rendering/util/ResourceTracker.hpp>
#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <scene/Scene.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/FogVolume.hpp>
#include <scene/ParticleVolume.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/camera/Camera.hpp>

#include <scene/animation/Skeleton.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/ThreadLocalStorage.hpp>

#include <Core/Util.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>

namespace Hyperion {

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

static constexpr uint32 AllBucketsMask = (1u << NumRenderBuckets) - 1;

#pragma region ParallelRenderingState

// Holds shared data for ParallelRenderingState instances to reduce memory usage
struct ParallelRenderingState_Shared
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    static constexpr uint32 MaxBatches = ParallelRenderingState::MaxBatches;
    static constexpr SizeType MaxLocalQueueSizeBytes = 64 * 1024 * 1024;

    using LocalQueue = ParallelRenderingState::LocalQueue;

    FixedArray<LocalQueue*, MaxBatches> localQueues;

    ParallelRenderingState_Shared()
        : localQueues {}
    {
        AssertOnThread(g_renderThread);

        for (uint32 i = 0; i < MaxBatches; i++)
        {
            localQueues[i] = PoolNew<LocalQueue>(*g_renderPool);
        }
    }

    ~ParallelRenderingState_Shared()
    {
        AssertOnThread(g_renderThread);

        for (uint32 i = 0; i < MaxBatches; i++)
        {
            if (localQueues[i])
            {
                PoolDelete(*g_renderPool, localQueues[i]);
            }
        }
    }

    void Reset()
    {
        for (uint32 i = 0; i < ParallelRenderingState::MaxBatches; i++)
        {
            // don't free memory; each queue uses thread-local memory allocators
            localQueues[i]->Clear(/* freeMemory */ false);
        }
    }
};

ParallelRenderingState::ParallelRenderingState(ParallelRenderingState_Shared* sharedData, bool ownsSharedData)
    : sharedData(sharedData),
      ownsSharedData(ownsSharedData)
{
    Assert(sharedData != nullptr);

    for (uint32 i = 0; i < MaxBatches; i++)
    {
        localQueues[i] = sharedData->localQueues[i];
    }
}

ParallelRenderingState::~ParallelRenderingState()
{
    if (ownsSharedData)
    {
        delete sharedData;
    }
}

#pragma endregion ParallelRenderingState

#pragma region GeometryPass

namespace GeometryPass {
namespace Props {

/// Static property names

static const Name s_nameInstancing = NAME("INSTANCING");
static const Name s_nameAlphaDiscard = NAME("ALPHA_DISCARD");
static const Name s_nameSkinning = NAME("SKINNING");
static const Name s_nameShadingType = NAME("SHADING_TYPE");
static const Name s_nameDeferred = NAME("DEFERRED");
static const Name s_nameForward = NAME("FORWARD");
static const Name s_nameLightmapped = NAME("LIGHTMAPPED");

static const Name s_nameHasDiffuseMap = NAME("HAS_DIFFUSE_MAP");
static const Name s_nameHasNormalMap = NAME("HAS_NORMAL_MAP");
static const Name s_nameHasParallaxMap = NAME("HAS_PARALLAX_MAP");
static const Name s_nameHasMetalnessMap = NAME("HAS_METALNESS_MAP");
static const Name s_nameHasRoughnessMap = NAME("HAS_ROUGHNESS_MAP");
static const Name s_nameHasAoMap = NAME("HAS_AO_MAP");

/// Property interning

static const ShaderPropertyId s_propInstancing = InternShaderProperty(ShaderProperty(s_nameInstancing));
static const ShaderPropertyId s_propAlphaDiscard = InternShaderProperty(ShaderProperty(s_nameAlphaDiscard));
static const ShaderPropertyId s_propSkinning = InternShaderProperty(ShaderProperty(s_nameSkinning));

// shading mode
static const ShaderPropertyId s_propShadingTypeDeferred = InternShaderProperty(ShaderProperty(s_nameShadingType, Name(s_nameDeferred)));
static const ShaderPropertyId s_propShadingTypeForward = InternShaderProperty(ShaderProperty(s_nameShadingType, Name(s_nameForward)));
static const ShaderPropertyId s_propShadingTypeLightmapped = InternShaderProperty(ShaderProperty(s_nameShadingType, Name(s_nameLightmapped)));

// textures
static const ShaderPropertyId s_propHasDiffuseMap = InternShaderProperty(ShaderProperty(s_nameHasDiffuseMap));
static const ShaderPropertyId s_propHasNormalMap = InternShaderProperty(ShaderProperty(s_nameHasNormalMap));
static const ShaderPropertyId s_propHasParallaxMap = InternShaderProperty(ShaderProperty(s_nameHasParallaxMap));
static const ShaderPropertyId s_propHasMetalnessMap = InternShaderProperty(ShaderProperty(s_nameHasMetalnessMap));
static const ShaderPropertyId s_propHasRoughnessMap = InternShaderProperty(ShaderProperty(s_nameHasRoughnessMap));
static const ShaderPropertyId s_propHasAoMap = InternShaderProperty(ShaderProperty(s_nameHasAoMap));

static const Pair<MaterialTextureKey, ShaderPropertyId> s_textureProperties[] = {
    { MaterialTextureKey::Diffuse, InternShaderProperty(ShaderProperty(s_nameHasDiffuseMap)) },
    { MaterialTextureKey::Normals, InternShaderProperty(ShaderProperty(s_nameHasNormalMap)) },
    { MaterialTextureKey::Parallax, InternShaderProperty(ShaderProperty(s_nameHasParallaxMap)) },
    { MaterialTextureKey::Metalness, InternShaderProperty(ShaderProperty(s_nameHasMetalnessMap)) },
    { MaterialTextureKey::Roughness, InternShaderProperty(ShaderProperty(s_nameHasRoughnessMap)) },
    { MaterialTextureKey::AmbientOcclusion, InternShaderProperty(ShaderProperty(s_nameHasAoMap)) }
};

} // namespace Props

// Get the stencil reference value to set for a lightmapped object,
// based on its associated atlas index.
static constexpr inline uint8 GetLightmapStencilValue(LightmapElementId lightmapElementId)
{
    if (lightmapElementId == InvalidLightmapElementId)
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

    attributes = proxy.cachedAttributes;

    if (overrideAttributes)
    {
        MaterialAttributes newMaterialAttributes = overrideAttributes->GetMaterialAttributes();
        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(newMaterialAttributes);
    }

    const bool hasInstancing = proxy.instanceData.enableAutoInstancing || proxy.instanceData.numInstances > 1;
    const bool hasForwardLighting = attributes.GetMaterialAttributes().bucket == RenderBucket::Translucent;
    const bool hasLightmaps = attributes.GetMaterialAttributes().bucket == RenderBucket::Lightmapped;
    const bool hasDeferredLighting = !hasForwardLighting && !hasLightmaps;
    const bool hasAlphaDiscard = bool(attributes.GetMaterialAttributes().flags & MAF_ALPHA_DISCARD);
    const bool hasSkinning = proxy.skeleton != nullptr && proxy.skeleton->GetRootBone() != nullptr;

    static const bool s_isPathTracer = CoreApi::GetGlobalConfig().Get("Rendering.RayTracing.PathTracing.Enabled").ToBool();

    // if lightmap volume is set we need stencil testing
    if (hasLightmaps && !s_isPathTracer)
    {
        const uint8 stencilReferenceValue = GetLightmapStencilValue(proxy.lightmapElementId) & LightmapStencilMask;

        if (stencilReferenceValue != (attributes.GetMaterialAttributes().stencilReference & LightmapStencilMask))
        {
            attributes.GetMaterialAttributes().flags |= MAF_STENCIL_TEST;
            attributes.GetMaterialAttributes().stencilReference &= ~LightmapStencilMask;
            attributes.GetMaterialAttributes().stencilReference |= stencilReferenceValue;
            attributes.Invalidate();
        }
    }
    else if (attributes.GetMaterialAttributes().stencilReference & LightmapStencilMask)
    {
        attributes.GetMaterialAttributes().stencilReference &= ~LightmapStencilMask;

        if (!attributes.GetMaterialAttributes().stencilReference)
        {
            attributes.GetMaterialAttributes().flags &= ~MAF_STENCIL_TEST;
        }

        attributes.Invalidate();
    }

    const ShaderPropertySet& currentShaderProperties = attributes.GetShaderProperties();

    ShaderPropertySet newShaderProperties = currentShaderProperties;

    if (hasInstancing != currentShaderProperties.Test(Props::s_propInstancing))
    {
        newShaderProperties.Set(Props::s_propInstancing, hasInstancing);
    }

    {
        if (hasDeferredLighting != currentShaderProperties.Test(Props::s_propShadingTypeDeferred))
        {
            newShaderProperties.Set(Props::s_propShadingTypeDeferred, hasDeferredLighting);
        }

        if (hasForwardLighting != currentShaderProperties.Test(Props::s_propShadingTypeForward))
        {
            newShaderProperties.Set(Props::s_propShadingTypeForward, hasForwardLighting);
        }

        if (hasLightmaps != currentShaderProperties.Test(Props::s_propShadingTypeLightmapped))
        {
            newShaderProperties.Set(Props::s_propShadingTypeLightmapped, hasLightmaps);
        }
    }

    if (hasAlphaDiscard != currentShaderProperties.Test(Props::s_propAlphaDiscard))
    {
        newShaderProperties.Set(Props::s_propAlphaDiscard, hasAlphaDiscard);
    }

    if (hasSkinning != currentShaderProperties.Test(Props::s_propSkinning))
    {
        newShaderProperties.Set(Props::s_propSkinning, hasSkinning);
    }

    // update shader properties to reflect texture presence based on texture mask
    for (const auto& [textureKey, property] : Props::s_textureProperties)
    {
        const bool presence = bool(attributes.GetMaterialAttributes().textureMask & uint32(textureKey));

        if (presence != currentShaderProperties.Test(property))
        {
            newShaderProperties.Set(property, presence);
        }
    }

    if (newShaderProperties != currentShaderProperties)
    {
        // Update the shader definition in the attributes
        attributes.SetShaderProperties(newShaderProperties);
    }
}

} // namespace GeometryPass

#pragma endregion GeometryPass

static RenderGroup* CreateRenderGroup(
    RenderCollector* renderCollector,
    DrawCallCollectionMapping& mapping,
    const RenderableAttributeSet& attributes)
{
    EnumFlags<RenderGroupFlags> renderGroupFlags = RenderGroupFlags::DEFAULT;

    // Disable occlusion culling for translucent objects
    const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

    if (RenderBucketMask<RenderBucket::Translucent, RenderBucket::Sky, RenderBucket::Debug> & (1u << uint32(rb)))
    {
        renderGroupFlags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
    }

    // Create RenderGroup
    RenderGroup* rg = new RenderGroup(attributes, renderGroupFlags);

    if (renderGroupFlags & RenderGroupFlags::INDIRECT_RENDERING)
    {
        AssertDebug(mapping.indirectRenderer == nullptr, "Indirect renderer already exists on mapping");

        mapping.indirectRenderer = PoolNew<IndirectRenderer>(*g_renderPool);
        mapping.indirectRenderer->Create(renderCollector->batchAllocator);
    }

    mapping.drawCallCollection.batchAllocator = renderCollector->batchAllocator;
    
    // If parallel rendering is globally disabled, disable it for this RenderGroup
    if (!g_renderInterface->GetRenderConfig().parallelRendering)
    {
        rg->flags &= ~RenderGroupFlags::PARALLEL_RENDERING;
    }

    if (!g_renderInterface->GetRenderConfig().indirectRendering)
    {
        rg->flags &= ~RenderGroupFlags::INDIRECT_RENDERING;
    }

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

                pProxy->version = *resource->GetRenderProxyVersionPtr();
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

            pProxy->version = *resource.GetRenderProxyVersionPtr();
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

#pragma region RenderProxyList

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
#if HYP_DEBUG_MODE
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
        HYP_LOG(Rendering, Verbose, "RenderProxyList destroyed with {} render proxies still in it", numRenderProxies);
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
        uint64 rwMarkerState = AtomicBitOr(&rwMarker, WriteFlag);
        while (HYP_UNLIKELY(rwMarkerState & ReadMask))
        {
            HYP_LOG(Rendering, Verbose, "Busy-waiting for read marker to be released! "
                                      "If this is occuring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

            rwMarkerState = AtomicAdd(&rwMarker, 0);
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
        AtomicBitAnd(&rwMarker, ~WriteFlag);
    }
}

void RenderProxyList::BeginRead(bool* pOutSuccess)
{
    constexpr uint32 MaxSpinsBeforeFail = 32;

    if (isShared)
    {
        int64 rwMarkerState;
        uint32 numSpins = 0;

        do
        {
            rwMarkerState = AtomicAdd(&rwMarker, 2);

            if (HYP_UNLIKELY(rwMarkerState & WriteFlag))
            {
                HYP_LOG(Rendering, Verbose, "Waiting for write marker to be released. "
                                          "If this is occurring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

                AtomicSub(&rwMarker, 2);

                if (pOutSuccess != nullptr && ++numSpins >= MaxSpinsBeforeFail)
                {
                    // fail state; stop spinning
                    *pOutSuccess = false;
                    return;
                }

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

    if (pOutSuccess)
    {
        *pOutSuccess = true;
    }
}

void RenderProxyList::EndRead()
{
    AssertDebug(state == CS_READING);

    if (isShared)
    {
        int64 rwMarkerState = AtomicSub(&rwMarker, 2);
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
    if (IsOnThread(g_renderThread))
    {
        function();
        return;
    }

    using Payload = Proc<void()>;

    Mutex::Guard* pGuard = nullptr;
    HYP_DEFER({ if (pGuard) delete pGuard; });

    Payload** ppPayload = DeletionQueue::GetInstance().AllocCustom<Payload*>([](void* ptr)
        {
            AssertOnThread(g_renderThread);

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
      batchAllocator(nullptr),
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

            Array<FixedArray<ParallelRenderingState::LocalQueue*, ParallelRenderingState::MaxBatches>> allLocalQueues;

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

                    if (state->ownsSharedData && state->sharedData != nullptr)
                    {
                        // take the local queues to free for ourselves - we need to free up their memory on a per-thread basis
                        allLocalQueues.PushBack(state->sharedData->localQueues);

                        state->sharedData->localQueues = {};
                    }

                    ParallelRenderingState* nextState = state->next;

                    delete state;

                    state = nextState;
                }
            }

            if (allLocalQueues.Any())
            {
                // we have to free up the memory for each local queue on individual threads,
                // due to the use of ThreadAllocator:
                Array<Task<void>> tasks;
                tasks.Reserve(ParallelRenderingState::MaxBatches);

                auto& poolThreads = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER).GetThreads();

                for (uint32 i = 0; i < uint32(poolThreads.Size()); i++)
                {
                    AssertDebug(poolThreads[i] != nullptr);

                    tasks.EmplaceBack(poolThreads[i]->GetScheduler().Enqueue([&allLocalQueues]()
                        {
                            for (FixedArray<ParallelRenderingState::LocalQueue*, ParallelRenderingState::MaxBatches>& queues : allLocalQueues)
                            {
                                ParallelRenderingState::LocalQueue* currQueue = queues[GetCurrentThreadIndex()];

                                if (currQueue != nullptr)
                                {
                                    currQueue->~TRenderQueue();
                                }
                            }
                        }));
                }

                AwaitAll(tasks.ToSpan());

                for (FixedArray<ParallelRenderingState::LocalQueue*, ParallelRenderingState::MaxBatches>& queues : allLocalQueues)
                {
                    for (ParallelRenderingState::LocalQueue* queue : queues)
                    {
                        // NOTE: not PoolDelete(), we already destructed it on its own thread.
                        PoolFree(*g_renderPool, queue);
                    }
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

                    delete mapping.renderGroup;
                    mapping.renderGroup = nullptr;
                }

                mappings.Clear();
            }
        });

    parallelRenderingStateHead = nullptr;
    parallelRenderingStateTail = nullptr;
}

#if HYP_DEBUG_MODE
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
    AssertOnThread(g_renderThread);

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

                delete mapping.renderGroup;
                mapping.renderGroup = nullptr;
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
    AssertOnThread(g_renderThread);

    ParallelRenderingState* curr = parallelRenderingStateTail;

    if (!curr)
    {
        if (!parallelRenderingStateHead)
        {
            ParallelRenderingState_Shared* sharedData = new ParallelRenderingState_Shared;

            parallelRenderingStateHead = new ParallelRenderingState(sharedData, true);

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
            ParallelRenderingState_Shared* sharedData = new ParallelRenderingState_Shared;

            ParallelRenderingState* newParallelRenderingState = new ParallelRenderingState(sharedData, true);

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
        state->rootQueue.Clear(/* freeMemory */ false);

        for (uint32 i = 0; i < ParallelRenderingState::MaxBatches; i++)
        {
            renderQueue.Concat(*state->localQueues[i]);
            state->localQueues[i]->Clear(/* freeMemory */ false);
        }

        renderQueue << SetStencilState(0, 0xFF, 0x0); // reset stencil

        // Add render stats counts to the engine's render stats
        for (EngineStatsValueSet& valueSet : state->statValues)
        {
            g_engineStats->RecordValueSet(valueSet);

            valueSet = {}; // Reset counts after adding for next use
        }

        state->sharedData->Reset();

        state->drawCalls.Clear();
        state->drawCallProcs.Clear();
        state->instancedDrawCalls.Clear();
        state->instancedDrawCallProcs.Clear();

        state->taskBatch->ResetState();

        state = state->next;
    }

    // Reset draw states
    renderQueue << SetTopology(TOP_TRIANGLES);
    renderQueue << SetFillMode(FM_FILL);
    renderQueue << SetFaceCullMode(FCM_BACK);
    renderQueue << SetVertexAttributes(VertexAttributeSet::StaticMeshVertexAttributes);
    renderQueue << SetCurrentBlendFunction(BlendFunction::None());
    renderQueue << SetDepthWrite(true);
    renderQueue << SetDepthTest(true);
    renderQueue << SetStencilTest(false);

    parallelRenderingStateTail = nullptr;
}

void RenderCollector::PerformOcclusionCulling(Frame* frame, const RenderSetup& renderSetup, uint32 bucketBits)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData to perform occlusion culling");

    static const bool s_isIndirectRenderingEnabled = g_renderInterface->GetRenderConfig().indirectRendering;
    const bool performOcclusionCulling = s_isIndirectRenderingEnabled && renderSetup.passData->cullData.depthPyramidImageView != nullptr;

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

                RenderGroup* renderGroup = mapping.renderGroup;
                AssertDebug(renderGroup != nullptr);

                DrawCallCollection& drawCallCollection = mapping.drawCallCollection;
                IndirectRenderer* indirectRenderer = mapping.indirectRenderer;

                if (renderGroup->flags & RenderGroupFlags::OCCLUSION_CULLING)
                {
                    AssertDebug((renderGroup->flags & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));
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
    Frame* frame,
    const RenderSetup& renderSetup,
    uint32 bucketBits,
    bool commit)
{
    AssertDebug(renderSetup.world && renderSetup.view);
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
    Frame* frame,
    const RenderSetup& renderSetup,
    const FramebufferRef& framebuffer,
    uint32 bucketBits,
    bool commit)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    Span<HashMap<RenderableAttributeSet, DrawCallCollectionMapping, NodeAllocator<RenderAllocator>>> groupsView;

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

        for (auto& mappings : mappingsByBucket)
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

    for (auto& mappings : groupsView)
    {
        for (auto& it : mappings)
        {
            const RenderableAttributeSet& attributes = it.first;

            DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

            if (!(bucketBits & (1u << uint32(rb))))
            {
                continue;
            }

            RenderGroup* renderGroup = mapping.renderGroup;
            AssertDebug(renderGroup != nullptr);

            DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

            IndirectRenderer* indirectRenderer = mapping.indirectRenderer;

            ParallelRenderingState* parallelRenderingState = nullptr;

            if (renderGroup->flags & RenderGroupFlags::PARALLEL_RENDERING)
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

// Called at start of frame on render thread
void RenderCollector::BuildDrawCalls(uint32 bucketBits)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!batchAllocator)
    {
        batchAllocator = GetOrCreateEntityBatchAllocator<EntityInstanceBatch>();
    }

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

        DrawCallCollection prevDrawCallCollection = std::move(mapping.drawCallCollection);

        DrawCallCollection& drawCallCollection = mapping.drawCallCollection;
        drawCallCollection.batchAllocator = batchAllocator;
        drawCallCollection.renderGroup = mapping.renderGroup;

        for (RenderProxyMesh* meshProxy : mapping.meshProxies)
        {
            AssertDebug(meshProxy->mesh != nullptr
                        && meshProxy->mesh->GetVertexBuffer() != nullptr
                        && meshProxy->mesh->GetIndexBuffer() != nullptr);

            AssertDebug(meshProxy->material != nullptr && meshProxy->material->IsReady());

            if (meshProxy->instanceData.numInstances == 0)
            {
                continue;
            }

            DrawCallID drawCallId = DrawCallID(meshProxy->mesh->Id(), meshProxy->material->Id());

            if (!meshProxy->instanceData.enableAutoInstancing && meshProxy->instanceData.numInstances == 1)
            {
                drawCallCollection.PushRenderProxy(drawCallId, *meshProxy); // NOLINT(bugprone-use-after-move)

                continue;
            }

            EntityInstanceBatch* batch = nullptr;

            if (prevDrawCallCollection.IsValid())
            {
                // take a batch for reuse if a draw call was using one
                if ((batch = prevDrawCallCollection.RecycleDrawBatch(drawCallId)) != nullptr)
                {
                    const uint32 batchIndex = batch->batchIndex;
                    AssertDebug(batchIndex != ~0u);

                    // Reset it
                    *batch = EntityInstanceBatch { batchIndex };
                }
            }

            drawCallCollection.PushRenderProxyInstanced(batch, drawCallId, *meshProxy);
        }

        if (prevDrawCallCollection.IsValid())
        {
            // Any draw calls that were not reused from the previous state, clear them out and release batch indices.
            prevDrawCallCollection.ResetDrawCalls();
        }
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

            delete mapping.renderGroup;
            mapping.renderGroup = nullptr;

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
#if HYP_DEBUG_MODE
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

            DrawCallCollectionMapping* prevMapping = &it->second;

            RenderProxyMesh* meshProxy = prevMapping->meshProxies.Get(idx);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet newAttributes;
            GeometryPass::BuildAttributes(*meshProxy, newAttributes, overrideAttributes);

            AssertDebug(newAttributes.GetMeshAttributes().vertexAttributes != 0);

            if (newAttributes == *cachedAttributes)
            {
                // not changed, skip
                continue;
            }
            
            prevMapping->meshProxies.EraseAt(idx);
            prevMapping = nullptr;

            // Add proxy to group
            DrawCallCollectionMapping& newMapping = mappingsByBucket[newAttributes.GetMaterialAttributes().bucket][newAttributes];

            RenderGroup*& rg = newMapping.renderGroup;

            if (!rg)
            {
                rg = CreateRenderGroup(this, newMapping, newAttributes);
            }

            AssertDebug(meshProxy->mesh != nullptr && meshProxy->material != nullptr);

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
#if HYP_DEBUG_MODE
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
#if HYP_DEBUG_MODE
            // type check - cannot be a subclass of Entity, indices would get messed up
            static const TypeId s_entityTypeId = TypeId::ForType<Entity>();
            Assert(id.GetTypeId() == s_entityTypeId, "Cannot include instance of Entity subclass in RenderGroup: {}", LookupTypeName(id.GetTypeId()));
#endif

            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet attributes;
            GeometryPass::BuildAttributes(*meshProxy, attributes, overrideAttributes);

            const RenderBucket bucket = attributes.GetMaterialAttributes().bucket;
            AssertDebug(bucket < mappingsByBucket.Size());

            // Add proxy to group
            DrawCallCollectionMapping& mapping = mappingsByBucket[bucket][attributes];
            RenderGroup*& rg = mapping.renderGroup;

            if (!rg)
            {
                rg = CreateRenderGroup(this, mapping, attributes);
            }

            const uint32 idx = id.ToIndex();

            mapping.meshProxies.Set(idx, const_cast<RenderProxyMesh*>(meshProxy));
            previousAttributes.Set(idx, attributes);
        }
    }
}

#pragma endregion RenderCollector

} // namespace Hyperion

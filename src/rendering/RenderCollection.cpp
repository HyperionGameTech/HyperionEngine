/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderMaterial.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderConfig.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>

#include <scene/Scene.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>

#include <scene/camera/Camera.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/Threads.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/Util.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

static constexpr bool doParallelCollection = false; // true;

/// FIXME: Refactor to not need meshProxiesBySubtype / previousAttributesBySubtype.

#pragma region DrawCallCollectionMapping

DrawCallCollectionMapping::DrawCallCollectionMapping()
{
    const HypClass* entityClass = Entity::Class();
    AssertDebug(entityClass != nullptr);

    meshProxiesBySubtype.Resize(entityClass->GetNumDescendants() + 1);
}

#pragma endregion DrawCallCollectionMapping

#pragma region RenderProxyList

static RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxyMesh& proxy, const RenderableAttributeSet* overrideAttributes = nullptr)
{
    HYP_SCOPE;

    const Handle<Mesh>& mesh = proxy.mesh;
    Assert(mesh.IsValid());

    const Handle<Material>& material = proxy.material;
    Assert(material.IsValid());

    RenderableAttributeSet attributes {
        mesh->GetMeshAttributes(),
        material->GetRenderAttributes()
    };

    if (overrideAttributes)
    {
        if (const ShaderDefinition& overrideShaderDefinition = overrideAttributes->GetShaderDefinition())
        {
            attributes.SetShaderDefinition(overrideShaderDefinition);
        }

        const ShaderDefinition& shaderDefinition = overrideAttributes->GetShaderDefinition().IsValid()
            ? overrideAttributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
        Assert(shaderDefinition.IsValid());
#endif

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

    return attributes;
}

static const Name g_nameInstancing = NAME("INSTANCING");
static const Name g_nameForwardLighting = NAME("FORWARD_LIGHTING");
static const Name g_nameAlphaDiscard = NAME("ALPHA_DISCARD");
static const Name g_nameSkinning = NAME("SKINNING");

static void UpdateRenderableAttributesDynamic(const RenderProxyMesh* proxy, RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    union
    {
        struct
        {
            bool hasInstancing : 1;
            bool hasForwardLighting : 1;
            bool hasAlphaDiscard : 1;
            bool hasSkinning : 1;
        };

        uint64 overridden;
    };

    overridden = 0;

    hasInstancing = proxy->instanceData.enableAutoInstancing || proxy->instanceData.numInstances > 1;
    hasForwardLighting = attributes.GetMaterialAttributes().bucket == RB_TRANSLUCENT;
    hasAlphaDiscard = bool(attributes.GetMaterialAttributes().flags & MAF_ALPHA_DISCARD);
    hasSkinning = proxy->skeleton.IsValid() && proxy->skeleton->NumBones() > 0;

    //    // temp testing
    //    MaterialAttributes materialAttributes = attributes.GetMaterialAttributes();
    //    materialAttributes.stencilFunction.mask = 0xFFu;
    //    materialAttributes.stencilFunction.value = 0x1u;
    //    attributes.SetMaterialAttributes(materialAttributes);
    //    overridden = 1;

    if (!overridden)
    {
        return;
    }

    bool shaderDefinitionChanged = false;
    ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();

    if (hasInstancing && !shaderDefinition.GetProperties().Has(g_nameInstancing))
    {
        shaderDefinition.GetProperties().Set(g_nameInstancing);
        shaderDefinitionChanged = true;
    }

    if (hasForwardLighting && !shaderDefinition.GetProperties().Has(g_nameForwardLighting))
    {
        shaderDefinition.GetProperties().Set(g_nameForwardLighting);
        shaderDefinitionChanged = true;
    }

    if (hasAlphaDiscard && !shaderDefinition.GetProperties().Has(g_nameAlphaDiscard))
    {
        shaderDefinition.GetProperties().Set(g_nameAlphaDiscard);
        shaderDefinitionChanged = true;
    }

    if (hasSkinning && !shaderDefinition.GetProperties().Has(g_nameSkinning))
    {
        shaderDefinition.GetProperties().Set(g_nameSkinning);
        shaderDefinitionChanged = true;
    }

    if (shaderDefinitionChanged)
    {
        // Update the shader definition in the attributes
        attributes.SetShaderDefinition(shaderDefinition);
    }
}

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

        mapping.indirectRenderer = new IndirectRenderer();
        mapping.indirectRenderer->Create(renderCollector->drawCallCollectionImpl);
    }

    mapping.drawCallCollection.impl = renderCollector->drawCallCollectionImpl;

    InitObject(rg);

    return rg;
}

template <class Functor, SizeType... Indices>
static inline void ForEachResourceTrackerType_Impl(Span<ResourceTrackerBase*> resourceTrackers, const Functor& functor, std::index_sequence<Indices...>)
{
    (functor(TypeWrapper<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type>(), resourceTrackers[Indices], Indices), ...);
}

template <class Functor>
static inline void ForEachResourceTrackerType(Span<ResourceTrackerBase*> resourceTrackers, const Functor& functor)
{
    ForEachResourceTrackerType_Impl(resourceTrackers, functor, std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());
}

template <class T>
static constexpr SizeType GetTrackedResourceTypeIndex()
{
    return FindTypeElementIndex<T, RenderProxyList::TrackedResourceTypes>::value;
}

template <class Functor, SizeType... Indices>
static inline void ForEachResourceTracker_Impl(Span<ResourceTrackerBase*> resourceTrackers, const Functor& functor, std::index_sequence<Indices...>)
{
    (functor(static_cast<typename TupleElement_Tuple<Indices, RenderProxyList::ResourceTrackerTypes>::Type&>(*resourceTrackers[Indices])), ...);
}

template <class Functor>
static inline void ForEachResourceTracker(Span<ResourceTrackerBase*> resourceTrackers, const Functor& functor)
{
    ForEachResourceTracker_Impl(resourceTrackers, functor, std::make_index_sequence<TupleSize<RenderProxyList::ResourceTrackerTypes>::value>());
}

template <class ElementType, class ProxyType>
static inline void UpdateRefs_Impl(ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>& resourceTracker)
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
        resource->GetObjectHeader_Internal()->IncRefStrong();

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
        resource->GetObjectHeader_Internal()->DecRefStrong();

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
    
    ForEachResourceTracker(renderProxyList.resourceTrackers.ToSpan(), []<class... Args>(Args&&... args) {
        UpdateRefs_Impl(std::forward<Args>(args)...);
    });
}


RenderProxyList::RenderProxyList(bool isShared, bool useRefCounting)
    : isShared(isShared),
      useRefCounting(useRefCounting),
      viewport(Viewport { Vec2u::One(), Vec2i::Zero() }),
      priority(0),
      resourceTrackers {},
      releaseRefsFunctions {}
{
    // initialize the resource trackers
    ForEachResourceTrackerType(resourceTrackers, [this]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>, ResourceTrackerBase*& pResourceTracker, SizeType idx)
        {
            AssertDebug(!pResourceTracker);

            pResourceTracker = new ResourceTrackerType();

            if (this->useRefCounting)
            {
                releaseRefsFunctions[idx] = [](ResourceTrackerBase* resourceTracker) -> void
                {
                    ResourceTrackerType* resourceTrackerCasted = static_cast<ResourceTrackerType*>(resourceTracker);
                    resourceTrackerCasted->Advance(/* clearNextState */ true);
                    
                    HashSet<HypObjectBase*> releasedObjects;
                    
                    const auto releaseRefs = [&](auto& elements)
                    {
                        // Release weak references to all tracked elements
                        for (auto* elem : elements)
                        {
                            AssertDebug(elem != nullptr);
                            
                            HypObjectBase* elemCasted = reinterpret_cast<HypObjectBase*>(elem);
                            AssertDebug(elemCasted->GetObjectHeader_Internal()->GetRefCountStrong() > 0);
                            
                            AssertDebug(!releasedObjects.Contains(elemCasted));
                            
                            elemCasted->GetObjectHeader_Internal()->DecRefStrong();
                            
                            releasedObjects.Insert(elemCasted);
                        }
                    };
                    
                    Assert(!resourceTrackerCasted->GetDiff().NeedsUpdate(),
                           "Update needed when resources are being released! This will lead to improper ref counts!");
                    
                    Array<typename ResourceTrackerType::TElementType> elements;
                    // get current REMOVED elements
                    resourceTrackerCasted->GetRemoved(elements, false);
                    releaseRefs(elements);
                    elements.Clear();
                    
                    // get current ADDED elements
                    resourceTrackerCasted->GetAdded(elements, false);
                    releaseRefs(elements);
                    elements.Clear();
                    
                    // advance, moving the current elements over to the REMOVED bin.
                    resourceTrackerCasted->Advance(/* clearNextState */ true);
                    
                    // release refs on elements that were moved over
                    resourceTrackerCasted->GetRemoved(elements, false);
                    releaseRefs(elements);
                    
                    AssertDebug(resourceTrackerCasted->NumCurrent() == 0);
                };
            }
        });
}

RenderProxyList::~RenderProxyList()
{
#ifdef HYP_DEBUG_MODE
    debugIsDestroyed = true;
#endif

    // Have to dec refs on all objects we hold a strong reference to.

    for (SizeType i = 0; i < resourceTrackers.Size(); i++)
    {
        ResourceTrackerBase* resourceTracker = resourceTrackers[i];
        AssertDebug(resourceTracker != nullptr);

        if (useRefCounting)
        {
            // Release all strong references to tracked elements
            AssertDebug(releaseRefsFunctions[i] != nullptr);
            releaseRefsFunctions[i](resourceTracker);
        }

        delete resourceTracker;
    }

    resourceTrackers = {};
}

void RenderProxyList::BeginWrite()
{
    if (isShared)
    {
        uint64 rwMarkerState = rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
        while (HYP_UNLIKELY(rwMarkerState & readMask))
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
        rwMarker.BitAnd(~writeFlag, MemoryOrder::RELEASE);
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

            if (HYP_UNLIKELY(rwMarkerState & writeFlag))
            {
                HYP_LOG_TEMP("Waiting for write marker to be released. "
                             "If this is occurring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

                rwMarker.Decrement(2, MemoryOrder::RELAXED);

                // spin to wait for write flag to be released
                HYP_WAIT_IDLE();
            }
        }
        while (HYP_UNLIKELY(rwMarkerState & writeFlag));
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
        AssertDebug(rwMarkerState & readMask, "Invalid state, expected read mask to be set when calling EndRead()");

        /// FIXME: If BeginRead() is called on other thread between the check and setting state to CS_DONE,
        /// we could set state to done when it isn't actually.
        if (((rwMarkerState - 2) & readMask) == 0)
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

RenderCollector::RenderCollector()
    : parallelRenderingStateHead(nullptr),
      parallelRenderingStateTail(nullptr),
      drawCallCollectionImpl(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>()),
      renderGroupFlags(RenderGroupFlags::DEFAULT)
{
    previousAttributesBySubtype.Resize(Entity::Class()->GetNumDescendants() + 1);
}

RenderCollector::~RenderCollector()
{
    if (parallelRenderingStateHead)
    {
        ParallelRenderingState* state = parallelRenderingStateHead;

        while (state != nullptr)
        {
            if (state->taskBatch != nullptr)
            {
                state->taskBatch->AwaitCompletion();
                delete state->taskBatch;
            }

            ParallelRenderingState* nextState = state->next;

            delete state;

            state = nextState;
        }
    }

    Clear(true);
}

void RenderCollector::Clear(bool freeMemory)
{
    HYP_SCOPE;

    // Keep the attribs and RenderGroups around so that we can have memory reserved for each slot
    for (auto& mappings : mappingsByBucket)
    {
        for (auto& it : mappings)
        {
            DrawCallCollectionMapping& mapping = it.second;

            for (auto& meshProxies : mapping.meshProxiesBySubtype)
            {
                meshProxies.Clear(/* deletePages */ freeMemory);
            }

            if (freeMemory && mapping.indirectRenderer)
            {
                delete mapping.indirectRenderer;
                mapping.indirectRenderer = nullptr;
            }
        }
    }
}

ParallelRenderingState* RenderCollector::AcquireNextParallelRenderingState()
{
    ParallelRenderingState* curr = parallelRenderingStateTail;

    if (!curr)
    {
        if (!parallelRenderingStateHead)
        {
            parallelRenderingStateHead = new ParallelRenderingState();

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* taskBatch = new TaskBatch();
            taskBatch->pool = &pool;

            parallelRenderingStateHead->taskBatch = taskBatch;
            parallelRenderingStateHead->numBatches = ParallelRenderingState::maxBatches;
        }

        curr = parallelRenderingStateHead;
    }
    else
    {
        if (!curr->next)
        {
            ParallelRenderingState* newParallelRenderingState = new ParallelRenderingState();

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* taskBatch = new TaskBatch();
            taskBatch->pool = &pool;

            newParallelRenderingState->taskBatch = taskBatch;
            newParallelRenderingState->numBatches = ParallelRenderingState::maxBatches;

            curr->next = newParallelRenderingState;
        }

        curr = curr->next;
    }

    parallelRenderingStateTail = curr;

    AssertDebug(curr != nullptr);
    AssertDebug(curr->taskBatch != nullptr);
    AssertDebug(curr->taskBatch->IsCompleted());

    return curr;
}

void RenderCollector::CommitParallelRenderingState(RenderQueue& renderQueue)
{
    ParallelRenderingState* state = parallelRenderingStateHead;

    while (state)
    {
        AssertDebug(state->taskBatch != nullptr);

        state->taskBatch->AwaitCompletion();

        renderQueue.Concat(std::move(state->rootQueue));

        for (RenderQueue& localQueue : state->localQueues)
        {
            renderQueue.Concat(std::move(localQueue));
        }

        // Add render stats counts to the engine's render stats
        for (RenderStatsCounts& counts : state->renderStatsCounts)
        {
            RenderApi_AddRenderStats(counts);

            counts = RenderStatsCounts(); // Reset counts after adding for next use
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

    static const bool isIndirectRenderingEnabled = g_renderBackend->GetRenderConfig().IsIndirectRenderingEnabled();
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

void RenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, uint32 bucketBits)
{
    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView(), "RenderSetup must have a View attached");
    AssertDebug(renderSetup.view->IsReady());

    if (renderSetup.view->GetFlags() & ViewFlags::GBUFFER)
    {
        // Pass NULL framebuffer for GBuffer rendering, as it will be handled by DeferredRenderer outside of this scope.
        ExecuteDrawCalls(frame, renderSetup, FramebufferRef::Null(), bucketBits);
    }
    else
    {
        const FramebufferRef& framebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer();
        AssertDebug(framebuffer != nullptr, "Must have a valid framebuffer for rendering");

        ExecuteDrawCalls(frame, renderSetup, framebuffer, bucketBits);
    }
}

void RenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    Span<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>> groupsView;

    if (bucketBits == 0)
    {
        static constexpr uint32 allBuckets = (1u << RB_MAX) - 1;
        bucketBits = allBuckets; // All buckets
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
            AssertDebug(renderGroup.IsValid());

            const DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

#ifdef HYP_DEBUG_MODE
            // debug checks
            for (const DrawCall& drawCall : drawCallCollection.drawCalls)
            {
                AssertDebug(RenderApi_RetrieveResourceBinding(drawCall.material) != ~0u);
            }
            for (const InstancedDrawCall& drawCall : drawCallCollection.instancedDrawCalls)
            {
                AssertDebug(RenderApi_RetrieveResourceBinding(drawCall.material) != ~0u);
            }
#endif

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

    // Wait for all parallel rendering tasks to finish
    CommitParallelRenderingState(frame->renderQueue);

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

            bool anyMeshProxies = false;

            for (auto &meshProxies : mapping.meshProxiesBySubtype)
            {
                if (meshProxies.Any())
                {
                    anyMeshProxies = true;
                    break;
                }
            }

            if (anyMeshProxies)
            {
                ++it;

                continue;
            }

            if (mapping.indirectRenderer)
            {
                delete mapping.indirectRenderer;
                mapping.indirectRenderer = nullptr;
            }

            it = mappings.Erase(it);
        }
    }
}

uint32 RenderCollector::NumRenderGroups() const
{
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

    Array<ObjId<Entity>> changedIds;
    renderProxyList.GetMeshEntities().GetChanged(changedIds);

    if (changedIds.Any())
    {
        for (const ObjId<Entity>& id : changedIds)
        {
            const int subclassIndex = GetSubclassIndex(TypeId::ForType<Entity>(), id.GetTypeId()) + 1;
            AssertDebug(subclassIndex >= 0 && subclassIndex < previousAttributesBySubtype.Size(),
                "Bad TypeId: {} is not a equal to nor a subclass of Entity", LookupTypeName(id.GetTypeId()));

            RenderableAttributeSet* cachedAttributes = previousAttributesBySubtype[subclassIndex].TryGet(id.ToIndex());
            AssertDebug(cachedAttributes != nullptr);

            // remove from prev
            auto& prevMappings = mappingsByBucket[cachedAttributes->GetMaterialAttributes().bucket];

            auto it = prevMappings.Find(*cachedAttributes);
            Assert(it != prevMappings.End());

            DrawCallCollectionMapping& prevMapping = it->second;
            Assert(subclassIndex >= 0 && subclassIndex < prevMapping.meshProxiesBySubtype.Size());

            auto& prevMeshProxies = prevMapping.meshProxiesBySubtype[subclassIndex];

            const uint32 idx = id.ToIndex();

            RenderProxyMesh* meshProxy = prevMeshProxies.Get(idx);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet newAttributes = GetRenderableAttributesForProxy(*meshProxy, overrideAttributes);
            UpdateRenderableAttributesDynamic(meshProxy, newAttributes);

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

            AssertDebug(meshProxy->mesh.IsValid() && meshProxy->material.IsValid());

            prevMeshProxies.EraseAt(idx);
            newMapping.meshProxiesBySubtype[subclassIndex].Set(idx, meshProxy);

            *cachedAttributes = newAttributes;
        }
    }

    Array<ObjId<Entity>> removed;
    renderProxyList.GetMeshEntities().GetRemoved(removed, false /* includeChanged */);

    Array<ObjId<Entity>> added;
    renderProxyList.GetMeshEntities().GetAdded(added, false /* includeChanged */);

    if (removed.Any())
    {
        for (const ObjId<Entity>& id : removed)
        {
            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            if (!meshProxy)
            {
                continue;
            }

            const int subclassIndex = GetSubclassIndex(TypeId::ForType<Entity>(), id.GetTypeId()) + 1;
            AssertDebug(subclassIndex >= 0 && subclassIndex < previousAttributesBySubtype.Size(),
                "Bad TypeId: {} is not a equal to nor a subclass of Entity", LookupTypeName(id.GetTypeId()));

            const uint32 idx = id.ToIndex();

            AssertDebug(previousAttributesBySubtype[subclassIndex].HasIndex(idx));

            const RenderableAttributeSet& attributes = previousAttributesBySubtype[subclassIndex].Get(idx);

            auto& mappings = mappingsByBucket[attributes.GetMaterialAttributes().bucket];

            auto it = mappings.Find(attributes);
            Assert(it != mappings.End());

            DrawCallCollectionMapping& mapping = it->second;
            Assert(mapping.IsValid());

            auto& meshProxies = mapping.meshProxiesBySubtype[subclassIndex];
            AssertDebug(meshProxies.HasIndex(idx));
            meshProxies.EraseAt(idx);

            previousAttributesBySubtype[idx].EraseAt(idx);
        }
    }

    if (added.Any())
    {
        for (const ObjId<Entity>& id : added)
        {
            const int subclassIndex = GetSubclassIndex(TypeId::ForType<Entity>(), id.GetTypeId()) + 1;
            AssertDebug(subclassIndex >= 0 && subclassIndex < previousAttributesBySubtype.Size(),
                "Bad TypeId: {} is not a equal to nor a subclass of Entity", LookupTypeName(id.GetTypeId()));

            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*meshProxy, overrideAttributes);
            UpdateRenderableAttributesDynamic(meshProxy, attributes);

            // Add proxy to group
            DrawCallCollectionMapping& mapping = mappingsByBucket[attributes.GetMaterialAttributes().bucket][attributes];

            auto& meshProxies = mapping.meshProxiesBySubtype[subclassIndex];

            Handle<RenderGroup>& rg = mapping.renderGroup;

            if (!rg.IsValid())
            {
                rg = CreateRenderGroup(this, mapping, attributes);
            }

            AssertDebug(meshProxy->mesh.IsValid() && meshProxy->material.IsValid());

            const uint32 idx = id.ToIndex();

            meshProxies.Set(idx, const_cast<RenderProxyMesh*>(meshProxy));
            previousAttributesBySubtype[subclassIndex].Set(idx, attributes);
        }
    }
}

// Called at start of frame on render thread
void RenderCollector::BuildDrawCalls(uint32 bucketBits)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    static const bool uniquePerMaterial = g_renderBackend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial();

    if (bucketBits == 0)
    {
        static constexpr uint32 allBuckets = (1u << RB_MAX) - 1;
        bucketBits = allBuckets; // All buckets
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
    //         return int(lhs->first.GetDrawableLayer()) < int(rhs->first.GetDrawableLayer());
    //     });

    for (IteratorType it : iterators)
    {
        const RenderableAttributeSet& attributes = it->first;

        DrawCallCollectionMapping& mapping = it->second;
        AssertDebug(mapping.IsValid());

        DrawCallCollection previousDrawState = std::move(mapping.drawCallCollection);

        DrawCallCollection& drawCallCollection = mapping.drawCallCollection;
        drawCallCollection.impl = drawCallCollectionImpl;
        drawCallCollection.renderGroup = mapping.renderGroup;

        for (auto& meshProxies : mapping.meshProxiesBySubtype)
        {
            for (RenderProxyMesh* meshProxy : meshProxies)
            {
                AssertDebug(meshProxy->mesh.IsValid() && meshProxy->mesh->IsReady());
                AssertDebug(meshProxy->material.IsValid() && meshProxy->material->IsReady());

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

                        drawCallCollection.impl->GetGpuBufferHolder()->MarkDirty(batch->batchIndex);
                    }
                }

                drawCallCollection.PushRenderProxyInstanced(batch, drawCallId, *meshProxy);
            }
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

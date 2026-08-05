/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderGroup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/IndirectDraw.hpp>
#include <Rendering/RenderGroupCache.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/MaterialTextureCache.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/CBufferAllocator.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMapAllocator.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/Scene.hpp>
#include <Scene/View.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/ParticleVolume.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/Sprite.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/ThreadLocalStorage.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Util.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/CVarManager.hpp>

#include <Framework/Threads/RenderWorkerThread.hpp>

#include <Framework/Resources/ResourceTracker.hpp>

#include <Framework/Config/EngineConfig.hpp>

namespace Hyperion {

using namespace Resources;

static constexpr uint32 AllBucketsMask = (1u << NumRenderBuckets) - 1;

extern EngineStatCounter<uint32> g_statDrawCalls;
extern EngineStatCounter<uint32> g_statInstancedDrawCalls;
extern EngineStatCounter<uint32> g_statTriangles;
extern EngineStatCounter<uint32> g_statRenderGroups;

extern EngineStatTimer g_statTotalStallTime;

static EngineStatTimer s_statProxyListReadWait("Rendering/CPU/ProxyListReadWait");

extern CVar<bool> g_cvDepthPrepass;
extern CVar<bool> g_cvPathTracing;

static const Name s_nameShadingType = NAME("SHADING_TYPE");
static const Name s_nameForward = NAME("FORWARD");

static const ShaderPropertyId s_propShadingTypeForward = InternShaderProperty(ShaderProperty(s_nameShadingType, Name(s_nameForward)));

static const ShaderPropertyId s_propForwardClustered = InternShaderProperty(ShaderProperty(NAME("FORWARD_CLUSTERED")));
static const ShaderPropertyId s_propForwardShading = InternShaderProperty(ShaderProperty(NAME("FORWARD_SHADING")));

static HYP_FORCE_INLINE bool IsCubemapShader(StringHash shaderNameHash)
{
    static constexpr StringHash CubemapShaderNames[] = { "DrawCubemap"_sh, "RenderSky"_sh };

    return shaderNameHash != CubemapShaderNames[0]
        && shaderNameHash != CubemapShaderNames[1];
}

static HYP_FORCE_INLINE bool IsGeometryPassShader(StringHash shaderNameHash)
{
    return shaderNameHash == "GeometryPass"_sh;
}

#pragma region ParallelRenderingState

// per-thread CommandRecorder
using ThreadedCommandRecorder = TCommandRecorder<RenderAllocator>;//TCommandRecorder<ThreadAllocator>;

// Holds shared data for ParallelRenderingState instances to reduce memory usage
struct ParallelRenderingState::StateData
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    static constexpr uint32 MaxBatches = NumRendererWorkerThreads;

    FixedArray<ThreadedCommandRecorder, MaxBatches> threadedCommandRecorders;

    StateData()
        : threadedCommandRecorders {}
    {
    }

    StateData(const StateData& other) = delete;
    StateData& operator=(const StateData& other) = delete;

    ~StateData()
    {
        AssertOnThread(g_renderThread);

        Array<Task<void>> tasks;
        tasks.Reserve(MaxBatches);


        AwaitAll(tasks.ToSpan());
    }

    void Reset()
    {
        for (uint32 i = 0; i < MaxBatches; i++)
        {
            threadedCommandRecorders[i].Reset(/* freeMemory */ false);
            //threadedCommandRecorders[i].Reset(/* freeMemory */ true);
        }
    }
};

ParallelRenderingState::ParallelRenderingState(StateData* data, bool ownsData)
    : data(data),
      ownsData(ownsData)
{
    Assert(data != nullptr);
}

ParallelRenderingState::~ParallelRenderingState()
{
    if (ownsData)
    {
        delete data;
        data = nullptr;
    }
}

#pragma endregion ParallelRenderingState

#pragma region GeometryPass

namespace GeometryPass {

static constexpr StringHash DefaultShaderName = "GeometryPass"_sh;

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

static const Pair<MaterialTextureKey, ShaderPropertyId> s_textureProperties[] = {
    { MaterialTextureKey::Diffuse, InternShaderProperty(ShaderProperty(s_nameHasDiffuseMap)) },
    { MaterialTextureKey::Normals, InternShaderProperty(ShaderProperty(s_nameHasNormalMap)) },
    { MaterialTextureKey::Parallax, InternShaderProperty(ShaderProperty(s_nameHasParallaxMap)) },
    { MaterialTextureKey::Metalness, InternShaderProperty(ShaderProperty(s_nameHasMetalnessMap)) },
    { MaterialTextureKey::Roughness, InternShaderProperty(ShaderProperty(s_nameHasRoughnessMap)) }
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
    Mesh* mesh = proxy.mesh;
    AssertDebug(mesh != nullptr);

    Material* material = proxy.material;
    AssertDebug(material != nullptr);

    attributes = proxy.attributes;

    if (overrideAttributes)
    {
        MaterialAttributes newMaterialAttributes = overrideAttributes->GetMaterialAttributes();
        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(newMaterialAttributes);
    }

    MaterialAttributes& mas = attributes.GetMaterialAttributes();

    const StringHash shaderNameHash = mas.shaderName;

    const RenderBucket bucket = mas.bucket;

    const bool hasForwardLighting = (bucket == RenderBucket::Translucent || bucket == RenderBucket::Sky || bucket == RenderBucket::Debug);
    const bool hasLightmaps = (bucket == RenderBucket::Lightmapped);
    const bool isSky = (bucket == RenderBucket::Sky);
    const bool isDebug = (bucket == RenderBucket::Debug);

    const bool hasDeferredLighting = !hasForwardLighting && !hasLightmaps;

    const bool hasInstancing = proxy.enableAutoInstancing || proxy.numInstances;
    const bool hasAlphaDiscard = bool(mas.flags & MAF_ALPHA_DISCARD);
    const bool hasSkinning = proxy.skeleton != nullptr && proxy.skeleton->GetRootBone() != nullptr;

    const bool isPathTracer = g_cvPathTracing.Get();

    // @TODO DRY this up
    // Shouldn't depend on the names of shaders to conditionally handle stuff!
    const bool isCubemap = IsCubemapShader(shaderNameHash);
    const bool isGeometryPass = IsGeometryPassShader(shaderNameHash);

    uint8 stencilReferenceValue = 0;

    if (isSky)
    {
        stencilReferenceValue = SkyStencilMask;
    }
    else if (isDebug)
    {
        // stencilReferenceValue = DebugStencilMask;
    }
    else if (hasLightmaps && !isPathTracer)
    {
        // if lightmap volume is set we need stencil testing
        stencilReferenceValue = GetLightmapStencilValue(proxy.lightmapElementId) & LightmapStencilMask;
    }
    else if (mas.stencilReference & LightmapStencilMask)
    {
        stencilReferenceValue = (mas.stencilReference & ~LightmapStencilMask);
    }

    mas.flags = (stencilReferenceValue != 0) ? (mas.flags | MAF_STENCIL_TEST) : (mas.flags & ~MAF_STENCIL_TEST);
    mas.stencilReference = stencilReferenceValue;

    ShaderPropertySet& shaderProperties = mas.shaderProperties;
    
    shaderProperties.Set(Props::s_propInstancing, hasInstancing);
    shaderProperties.Set(Props::s_propAlphaDiscard, hasAlphaDiscard);
    shaderProperties.Set(Props::s_propSkinning, hasSkinning);

    if (isGeometryPass)
    {
        shaderProperties.Set(Props::s_propShadingTypeDeferred, hasDeferredLighting);
        shaderProperties.Set(Props::s_propShadingTypeForward, hasForwardLighting);
        shaderProperties.Set(Props::s_propShadingTypeLightmapped, hasLightmaps);
    }
}

} // namespace GeometryPass

#pragma endregion GeometryPass

#pragma region DepthPrepass

namespace DepthPrepass {

enum Stage : uint8
{
    DPP_NotActive = 0,
    DPP_InPrepass = 1,
    DPP_InMainPass = 2
};

static inline Stage GetStage(const RenderSetup& renderSetup, bool isDepthPrepass)
{
    // We only consider DPP to be active when we are in the main geometry pass
    // Shadows, env probes etc should not be considered
    static constexpr uint64 PassDataTypeId = CONSTEXPR_TYPE_ID(DeferredPassData);

    if (!renderSetup.passData
        || renderSetup.passData->Id().GetTypeId().Value() != PassDataTypeId
        || !g_cvDepthPrepass.Get())
    {
        return DPP_NotActive;
    }

    return isDepthPrepass ? DPP_InPrepass : DPP_InMainPass;
}

static inline bool ShouldIncludeInPrepass(
    const Viewport& viewport,
    const Mat4f& viewProjMat,
    const RenderProxyMesh& meshProxy)
{
    const MaterialAttributes& mas = meshProxy.attributes.GetMaterialAttributes();
    const RenderBucket bucket = mas.bucket;

    if (!(mas.flags & MAF_DEPTH_WRITE))
    {
        return false;
    }

    if (static_cast<uint8>(bucket) >= static_cast<uint8>(RenderBucket::Translucent))
    {
        return false;
    }

    BoundingBox worldBounds;
    worldBounds.min = meshProxy.bufferData.worldAabbMin;
    worldBounds.max = meshProxy.bufferData.worldAabbMax;

    BoundingBox ndcBounds = viewProjMat * worldBounds;

    const Vec3f ndcMin = ndcBounds.min;
    const Vec3f ndcMax = ndcBounds.max;

    const Vec3f ndcHalfExtent = (ndcMax - ndcMin) * 0.5f;

    const float screenSpaceWidth = ndcHalfExtent.x * float(viewport.extent.x);
    const float screenSpaceHeight = ndcHalfExtent.y * float(viewport.extent.y);

    constexpr float PrepassPixelCutoff = 1.0f;

    return (screenSpaceWidth >= PrepassPixelCutoff
            && screenSpaceHeight >= PrepassPixelCutoff);
}

} // namespace DepthPrepass

#pragma endregion DepthPrepass

static void InitDrawCallCollection(
    const RenderCollector& renderCollector,
    DrawCallCollection& drawCallCollection,
    const RenderableAttributeSet& attributes)
{
    AssertDebug(renderCollector.batchAllocator != nullptr);

    EnumFlags<RenderGroupFlags> renderGroupFlags = renderCollector.renderGroupFlags;

    // Disable occlusion culling for translucent objects
    const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

    if (RenderBucketMask<RenderBucket::Translucent, RenderBucket::Sky, RenderBucket::Debug> & (1u << uint32(rb)))
    {
        renderGroupFlags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
    }

    drawCallCollection.attributes = attributes;
    drawCallCollection.flags = renderGroupFlags;

    if (!RI.GetRenderConfig().indirectRendering)
    {
        renderGroupFlags &= ~RenderGroupFlags::INDIRECT_RENDERING;
    }
    else if (renderGroupFlags & RenderGroupFlags::INDIRECT_RENDERING)
    {
        AssertDebug(drawCallCollection.indirectRenderer == nullptr, "Indirect renderer already exists on mapping");

        drawCallCollection.indirectRenderer = HYP_POOL_NEW(g_renderPool, IndirectRenderer);
        drawCallCollection.indirectRenderer->Create(renderCollector.batchAllocator);
    }

    drawCallCollection.batchAllocator = renderCollector.batchAllocator;

    // If parallel rendering is globally disabled, disable it for this RenderGroup
    if (!RI.GetRenderConfig().parallelRendering)
    {
        drawCallCollection.flags &= ~RenderGroupFlags::PARALLEL_COLLECTION;
    }

    drawCallCollection.isInit = true;
}

template <class AllocatorType, class Functor, size_t... Indices>
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
static constexpr size_t GetTrackedResourceTypeIndex()
{
    return FindTypeElementIndex<T, RenderProxyList::TrackedResourceTypes>::value;
}

template <class AllocatorType, class Functor, size_t... Indices>
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
    const ResourceTrackerDiff& diff = resourceTracker.GetDiff();
    if (!diff.NeedsUpdate())
    {
        return;
    }

    Array<ElementType*, ThreadAllocator> removed;
    resourceTracker.GetRemoved(removed, false);

    Array<ElementType*, ThreadAllocator> added;
    resourceTracker.GetAdded(added, false);

    for (ElementType* resource : added)
    {
        if constexpr (CONSTEXPR_TYPE_ID(ProxyType) != CONSTEXPR_TYPE_ID(NullProxy))
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
        if constexpr (CONSTEXPR_TYPE_ID(ProxyType) != CONSTEXPR_TYPE_ID(NullProxy))
        {
            resourceTracker.RemoveProxy(ObjId<ElementType>(resource->Id()));
        }
    }

    if constexpr (CONSTEXPR_TYPE_ID(ProxyType) != CONSTEXPR_TYPE_ID(NullProxy) && HYP_HAS_METHOD(ElementType, UpdateRenderProxy))
    {
        Array<ObjId<ElementType>, ThreadAllocator> changedIds;
        resourceTracker.GetChanged(changedIds);

        for (const ObjId<ElementType> id : changedIds)
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

    ForEachResourceTracker(
        renderProxyList.resourceTrackers.ToSpan(),
        []<class... Args>(Args&&... args)
        {
            UpdateRefs_Impl(std::forward<Args>(args)...);
        });
}

#pragma region RenderProxyList

RenderProxyList::RenderProxyList(bool isShared, bool useRefCounting)
    : isShared(isShared),
      useRefCounting(useRefCounting),
      priority(0),
      resourceTrackers {}
{
    // initialize the resource trackers
    ForEachResourceTrackerType(
        resourceTrackers.ToSpan(),
        [this]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>, ResourceTrackerBase<AllocatorType>*& pResourceTracker, size_t idx)
        {
            AssertDebug(!pResourceTracker);

            pResourceTracker = new ResourceTrackerType();
        });
}

RenderProxyList::~RenderProxyList()
{
#if HYP_DEBUG_MODE
    int numRenderProxies = 0;

    ForEachResourceTracker(
        resourceTrackers.ToSpan(),
        [&](auto&& resourceTracker)
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

    for (size_t i = 0; i < resourceTrackers.Size(); i++)
    {
        ResourceTrackerBase<AllocatorType>* resourceTracker = resourceTrackers[i];

        delete resourceTracker;
    }

    resourceTrackers = {};
}

void RenderProxyList::BeginWrite()
{
    m_lock.LockWriter();

    AssertDebug(state != CS_READING);
    state = CS_WRITING;

    // advance all trackers to the next state before we write into them.
    // this clears their 'next' bits and sets their 'previous' bits so we can tell what changed.
    ForEachResourceTracker(
        resourceTrackers.ToSpan(),
        [](auto&& resourceTracker)
        {
            resourceTracker.Advance(/* clearNextState */ true);
        });
}

void RenderProxyList::EndWrite()
{
    AssertDebug(state == CS_WRITING);

    if (useRefCounting)
    {
        UpdateRefs(*this);
    }

    ++writeGeneration;
    state = CS_WRITTEN;

    m_lock.UnlockWriter();
}

void RenderProxyList::BeginRead()
{
    bool lockAcquired = false;
    uint32 numSpins = 0;

    {
        ENGINE_STAT_SCOPE(&g_statTotalStallTime);
        ENGINE_STAT_SCOPE(&s_statProxyListReadWait);

        while (!lockAcquired)
        {
            while (!(lockAcquired = m_lock.TryLockReader()) && numSpins++ < 32)
                ;

            if (!lockAcquired && numSpins >= 32)
            {
                HYP_LOG(Rendering, Verbose, "Failed to acquire read lock. "
                                            "If this is occurring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

                // continue and try again, if no pOutSuccess
                ThreadSleep(0);
            }
        }
    }

    AssertDebug(state != CS_WRITING);
    state = CS_READING;
}

void RenderProxyList::EndRead()
{
    AssertDebug(state == CS_READING);

    /// @NOTE: If BeginRead() is called on other thread between the check and setting state to CS_DONE,
    /// we could set state to done when it isn't actually.
    if (m_lock.UnlockReader() == 0)
    {
        state = CS_DONE;
    }
}

void RenderProxyList::ClearAll()
{
    AssertDebug(state == CS_WRITING);

    // Advance again.
    // First advance would be from BeginWrite(), we advance again without tracking any new resources,
    // so the current ones all get disposed.
    ForEachResourceTracker(resourceTrackers.ToSpan(), [](auto&& resourceTracker)
                           {
                               resourceTracker.Advance(/* clearNextState */ true);
                           });
}

#pragma endregion RenderProxyList

#pragma region RenderCollector Helpers

struct PerformRenderingPayloadBase final
{
    RenderSetup renderSetup;
    IndirectRenderer* pIndirectRenderer;
    const DrawCallCollection* pDrawCallCollection;
    DepthPrepass::Stage prepassStage : 3;
};

template <class TCommandRecorder>
struct TPerformRenderingPayload
{
    TCommandRecorder* pCommandRecorder;

    PerformRenderingPayloadBase* pNext;
};

/// Forward shading, NOT clustered
template <class TCommandRecorder>
static void SetForwardShadingConstants(
    const RenderSetup& renderSetup,
    TCommandRecorder& cr,
    uint32& numShaderUniforms)
{
    struct ForwardShadingConstants
    {
        LightShaderData lights[MaxBoundLightsForwardShading];
        ShadowMapData shadowMaps[MaxBoundLightsForwardShading];
        EnvProbeShaderData fallbackProbe;
        uint32 numBoundLights;
    };

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    ForwardShadingConstants* forwardShadingConstants = (ForwardShadingConstants*)RI.cbufferAllocator->Allocate(
        sizeof(ForwardShadingConstants),
        alignof(ForwardShadingConstants),
        cbuffer,
        cbufferOffset);

    Assert(forwardShadingConstants != nullptr);
    Memory::Zero(forwardShadingConstants, sizeof(ForwardShadingConstants));

    cbufferSize = sizeof(ForwardShadingConstants);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    // @TODO Sort by light dist

    for (Light* light : rpl.GetLights())
    {
        if (forwardShadingConstants->numBoundLights >= MaxBoundLightsForwardShading)
        {
            break;
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
        Assert(lightProxy != nullptr);

        const uint32 lightIndex = forwardShadingConstants->numBoundLights++;

        forwardShadingConstants->lights[lightIndex] = lightProxy->bufferData;

        // Set shadow map
        ShadowMapData& currShadowMapData = forwardShadingConstants->shadowMaps[lightIndex];

        const uint32 cascadeIndex = 0;

        View* shadowMapViewDynamic;
        View* shadowMapViewStatic;

        ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
            light,
            renderSetup.view,
            cascadeIndex,
            shadowMapViewDynamic,
            shadowMapViewStatic);

        if (shadowMap != nullptr)
        {
            ShadowMapAtlasElement* atlasElement = shadowMap->GetAtlasElement();
            AssertDebug(atlasElement != nullptr);

            if (!atlasElement)
            {
                continue;
            }

            AssertDebug(shadowMapViewDynamic != nullptr && shadowMapViewDynamic->GetCamera() != nullptr);

            RenderProxyList& rpl = GetConsumerProxyList(shadowMapViewDynamic);
            rpl.BeginRead();
            HYP_DEFER({ rpl.EndRead(); });

            const Mat4f& viewProjMat = rpl.cachedMatrices.viewProj;
            const Mat4f& invProjMat = rpl.cachedMatrices.invProj;

            BoundingBox shadowBoundsNDC;
            shadowBoundsNDC.min = Vec3f(-1.0f);
            shadowBoundsNDC.max = Vec3f(1.0f);

            BoundingBox shadowBoundsWS = viewProjMat.Inverse() * shadowBoundsNDC;

            currShadowMapData.layerIndex = atlasElement->layerIndex;

            currShadowMapData.viewProjMat = viewProjMat;
            currShadowMapData.invProjMat = invProjMat;

            currShadowMapData.aabbMin.x = shadowBoundsWS.min.x;
            currShadowMapData.aabbMin.y = shadowBoundsWS.min.y;
            currShadowMapData.aabbMin.z = shadowBoundsWS.min.z;
            currShadowMapData.aabbMin.w = atlasElement->offsetUV.x;

            currShadowMapData.aabbMax.x = shadowBoundsWS.max.x;
            currShadowMapData.aabbMax.y = shadowBoundsWS.max.y;
            currShadowMapData.aabbMax.z = shadowBoundsWS.max.z;
            currShadowMapData.aabbMax.w = atlasElement->offsetUV.y;

            currShadowMapData.dimensionsScale = Vec4f(Vec2f(atlasElement->dimensions), atlasElement->scale);

            currShadowMapData.splitDistance = 0.0f; // @TODO
        }
    }

    // Set fallback probe
    EnvProbe* fallbackProbe = nullptr;

    for (EnvProbe* envProbe : rpl.GetEnvProbes())
    {
        // if we are drawing an env probe we don't want to use it as fallback!
        if (envProbe == renderSetup.envProbe)
        {
            continue;
        }

        if (!fallbackProbe || uint32(envProbe->GetEnvProbeType()) < uint32(fallbackProbe->GetEnvProbeType()))
        {
            fallbackProbe = envProbe;
        }
    }

    if (fallbackProbe != nullptr)
    {
        RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(fallbackProbe));
        Assert(envProbeProxy != nullptr);

        forwardShadingConstants->fallbackProbe = envProbeProxy->bufferData;
    }
    else
    {
        forwardShadingConstants->fallbackProbe = EnvProbeShaderData {};
    }

    rpl.EndRead();

    cr << SetShaderUniform(numShaderUniforms++, "ForwardShadingConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
}

template <bool UseIndirectRendering, class TCommandRecorder>
static void RenderAll(Frame* frame, const TPerformRenderingPayload<TCommandRecorder>& payload)
{
    TCommandRecorder& cr = *payload.pCommandRecorder;

    const RenderSetup& renderSetup = payload.pNext->renderSetup;
    IndirectRenderer* indirectRenderer = payload.pNext->pIndirectRenderer;
    const DrawCallCollection& drawCallCollection = *payload.pNext->pDrawCallCollection;

    const DepthPrepass::Stage prepassStage = payload.pNext->prepassStage;

    Mat4f viewProjMat;

    RenderProxyCamera* cameraProxy = nullptr;
    if (renderSetup.view != nullptr)
    {
        AssertDebug(drawCallCollection.renderProxyList != nullptr);

        viewProjMat = drawCallCollection.renderProxyList->cachedMatrices.viewProj;

        Camera* camera = renderSetup.view->GetCamera();
        cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(camera));
    }

    if constexpr (UseIndirectRendering)
    {
        AssertDebug(indirectRenderer != nullptr);
    }

    if (drawCallCollection.instancedDrawCalls.Empty() && drawCallCollection.drawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    const RenderableAttributeSet& ras = drawCallCollection.attributes;
    const MaterialAttributes& mas = ras.GetMaterialAttributes();

    const bool isForwardClustered = mas.shaderProperties.Test(s_propShadingTypeForward);
    const bool isForwardNonClustered = mas.shaderProperties.Test(s_propForwardShading);

    uint32 numShaderUniforms = 0;

    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

    const uint32 cbufferBinding = numShaderUniforms++;

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));

    cr << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);

    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    cr << SetShaderUniform(numShaderUniforms++, "LightsBuffer"_sh, RI.namedBuffers[NamedBuffer::Lights]);

    // These two (CurrentLight, CurrentEnvProbe) should be refactored out; they exist for RenderSky shader currently.
    if (renderSetup.light != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, RI.namedBuffers[NamedBuffer::Lights], Resources::GetBinding(renderSetup.light));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, RI.namedBuffers[NamedBuffer::Lights], 0);

    if (renderSetup.envProbe != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], Resources::GetBinding(renderSetup.envProbe));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], 0);

    // Will only be non-null if we are in a deferred rendering pass.
    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);

    if (dpd != nullptr)
    {
        cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));

        if (isForwardClustered)
        {
            // Even though the name (DeferredPassData) suggests otherwise, we are in forward pass, where we use clustered shading

            AssertDebug(dpd->gridTilesBuffer != nullptr && dpd->gridIndexBuffer != nullptr);

            // set cluster grid / index buffers for forward shading pass.
            cr << SetShaderUniform(numShaderUniforms++, "ClusterGridBuffer"_sh, *dpd->gridTilesBuffer);
            cr << SetShaderUniform(numShaderUniforms++, "ClusterIndexBuffer"_sh, *dpd->gridIndexBuffer);
        }
    }

    if (isForwardNonClustered)
    {
        SetForwardShadingConstants(renderSetup, cr, numShaderUniforms);
    }

    static const bool s_useBindlessTextures = RI.GetRenderConfig().bindlessTextures;

    Mesh* prevMesh = nullptr;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferSize = 0;
    size_t cbufferOffset = 0;

    bool isDepthWriteEnabled = bool(mas.flags & MAF_DEPTH_WRITE);

    // Helper lambda to handle depth prepass logic. Returns true if the draw should be skipped.
    auto handleDepthPrepass = [&](const RenderProxyMesh& meshProxy) -> bool
    {
        if (prepassStage == DepthPrepass::DPP_InPrepass)
        {
            if (DepthPrepass::ShouldIncludeInPrepass(renderSetup.viewport, viewProjMat, meshProxy))
            {
                if (!isDepthWriteEnabled)
                {
                    cr << SetDepthWrite(true);
                    isDepthWriteEnabled = true;
                }
            }
            else
            {
                return true; // skip draw
            }
        }
        else if (prepassStage == DepthPrepass::DPP_InMainPass)
        {
            bool shouldEnableDepthWrite;

            if (DepthPrepass::ShouldIncludeInPrepass(renderSetup.viewport, viewProjMat, meshProxy))
            {
                // No depth write; depth would have been written by the prepass
                shouldEnableDepthWrite = false;
            }
            else
            {
                // we need to set depth write based on the mesh proxy's depth write flag
                shouldEnableDepthWrite = bool(mas.flags & MAF_DEPTH_WRITE);
            }

            if (shouldEnableDepthWrite != isDepthWriteEnabled)
            {
                cr << SetDepthWrite(shouldEnableDepthWrite);
                isDepthWriteEnabled = shouldEnableDepthWrite;
            }
        }

        return false;
    };

    const DrawCallStorage& drawCalls = drawCallCollection.drawCalls;

    for (size_t i = 0; i < drawCalls.Size(); i++)
    {
        uint32 numDrawCallUniforms = numShaderUniforms;

        const RenderProxyMesh& meshProxy = *drawCalls.meshProxies[i];

        if (handleDepthPrepass(meshProxy))
        {
            continue;
        }

        const RenderProxyMaterial* materialProxy = static_cast<const RenderProxyMaterial*>(GetRenderProxy(meshProxy.material));
        AssertDebug(materialProxy != nullptr);

        if (HYP_UNLIKELY(!materialProxy))
        {
            continue;
        }

        { // Write constants for the draw
            CBufferAllocator& cba = *RI.cbufferAllocator;
            cba.Write(&meshProxy.bufferData);
            cba.Write(&cameraProxy->bufferData);
            cba.Write(&materialProxy->bufferData);
            cba.Write(&viewProjMat);
            cba.Commit(cbuffer, cbufferOffset, cbufferSize);
        }

        cr << SetShaderUniform(cbufferBinding, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        if (meshProxy.skeleton != nullptr)
        {
            cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh, RI.namedBuffers[NamedBuffer::Skeletons], Resources::GetBinding(meshProxy.skeleton));
        }

        if (!s_useBindlessTextures)
        {
            const uint32 materialBoundIndex = Resources::GetBinding(meshProxy.material);
            AssertDebug(materialBoundIndex != ~0u);

            Span<const GpuImageViewRef> imageViews = RI.materialTextureCache->imageViews.Get(materialBoundIndex);
            AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

            FOR_EACH_BIT(materialProxy->bufferData.textureUsage, bit)
            {
                const StringHash textureUniformName = Material::s_textureNames[bit];

                cr << SetShaderUniform(numDrawCallUniforms++, textureUniformName, imageViews[materialProxy->boundTextureIndices[bit]]);
            }
        }

        cr << CommitDrawState();

        if (!prevMesh || prevMesh != meshProxy.mesh)
        {
            cr << BindVertexBuffer(meshProxy.mesh->GetVertexBuffer());
            cr << BindIndexBuffer(meshProxy.mesh->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(meshProxy.material != nullptr);

            if (!meshProxy.material->GetTexture(MaterialTextureKey::Diffuse))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", meshProxy.material->GetName());
            }
#endif
        }

        if (UseIndirectRendering && drawCalls.drawCommandIndices[i] != ~0u)
        {
            cr << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frame->GetFrameIndex()),
                drawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            cr << DrawIndexed(meshProxy.numIndices, 1);
        }

        prevMesh = meshProxy.mesh;

        if (!drawCallCollection.suppressStats && prepassStage != DepthPrepass::DPP_InPrepass)
        {
            g_statDrawCalls++;
            g_statTriangles += meshProxy.numIndices / 3;
        }
    }

    const InstancedDrawCallStorage& instancedDrawCalls = drawCallCollection.instancedDrawCalls;

    for (size_t i = 0; i < instancedDrawCalls.Size(); i++)
    {
        uint32 numDrawCallUniforms = numShaderUniforms;

        const RenderProxyMesh& meshProxy = *instancedDrawCalls.meshProxies[i];

        if (handleDepthPrepass(meshProxy))
        {
            continue;
        }

        const RenderProxyMaterial* materialProxy = static_cast<const RenderProxyMaterial*>(GetRenderProxy(meshProxy.material));
        AssertDebug(materialProxy != nullptr);

        if (HYP_UNLIKELY(!materialProxy))
        {
            continue;
        }

        { // Write constants for the draw
            CBufferAllocator& cba = *RI.cbufferAllocator;
            cba.Write(&meshProxy.bufferData);
            cba.Write(&cameraProxy->bufferData);
            cba.Write(&materialProxy->bufferData);
            cba.Write(&viewProjMat);
            cba.Commit(cbuffer, cbufferOffset, cbufferSize);
        }

        cr << SetShaderUniform(cbufferBinding, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        EntityInstanceBatch* entityInstanceBatch = instancedDrawCalls.batches[i];
        AssertDebug(entityInstanceBatch != nullptr);

        const StructuredBuffer& entityInstanceBatchBuffer = drawCallCollection.batchAllocator->GetStructuredBuffer();

        cr << SetShaderUniform(numDrawCallUniforms++, "EntityInstanceBatchesBuffer"_sh, entityInstanceBatchBuffer, entityInstanceBatch->batchIndex);

        if (meshProxy.skeleton != nullptr)
        {
            cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh, RI.namedBuffers[NamedBuffer::Skeletons], Resources::GetBinding(meshProxy.skeleton));
        }

        if (!s_useBindlessTextures)
        {
            const uint32 materialBoundIndex = Resources::GetBinding(meshProxy.material);
            AssertDebug(materialBoundIndex != ~0u);

            Span<const GpuImageViewRef> imageViews = RI.materialTextureCache->imageViews.Get(materialBoundIndex);
            AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

            FOR_EACH_BIT(materialProxy->bufferData.textureUsage, bit)
            {
                const StringHash textureUniformName = Material::s_textureNames[bit];

                cr << SetShaderUniform(
                    numDrawCallUniforms++,
                    textureUniformName,
                    imageViews[materialProxy->boundTextureIndices[bit]]);
            }
        }

        cr << CommitDrawState();

        if (!prevMesh || prevMesh != meshProxy.mesh)
        {
            cr << BindVertexBuffer(meshProxy.mesh->GetVertexBuffer());
            cr << BindIndexBuffer(meshProxy.mesh->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(meshProxy.material != nullptr);
            
            if (!meshProxy.material->GetTexture(MaterialTextureKey::Diffuse))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", meshProxy.material->GetName());
            }
#endif
        }

        if (UseIndirectRendering && instancedDrawCalls.drawCommandIndices[i] != ~0u)
        {
            cr << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frame->GetFrameIndex()),
                instancedDrawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            cr << DrawIndexed(meshProxy.numIndices, entityInstanceBatch->numEntities);
        }

        prevMesh = meshProxy.mesh;

        // @NOTE For indirect rendering we would need to read back the number of drawn instances from the GPU to get correct stats.
        if (!drawCallCollection.suppressStats && prepassStage != DepthPrepass::DPP_InPrepass)
        {
            g_statInstancedDrawCalls += entityInstanceBatch->numEntities;
            g_statTriangles += meshProxy.numIndices / 3;
        }
    }
}

template <class TCommandRecorder>
static void PerformRenderingImpl(Frame* frame, const TPerformRenderingPayload<TCommandRecorder>& payload)
{
    TCommandRecorder& cr = *payload.pCommandRecorder;

    const RenderSetup& renderSetup = payload.pNext->renderSetup;
    IndirectRenderer* indirectRenderer = payload.pNext->pIndirectRenderer;
    const DrawCallCollection& drawCallCollection = *payload.pNext->pDrawCallCollection;

    const DepthPrepass::Stage prepassStage = payload.pNext->prepassStage;

    static const bool s_indirectRenderingEnabled = RI.GetRenderConfig().indirectRendering;

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);

    const bool useIndirectRendering = indirectRenderer != nullptr
        && prepassStage != DepthPrepass::DPP_InPrepass
        && s_indirectRenderingEnabled
        && drawCallCollection.flags[RenderGroupFlags::INDIRECT_RENDERING]
        && dpd != nullptr;

    // Not env probes, prepass, etc. Just main drawing pass.
    const bool isNormalDrawingPass = dpd != nullptr
        && prepassStage != DepthPrepass::DPP_InPrepass;

    const RenderableAttributeSet& ras = drawCallCollection.attributes;
    const MaterialAttributes& mas = ras.GetMaterialAttributes();

    AssertDebug(mas.shaderName.IsValid());

    const uint8 stencilReference = mas.stencilReference;

    cr << SetTopology(ras.GetMeshAttributes().topology);
    cr << SetInputLayout(ras.GetMeshAttributes().inputLayout);

    cr << SetCurrentViewport(renderSetup.viewport);

    if (isNormalDrawingPass
        && mas.shaderName == GeometryPass::DefaultShaderName
        && mas.shaderProperties.Test(s_propShadingTypeForward)
        && dpd->gridTilesBuffer != nullptr)
    {
        // If we are in normal drawing (e.g NOT env probes, shadows, etc.) - we use clusters of lights and EnvProbe data
        // Therefore we need to set FORWARD_CLUSTERED prop to true to choose the correct variant.
        ShaderPropertySet shaderProperties = mas.shaderProperties;
        shaderProperties.Add(s_propForwardClustered);

        cr << SetCurrentShader(ShaderDesc(mas.shaderName, shaderProperties));
    }
    else
    {
        cr << SetCurrentShader(ShaderDesc(mas.shaderName, mas.shaderProperties));
    }

    cr << SetFillMode(mas.fillMode);
    cr << SetFaceCullMode(mas.cullFaces);

    cr << SetCurrentBlendFunction(mas.blendFunction);

    cr << SetDepthTest(bool(mas.flags & MAF_DEPTH_TEST));
    cr << SetDepthClamp(bool(mas.flags & MAF_DEPTH_CLAMP));
    cr << SetDepthWrite(bool(mas.flags & MAF_DEPTH_WRITE));

    if (mas.flags & MAF_DEPTH_BIAS)
    {
        cr << SetDepthBias(mas.depthBias, mas.depthBiasSlope);
    }

    cr << SetStencilTest(bool(mas.flags & MAF_STENCIL_TEST));
    cr << SetStencilFunction(mas.stencilFunction);

    if (stencilReference != 0)
    {
        // apply stencil state before render (write)
        cr << SetStencilState(stencilReference, 0x0, 0xFF);
    }

    if (useIndirectRendering)
    {
        RenderAll<true>(frame, payload);
    }
    else
    {
        RenderAll<false>(frame, payload);
    }
}

#pragma endregion RenderCollector Helpers

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

    Payload** ppPayload = DeletionQueue::GetInstance().AllocCustom<Payload*>(
        [](void* ptr)
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
    : parallelRenderingStates {},
      batchAllocator(nullptr),
      renderGroupFlags(RenderGroupFlags::DEFAULT),
      isFallback(false)
{
}

RenderCollector::~RenderCollector()
{
    if (isFallback)
    {
        return;
    }

    DeleteOnRenderThread(
        [attrs = std::move(previousAttributes), m = std::move(mappingsByBucket), states = parallelRenderingStates]() mutable
        {
            attrs.Clear(/* freeMemory */ true);

            // Collect command recorders.
            for (auto& list : states)
            {
                if (list.head)
                {
                    ParallelRenderingState* state = list.head;

                    Array<ParallelRenderingState*> toDelete;

                    while (state != nullptr)
                    {
                        if (state->taskBatch != nullptr)
                        {
                            state->taskBatch->AwaitCompletion();

                            delete state->taskBatch;
                        }

                        ParallelRenderingState* nextState = state->next;

                        AssertDebug(!toDelete.Contains(state));

                        toDelete.PushBack(state);

                        state = nextState;
                    }

                    Set<ParallelRenderingState::StateData*> deletedSharedData;

                    for (size_t i = toDelete.Size(); i > 0; i--)
                    {
                        if (toDelete[i - 1]->ownsData)
                        {
                            AssertDebug(!deletedSharedData.Contains(toDelete[i - 1]->data));

                            deletedSharedData.Add(toDelete[i - 1]->data);
                        }

                        delete toDelete[i - 1];
                    }
                }
            }

            for (auto& mappings : m)
            {
                for (DrawCallCollection& drawCallCollection : mappings)
                {
                    drawCallCollection.meshProxies.Clear(/* freeMemory */ true);

                    if (drawCallCollection.indirectRenderer)
                    {
                        PoolDelete(*g_renderPool, drawCallCollection.indirectRenderer);
                        drawCallCollection.indirectRenderer = nullptr;
                    }
                }

                mappings.Clear();
            }
        });

    parallelRenderingStates = {};
}

size_t RenderCollector::NumDrawCallsCollected() const
{
    size_t numDrawCalls = 0;

    for (const auto& mappings : mappingsByBucket)
    {
        for (const DrawCallCollection& drawCallCollection : mappings)
        {
            numDrawCalls += drawCallCollection.drawCalls.Size()
                + drawCallCollection.instancedDrawCalls.Size();
        }
    }

    return numDrawCalls;
}

void RenderCollector::Clear(bool freeMemory)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    for (auto& mappings : mappingsByBucket)
    {
        for (DrawCallCollection& drawCallCollection : mappings)
        {
            drawCallCollection.meshProxies.Clear(/* freeMemory */ freeMemory);

            if (freeMemory)
            {
                if (drawCallCollection.indirectRenderer)
                {
                    PoolDelete(*g_renderPool, drawCallCollection.indirectRenderer);
                    drawCallCollection.indirectRenderer = nullptr;
                }
            }
        }

        if (freeMemory)
        {
            mappings.Clear();
        }
    }
}

HYP_NODISCARD ParallelRenderingState* RenderCollector::AcquireNextParallelRenderingState(uint8 index)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    ParallelRenderingState*& parallelRenderingStateHead = parallelRenderingStates[index].head;
    ParallelRenderingState*& parallelRenderingStateTail = parallelRenderingStates[index].tail;

    ParallelRenderingState* curr = parallelRenderingStateTail;

    if (!curr)
    {
        if (!parallelRenderingStateHead)
        {
            parallelRenderingStateHead = new ParallelRenderingState(new ParallelRenderingState::StateData, true);

            TaskBatch* taskBatch = new TaskBatch;
            taskBatch->pool = g_renderWorkerThreadPool;

            parallelRenderingStateHead->taskBatch = taskBatch;
        }

        curr = parallelRenderingStateHead;
    }
    else
    {
        ParallelRenderingState*& next = curr->next;

        if (!next)
        {
            ParallelRenderingState* newParallelRenderingState = new ParallelRenderingState(new ParallelRenderingState::StateData, true);

            TaskBatch* taskBatch = new TaskBatch;
            taskBatch->pool = g_renderWorkerThreadPool;

            newParallelRenderingState->taskBatch = taskBatch;

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

void RenderCollector::Commit(CommandRecorder& cr, uint8 index)
{
    HYP_SCOPE;

    ParallelRenderingState*& parallelRenderingStateHead = parallelRenderingStates[index].head;
    ParallelRenderingState*& parallelRenderingStateTail = parallelRenderingStates[index].tail;

    ParallelRenderingState* state = parallelRenderingStateHead;

    if (!state)
    {
        // non threaded -- reset draw states

        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
        cr << SetTopology(TOP_TRIANGLES);
        cr << SetFillMode(FM_FILL);
        cr << SetFaceCullMode(FCM_BACK);
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        cr << SetDepthCompareOp(DCO_LESS);
        cr << SetDepthBias(0, 0.0f);
        cr << SetDepthClamp(false);
        cr << SetStencilTest(false);

        return;
    }

    while (state)
    {
        state->renderThreadRecorder.Done();
        cr.Concat(state->renderThreadRecorder);
        state->renderThreadRecorder.Reset(/* freeMemory */ false);

        AssertDebug(state->taskBatch != nullptr);
        state->taskBatch->AwaitCompletion();

        for (uint32 i = 0; i < ParallelRenderingState::StateData::MaxBatches; i++)
        {
            state->data->threadedCommandRecorders[i].Done();
            cr.Concat(state->data->threadedCommandRecorders[i]);
        }

        // end threaded commands -- reset draw states
        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
        cr << SetTopology(TOP_TRIANGLES);
        cr << SetFillMode(FM_FILL);
        cr << SetFaceCullMode(FCM_BACK);
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        cr << SetDepthCompareOp(DCO_LESS);
        cr << SetDepthBias(0, 0.0f);
        cr << SetDepthClamp(false);
        cr << SetStencilTest(false);

        state->data->Reset();
        state->taskBatch->ResetState();

        state = state->next;
    }

    parallelRenderingStateTail = nullptr;
}

void RenderCollector::PerformOcclusionCulling(Frame* frame, const RenderSetup& renderSetup, uint32 bucketBits)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData to perform occlusion culling");

    static const bool s_isIndirectRenderingEnabled = RI.GetRenderConfig().indirectRendering;
    const bool performOcclusionCulling = s_isIndirectRenderingEnabled;

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

            for (DrawCallCollection& drawCallCollection : mappings)
            {
                AssertDebug(drawCallCollection.isInit);

                IndirectRenderer* indirectRenderer = drawCallCollection.indirectRenderer;

                if (drawCallCollection.flags & RenderGroupFlags::OCCLUSION_CULLING)
                {
                    AssertDebug((drawCallCollection.flags & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));
                    AssertDebug(indirectRenderer != nullptr);

                    indirectRenderer->GetDrawState().ResetDrawState();

                    indirectRenderer->PushDrawCallsToIndirectState(frame->cr, drawCallCollection);
                    indirectRenderer->ExecuteCullShaderInBatches(frame->cr, renderSetup);
                }
            }
        }
    }
}

bool RenderCollector::BeginRecordDrawCalls(
    Frame* frame,
    const RenderSetup& renderSetup,
    uint32 bucketBits,
    bool isDepthPrepass)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (bucketBits == 0)
    {
        bucketBits = AllBucketsMask;
    }

    const DepthPrepass::Stage prepassStage = DepthPrepass::GetStage(renderSetup, isDepthPrepass);

    Span<BinnedDrawCallCollections> groupsView;

    if (ByteUtil::BitCount(bucketBits) == 1)
    {
        const uint32 renderBucketIndex = uint32(MathUtil::FastLog2_Pow2(bucketBits));

        auto& mappings = mappingsByBucket[renderBucketIndex];

        if (mappings.Empty())
        {
            return false;
        }

        groupsView = { &mappings, 1 };
    }
    else
    {
        bool allEmpty = true;

        FOR_EACH_BIT(bucketBits, bit)
        {
            if (mappingsByBucket[bit].Any())
            {
                allEmpty = false;
                break;
            }
        }

        if (allEmpty)
        {
            // Nothing to record draw calls for.
            return false;
        }

        groupsView = mappingsByBucket.ToSpan();
    }

    bool anyEnqueued = false;

    for (size_t i = 0; i < groupsView.Size(); i++)
    {
        size_t mappingIndex = std::distance(mappingsByBucket.Begin(), &groupsView[i]);
        AssertDebug(mappingIndex < mappingsByBucket.Size());

        if (!(bucketBits & (1u << uint32(mappingIndex))))
        {
            continue;
        }

        auto& mappings = groupsView[i];

        ParallelRenderingState* parallelRenderingState = nullptr;

        for (DrawCallCollection& drawCallCollection : mappings)
        {
            AssertDebug(drawCallCollection.isInit);

            if (!(drawCallCollection.flags & RenderGroupFlags::PARALLEL_COLLECTION))
            {
                continue;
            }

            AssertDebug(drawCallCollection.parallelRenderingState == nullptr);

            if (!parallelRenderingState)
            {
                parallelRenderingState = AcquireNextParallelRenderingState(uint8(mappingIndex));

                AssertDebug(parallelRenderingState != nullptr);
            }

            drawCallCollection.parallelRenderingState = parallelRenderingState;

            AssertDebug(drawCallCollection.parallelRenderingState->taskBatch != nullptr);

            struct PerformRenderingFunctor
            {
                RenderCollector* pRenderCollector;
                Frame* pFrame;
                PerformRenderingPayloadBase payload;

                void operator()()
                {
                    pRenderCollector->PerformRendering(pFrame, payload);
                }
            };

            PerformRenderingFunctor functor {};
            functor.pRenderCollector = this;
            functor.pFrame = frame;
            functor.payload.renderSetup = renderSetup;
            functor.payload.pDrawCallCollection = &drawCallCollection;
            functor.payload.prepassStage = prepassStage;
            functor.payload.pIndirectRenderer = isDepthPrepass ? nullptr : drawCallCollection.indirectRenderer;

            parallelRenderingState->taskBatch->AddTask(functor);

            anyEnqueued = true;
        }

        if (parallelRenderingState != nullptr)
        {
            TaskSystem::GetInstance().EnqueueBatch(parallelRenderingState->taskBatch);
        }
    }

    return anyEnqueued;
}

void RenderCollector::ExecuteDrawCalls(
    Frame* frame,
    const RenderSetup& renderSetup,
    uint32 bucketBits,
    bool isDepthPrepass)
{
    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.view->IsReady());

    if (renderSetup.view->GetFlags() & ViewFlags::GBUFFER)
    {
        // Pass NULL framebuffer for GBuffer rendering, as it will be handled by DeferredPass outside of this scope.
        ExecuteDrawCalls(frame, renderSetup, nullptr, bucketBits, isDepthPrepass);
    }
    else
    {
        // Use the framebuffer on the RenderSetup
        Framebuffer* framebuffer = renderSetup.framebuffer;

        // If none on the RenderSetup, use the View's output target framebuffer, if available.
        if (!framebuffer)
        {
            framebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer();
        }

        // If no framebuffer is bound due to the above logic, we assume that the framebuffer was bound at a
        // higher level
        //
        // Note that this does have valid uses, that's why there is no assertion, check etc. here.
        // For example, in FinalPass we bind the framebuffer that writes directly to the backbuffer,
        // and draw UI into that.

        ExecuteDrawCalls(frame, renderSetup, framebuffer, bucketBits, isDepthPrepass);
    }
}

void RenderCollector::ExecuteDrawCalls(
    Frame* frame,
    const RenderSetup& renderSetup,
    Framebuffer* framebuffer,
    uint32 bucketBits,
    bool isDepthPrepass)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const DepthPrepass::Stage prepassStage = DepthPrepass::GetStage(renderSetup, isDepthPrepass);

    if (bucketBits == 0)
    {
        bucketBits = AllBucketsMask;
    }

    Span<BinnedDrawCallCollections> groupsView;

    // If only one bit is set, we can skip the loop by directly accessing the RenderGroup
    if (ByteUtil::BitCount(bucketBits) == 1)
    {
        const uint32 renderBucketIndex = uint32(MathUtil::FastLog2_Pow2(bucketBits));

        auto& mappings = mappingsByBucket[renderBucketIndex];

        if (mappings.Empty())
        {
            return;
        }

        groupsView = { &mappings, 1 };
    }
    else
    {
        bool allEmpty = true;

        FOR_EACH_BIT(bucketBits, bit)
        {
            if (mappingsByBucket[bit].Any())
            {
                allEmpty = false;
                break;
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
        frame->cr << SetCurrentFramebuffer(framebuffer);
    }

    // set these to null after rendering
    static Array<ParallelRenderingState**> s_parallelRenderingStatesToNull;
    s_parallelRenderingStatesToNull.Reserve(32);

    for (auto& mappings : groupsView)
    {
        for (DrawCallCollection& drawCallCollection : mappings)
        {
            AssertDebug(drawCallCollection.batchAllocator != nullptr && drawCallCollection.isInit);

            const RenderBucket rb = drawCallCollection.attributes.GetMaterialAttributes().bucket;

            if (!(bucketBits & (1u << uint32(rb))))
            {
                continue;
            }

            AssertDebug(drawCallCollection.isInit);

            bool shouldExecuteSynchronously = true;

            if (drawCallCollection.flags & RenderGroupFlags::PARALLEL_COLLECTION)
            {
                if (drawCallCollection.parallelRenderingState != nullptr)
                {
                    // If BeginRecordDrawCalls() was used, parallelRenderingState would be non-null,
                    // therefore we skip enqueueing teh task batch if that is set and instead just
                    // will wait on the existing one

                    shouldExecuteSynchronously = false;
                }
                else
                {
                    HYP_LOG_ONCE(Rendering, Warning, "Executing draw calls for bucket {} SYNCHRONOUSLY! BeginRecordDrawCalls() should be used to ensure parallel rendering is used.", uint32(rb));
                }
            }

            if (shouldExecuteSynchronously)
            {
                PerformRenderingPayloadBase payload {};
                payload.renderSetup = renderSetup;
                payload.prepassStage = prepassStage;
                payload.pDrawCallCollection = &drawCallCollection;
                payload.pIndirectRenderer = isDepthPrepass ? nullptr : drawCallCollection.indirectRenderer;

                PerformRendering(frame, payload);
            }
            else
            {
                // Set null for next frame
                s_parallelRenderingStatesToNull.PushBack(&drawCallCollection.parallelRenderingState);
            }
        }
    }

    FOR_EACH_BIT(bucketBits, bit)
    {
        Commit(frame->cr, uint8(bit));
    }

    if (s_parallelRenderingStatesToNull.Any())
    {
        for (ParallelRenderingState** pp : s_parallelRenderingStatesToNull)
        {
            *pp = nullptr;
        }

        s_parallelRenderingStatesToNull.Resize(0);
    }

    if (framebuffer)
    {
        frame->cr << SetCurrentFramebuffer(nullptr);
    }
}

// Called at start of frame on render thread
void RenderCollector::CollectRenderables(uint32 bucketBits)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!batchAllocator)
    {
        batchAllocator = GetOrCreateEntityBatchAllocator<MeshEntityInstanceBatch>();
    }

    if (bucketBits == 0)
    {
        bucketBits = AllBucketsMask;
    }

    using IteratorType = BinnedDrawCallCollections::Iterator;

    Array<IteratorType, RenderTempAllocator> iterators;

    FOR_EACH_BIT(bucketBits, bitIndex)
    {
        AssertDebug(bitIndex < mappingsByBucket.Size());

        BinnedDrawCallCollections& mappings = mappingsByBucket[bitIndex];

        if (mappings.Empty())
        {
            continue;
        }

        iterators.Reserve(iterators.Size() + mappings.Count());

        for (auto it = mappings.Begin(); it != mappings.End(); ++it)
        {
            iterators.PushBack(it);
        }
    }

    if (iterators.Empty())
    {
        return;
    }

    for (const IteratorType& it : iterators)
    {
        DrawCallCollection& drawCallCollection = *it;
        AssertDebug(drawCallCollection.batchAllocator != nullptr && drawCallCollection.isInit);

        DrawCallCollection prevDrawCallCollection;
        drawCallCollection.TakeDrawCalls(prevDrawCallCollection);

        for (RenderProxyMesh* meshProxy : drawCallCollection.meshProxies)
        {
            AssertDebug(Resources::GetBinding(meshProxy->mesh) != Resources::InvalidBinding);

            AssertDebug(meshProxy->mesh != nullptr
                        && meshProxy->mesh->GetVertexBuffer() != nullptr
                        && meshProxy->mesh->GetIndexBuffer() != nullptr);

            AssertDebug(meshProxy->material != nullptr);

            DrawCallID drawCallId { meshProxy->mesh->Id(), meshProxy->material->Id() };

            if (!meshProxy->enableAutoInstancing && !meshProxy->numInstances)
            {
                drawCallCollection.PushDrawCall(drawCallId, meshProxy);

                continue;
            }

            EntityInstanceBatch* batch = nullptr;

            if (prevDrawCallCollection.batchAllocator != nullptr && prevDrawCallCollection.isInit)
            {
                // take a batch for reuse if a draw call was using one
                if ((batch = prevDrawCallCollection.RecycleDrawBatch(drawCallId)) != nullptr)
                {
                    const uint32 batchIndex = batch->batchIndex;
                    AssertDebug(batchIndex != ~0u);

                    // `batch` points to a BatchType instance (e.g. MeshEntityInstanceBatch) that is larger than
                    // EntityInstanceBatch. Resetting via `*batch = EntityInstanceBatch { batchIndex }` only clears
                    // the EntityInstanceBatch base subobject and leaves derived-only fields (e.g. previousTransforms)
                    // stale from whatever draw call previously owned this recycled batch slot.
                    Memory::Zero(batch, prevDrawCallCollection.batchAllocator->GetStructSize());
                    batch->batchIndex = batchIndex;
                }
            }

            drawCallCollection.PushInstancedDrawCall(drawCallId, meshProxy, batch);
        }

        if (prevDrawCallCollection.batchAllocator != nullptr && prevDrawCallCollection.isInit)
        {
            // Any draw calls that were not reused from the previous state, clear them out and release batch indices.
            prevDrawCallCollection.ResetDrawCalls();
        }
    }

    if (batchAllocator != nullptr)
    {
        batchAllocator->Flush();
    }
}

void RenderCollector::RemoveEmptyRenderGroups()
{
    HYP_SCOPE;

    for (auto& mappings : mappingsByBucket)
    {
        for (auto it = mappings.Begin(); it != mappings.End();)
        {
            DrawCallCollection& drawCallCollection = *it;
            AssertDebug(drawCallCollection.batchAllocator != nullptr && drawCallCollection.isInit);

            if (drawCallCollection.meshProxies.Any())
            {
                ++it; // skip non-empty

                continue;
            }

            if (drawCallCollection.indirectRenderer)
            {
                PoolDelete(*g_renderPool, drawCallCollection.indirectRenderer);
                drawCallCollection.indirectRenderer = nullptr;
            }

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
        for (const DrawCallCollection& drawCallCollection : mappings)
        {
            AssertDebug(drawCallCollection.batchAllocator != nullptr && drawCallCollection.isInit);

            ++count;
        }
    }

    return count;
}

void RenderCollector::BuildRenderGroups(View* view, RenderProxyList& renderProxyList)
{
    HYP_SCOPE;

    AssertDebug(view != nullptr);
    AssertDebug(renderProxyList.state == RenderProxyList::CS_READING);

#if HYP_DEBUG_MODE
    HYP_DEFER({ lastFrameDEBUG = GetFrameCounter(); });
#endif // HYP_DEBUG_MODE

    RenderGroupCache& attributeRegistry = *RI.renderGroupCache;

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
        for (const ObjId<Entity> id : changedIds)
        {
            RenderableAttributeHandle* cachedHandle = previousAttributes.TryGet(id.ToIndex());
            AssertDebug(cachedHandle != nullptr && cachedHandle->IsValid());

            if (!cachedHandle || !cachedHandle->IsValid())
            {
                continue;
            }

            const RenderBucket prevBucket = cachedHandle->GetBucket();
            const uint32 prevIndex = cachedHandle->GetIndex();

            // remove from prev
            auto& prevMappings = mappingsByBucket[uint32(prevBucket)];
            AssertDebug(prevMappings.HasIndex(prevIndex));

            DrawCallCollection* prevDrawCallCollection = &prevMappings.Get(prevIndex);

            RenderProxyMesh* meshProxy = prevDrawCallCollection->meshProxies.Get(id.ToIndex());
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet newAttributes;
            GeometryPass::BuildAttributes(*meshProxy, newAttributes, overrideAttributes);

            AssertDebug(newAttributes.GetMeshAttributes().inputLayout.mask != 0);

            const RenderBucket newBucket = newAttributes.GetMaterialAttributes().bucket;
            const RenderableAttributeHandle newHandle = attributeRegistry.GetOrCreate(newAttributes);

            if (newHandle == *cachedHandle)
            {
                // not changed, skip
                continue;
            }

            prevDrawCallCollection->meshProxies.EraseAt(id.ToIndex());
            prevDrawCallCollection = nullptr;

            // Add proxy to group
            DrawCallCollection& newDrawCallCollection = mappingsByBucket[uint32(newBucket)][newHandle.GetIndex()];

            AssertDebug(newDrawCallCollection.parallelRenderingState == nullptr); // not handled properly? should be set to null after awaited

            if (!newDrawCallCollection.isInit)
            {
                InitDrawCallCollection(*this, newDrawCallCollection, newAttributes);
                newDrawCallCollection.renderProxyList = &renderProxyList;
            }

            AssertDebug(meshProxy->mesh != nullptr && meshProxy->material != nullptr);

            newDrawCallCollection.meshProxies.Set(id.ToIndex(), meshProxy);

            *cachedHandle = newHandle;
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
            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            if (!meshProxy)
            {
                continue;
            }

            const uint32 idx = id.ToIndex();

            const RenderableAttributeHandle* attributeHandle = previousAttributes.TryGet(idx);

            if (HYP_UNLIKELY(!attributeHandle))
            {
                // nothing to remove here - Skip it
                continue;
            }

            const RenderBucket bucket = attributeHandle->GetBucket();
            const uint32 index = attributeHandle->GetIndex();

            auto& mappings = mappingsByBucket[uint32(bucket)];
            AssertDebug(mappings.HasIndex(index));

            DrawCallCollection& drawCallCollection = mappings.Get(index);
            Assert(drawCallCollection.batchAllocator != nullptr && drawCallCollection.isInit);

            AssertDebug(drawCallCollection.meshProxies.HasIndex(idx));
            drawCallCollection.meshProxies.EraseAt(idx);

            previousAttributes.EraseAt(idx);
        }
    }

    if (added.Any())
    {
        for (const ObjId<Entity>& id : added)
        {
            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet attributes;
            GeometryPass::BuildAttributes(*meshProxy, attributes, overrideAttributes);

            const RenderableAttributeHandle handle = attributeRegistry.GetOrCreate(attributes);

            // Add proxy to group
            DrawCallCollection& drawCallCollection = mappingsByBucket[uint32(handle.GetBucket())][handle.GetIndex()];

            if (!drawCallCollection.isInit)
            {
                InitDrawCallCollection(*this, drawCallCollection, attributes);
                drawCallCollection.renderProxyList = &renderProxyList;
            }

            const uint32 idx = id.ToIndex();

            drawCallCollection.meshProxies.Set(idx, const_cast<RenderProxyMesh*>(meshProxy));
            previousAttributes.Set(idx, handle);
        }
    }
}

void RenderCollector::PerformRendering(Frame* frame, PerformRenderingPayloadBase& payload)
{
    HYP_SCOPE;

    const RenderSetup& renderSetup = payload.renderSetup;
    const DrawCallCollection& drawCallCollection = *payload.pDrawCallCollection;

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData for rendering!");

    if (drawCallCollection.drawCalls.Empty() && drawCallCollection.instancedDrawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    // Render thread index starts at 0 for main render thread, worker threads are 1,2,...
    const int32 renderThreadIndex = CurrentRenderThreadIndex();

    if (drawCallCollection.parallelRenderingState != nullptr && renderThreadIndex >= 1)
    {
        AssertDebug(drawCallCollection.flags & RenderGroupFlags::PARALLEL_COLLECTION);

        auto& cr = drawCallCollection.parallelRenderingState->data->threadedCommandRecorders[renderThreadIndex - 1];

        TPerformRenderingPayload payloadNext { &cr, &payload };

        PerformRenderingImpl(frame, payloadNext);
    }
    else
    {
        TPerformRenderingPayload payloadNext { &frame->cr, &payload };

        PerformRenderingImpl(frame, payloadNext);
    }

    g_statRenderGroups++;
}

void RenderCollector::PerformRendering(Frame* frame, const RenderSetup& renderSetup, const DrawCallCollection& drawCallCollection)
{
    PerformRenderingPayloadBase payload {};
    payload.renderSetup = renderSetup;
    payload.pDrawCallCollection = &drawCallCollection;
    payload.pIndirectRenderer = nullptr;

    PerformRendering(frame, payload);
}

#pragma endregion RenderCollector

} // namespace Hyperion

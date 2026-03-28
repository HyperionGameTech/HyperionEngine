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
#include <rendering/MaterialTextureCache.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ConstantsAllocator.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <engine/resources/ResourceTracker.hpp>
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

#include <engine/config/EngineConfig.hpp>

namespace Hyperion {

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

extern uint32 CurrentRenderThreadIndex();

static constexpr uint32 AllBucketsMask = (1u << NumRenderBuckets) - 1;

extern EngineStatCounter<uint32> g_statDrawCalls;
extern EngineStatCounter<uint32> g_statInstancedDrawCalls;
extern EngineStatCounter<uint32> g_statTriangles;
extern EngineStatCounter<uint32> g_statRenderGroups;

static const Name s_nameShadingType = NAME("SHADING_TYPE");
static const Name s_nameForward = NAME("FORWARD");

static const ShaderPropertyId s_propShadingTypeForward = InternShaderProperty(ShaderProperty(s_nameShadingType, Name(s_nameForward)));

#pragma region ParallelRenderingState

// Holds shared data for ParallelRenderingState instances to reduce memory usage
struct ParallelRenderingState_Shared
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    static constexpr uint32 MaxBatches = ParallelRenderingState::MaxBatches;

    using LocalQueue = ParallelRenderingState::LocalQueue;

    FixedArray<LocalQueue*, MaxBatches> threadLocalRecorders;

    ParallelRenderingState_Shared()
        : threadLocalRecorders {}
    {
        AssertOnThread(g_renderThread);

        for (uint32 i = 0; i < MaxBatches; i++)
        {
            threadLocalRecorders[i] = PoolNew<LocalQueue>(*g_renderPool);
        }
    }

    ~ParallelRenderingState_Shared()
    {
        AssertOnThread(g_renderThread);

        for (uint32 i = 0; i < MaxBatches; i++)
        {
            if (threadLocalRecorders[i])
            {
                PoolDelete(*g_renderPool, threadLocalRecorders[i]);
            }
        }
    }

    void Reset()
    {
        for (uint32 i = 0; i < ParallelRenderingState::MaxBatches; i++)
        {
            // don't free memory; each queue uses thread-local memory allocators
            threadLocalRecorders[i]->Reset(/* freeMemory */ false);
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
        threadLocalRecorders[i] = sharedData->threadLocalRecorders[i];
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
static const Name s_nameMaxLights = NAME("MAX_LIGHTS");

static const Name s_nameHasDiffuseMap = NAME("HAS_DIFFUSE_MAP");
static const Name s_nameHasNormalMap = NAME("HAS_NORMAL_MAP");
static const Name s_nameHasParallaxMap = NAME("HAS_PARALLAX_MAP");
static const Name s_nameHasMetalnessMap = NAME("HAS_METALNESS_MAP");
static const Name s_nameHasRoughnessMap = NAME("HAS_ROUGHNESS_MAP");

/// Property interning

static const ShaderPropertyId s_propInstancing = InternShaderProperty(ShaderProperty(s_nameInstancing));
static const ShaderPropertyId s_propAlphaDiscard = InternShaderProperty(ShaderProperty(s_nameAlphaDiscard));
static const ShaderPropertyId s_propSkinning = InternShaderProperty(ShaderProperty(s_nameSkinning));
static const ShaderPropertyId s_propMaxLights = InternShaderProperty(ShaderProperty(s_nameMaxLights, int(MaxBoundLightsForwardShading)));

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

    const bool hasInstancing = proxy.enableAutoInstancing || proxy.numInstances > 1;
    const bool hasForwardLighting = attributes.GetMaterialAttributes().bucket == RenderBucket::Translucent;
    const bool hasLightmaps = attributes.GetMaterialAttributes().bucket == RenderBucket::Lightmapped;
    const bool hasDeferredLighting = !hasForwardLighting && !hasLightmaps;
    const bool hasAlphaDiscard = bool(attributes.GetMaterialAttributes().flags & MAF_ALPHA_DISCARD);
    const bool hasSkinning = proxy.skeleton != nullptr && proxy.skeleton->GetRootBone() != nullptr;

    const bool isPathTracer = GetEngineConfig().Get("Rendering.RayTracing.PathTracing.Enabled").ToBool();

    // if lightmap volume is set we need stencil testing
    if (hasLightmaps && !isPathTracer)
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
            // set 'MAX_LIGHTS' if using forward shading
            newShaderProperties.Set(Props::s_propMaxLights, hasForwardLighting);
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

static RenderGroup CreateRenderGroup(
    RenderCollector* renderCollector,
    DrawCallCollection& drawCallCollection,
    const RenderableAttributeSet& attributes)
{
    AssertDebug(renderCollector->batchAllocator != nullptr);

    EnumFlags<RenderGroupFlags> renderGroupFlags = renderCollector->renderGroupFlags;

    // Disable occlusion culling for translucent objects
    const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

    if (RenderBucketMask<RenderBucket::Translucent, RenderBucket::Sky, RenderBucket::Debug> & (1u << uint32(rb)))
    {
        renderGroupFlags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
    }

    // Create RenderGroup
    RenderGroup rg {};
    rg.renderableAttributes = attributes;
    rg.flags = renderGroupFlags;

    if (renderGroupFlags & RenderGroupFlags::INDIRECT_RENDERING)
    {
        AssertDebug(drawCallCollection.indirectRenderer == nullptr, "Indirect renderer already exists on mapping");

        drawCallCollection.indirectRenderer = PoolNew<IndirectRenderer>(*g_renderPool);
        drawCallCollection.indirectRenderer->Create(renderCollector->batchAllocator);
    }

    drawCallCollection.batchAllocator = renderCollector->batchAllocator;
    
    // If parallel rendering is globally disabled, disable it for this RenderGroup
    if (!g_renderInterface->GetRenderConfig().parallelRendering)
    {
        rg.flags &= ~RenderGroupFlags::PARALLEL_RENDERING;
    }

    if (!g_renderInterface->GetRenderConfig().indirectRendering)
    {
        rg.flags &= ~RenderGroupFlags::INDIRECT_RENDERING;
    }

    return rg;
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
      priority(0),
      resourceTrackers {}
{
    AssertDebug(pAllocator != nullptr);

    // initialize the resource trackers
    ForEachResourceTrackerType(resourceTrackers.ToSpan(), [this, pAllocator]<class ResourceTrackerType>(TypeWrapper<ResourceTrackerType>, ResourceTrackerBase<AllocatorType>*& pResourceTracker, size_t idx)
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

    m_lock.UnlockWriter();
}

void RenderProxyList::BeginRead(bool* pOutSuccess)
{
    constexpr uint32 MaxSpinsBeforeFail = 32;

    bool lockAcquired = false;
    uint32 numSpins = 0;

    while (!lockAcquired)
    {
        while (!(lockAcquired = m_lock.TryLockReader()) && numSpins++ < MaxSpinsBeforeFail)
            ;

        if (!lockAcquired && numSpins >= MaxSpinsBeforeFail)
        {
            HYP_LOG(Rendering, Verbose, "Failed to acquire read lock. "
                                        "If this is occurring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

            if (pOutSuccess != nullptr)
            {
                *pOutSuccess = false;

                return;
            }

            // continue and try again, if no pOutSuccess
            ThreadSleep(0);
        }
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
    
    /// @NOTE: If BeginRead() is called on other thread between the check and setting state to CS_DONE,
    /// we could set state to done when it isn't actually.
    if (m_lock.UnlockReader() == 0)
    {
        state = CS_DONE;
    }
}

#pragma endregion RenderProxyList

#pragma region RenderCollector Helpers

template <class TCommandRecorder>
static void SetForwardShadingUniforms(
    const RenderSetup& renderSetup,
    TCommandRecorder& cr,
    uint32& numShaderUniforms)
{
    struct ForwardShadingConstants
    {
        LightShaderData lights[MaxBoundLightsForwardShading];
        ShadowMapData shadowMaps[MaxBoundLightsForwardShading];
        uint32 numBoundLights;
    };

    GpuBuffer* cBuffer = nullptr;
    size_t cBufferOffset = 0;
    size_t cBufferSize = 0;

    ForwardShadingConstants* forwardShadingConstants = (ForwardShadingConstants*)g_renderInterface->constantsAllocator->Allocate(
        sizeof(ForwardShadingConstants),
        alignof(ForwardShadingConstants),
        cBuffer,
        cBufferOffset);

    Assert(forwardShadingConstants != nullptr);
    Memory::Zero(forwardShadingConstants, sizeof(ForwardShadingConstants));

    cBufferSize = sizeof(ForwardShadingConstants);
        
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
                
        View* shadowMapViewDynamic;
        View* shadowMapViewStatic;

        ShadowMap* shadowMap = g_renderInterface->shadowMapCache->GetShadowMap(
            light,
            renderSetup.view,
            0,
            shadowMapViewDynamic,
            shadowMapViewStatic);

        if (shadowMap != nullptr)
        {
            ShadowMapAtlasElement* atlasElement = shadowMap->GetAtlasElement();
            AssertDebug(atlasElement != nullptr);

            if (!atlasElement)
                continue;

            AssertDebug(shadowMapViewDynamic != nullptr && shadowMapViewDynamic->GetCamera() != nullptr);

            RenderProxyCamera* shadowCameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(shadowMapViewDynamic->GetCamera()));
            AssertDebug(shadowCameraProxy != nullptr);

            const Mat4f& viewProjMat = shadowCameraProxy->bufferData.viewProjMat;

            BoundingBox shadowBoundsNDC;
            shadowBoundsNDC.min = Vec3f(-1.0f);
            shadowBoundsNDC.max = Vec3f(1.0f);

            BoundingBox shadowBoundsWS = viewProjMat.Inverse() * shadowBoundsNDC;
        
            currShadowMapData.layerIndex = atlasElement->layerIndex;

            currShadowMapData.viewProjMat = viewProjMat;

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

    rpl.EndRead();

    cr << SetShaderUniform(numShaderUniforms++, "FowardShadingConstants"_sh, cBuffer, ShaderDataOffset(cBufferOffset, cBufferSize));
}

template <bool UseIndirectRendering, class TCommandRecorder>
static void RenderAll(
    Frame* frame,
    TCommandRecorder& cr,
    const RenderSetup& renderSetup,
    IndirectRenderer* indirectRenderer,
    const DrawCallCollection& drawCallCollection)
{
    HYP_SCOPE;

    if constexpr (UseIndirectRendering)
    {
        AssertDebug(indirectRenderer != nullptr);
    }

    static const bool s_useBindlessTextures = g_renderInterface->GetRenderConfig().bindlessTextures;

    if (drawCallCollection.instancedDrawCalls.Empty() && drawCallCollection.drawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    const uint32 frameIndex = frame->GetFrameIndex();

    const RenderGroup& renderGroup = drawCallCollection.renderGroup;
    const RenderableAttributeSet& renderableAttributes = renderGroup.renderableAttributes;

    uint32 numShaderUniforms = 0;
    
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));
    
    cr << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));

    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    
    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetAtlasImageView()); 
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetPointLightShadowMapImageView());
    
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));
    
    if (renderSetup.light != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex), TShaderDataOffset<LightShaderData>(renderSetup.light));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex), TShaderDataOffset<LightShaderData>(0));

    if (renderSetup.envProbe != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(0));
    
    if (renderableAttributes.GetMaterialAttributes().shaderProperties.Test(s_propShadingTypeForward))
    {
        SetForwardShadingUniforms(renderSetup, cr, numShaderUniforms);
    }

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    if (dpd != nullptr)
    {
        cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));
    }

    Mesh* prevMesh = nullptr;

    const DrawCallStorage& drawCalls = drawCallCollection.drawCalls;
    for (size_t i = 0; i < drawCalls.Size(); i++)
    {
        AssertDebug(drawCalls.entityIds[i].GetTypeId() == TypeId::ForType<Entity>());

        const uint32 materialBoundIndex = Resources::GetBinding(drawCalls.materials[i]);
        AssertDebug(materialBoundIndex != ~0u);

        uint32 numDrawCallUniforms = numShaderUniforms;

        cr << SetShaderUniform(numDrawCallUniforms++, "CurrentEntity"_sh,
            g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex),
            TShaderDataOffset<EntityShaderData>(drawCalls.entityIds[i].ToIndex()));

        cr << SetShaderUniform(numDrawCallUniforms++, "MaterialsBuffer"_sh,
            g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex),
            TShaderDataOffset<MaterialShaderData>(materialBoundIndex));
                        
        if (drawCalls.skeletons[i] != nullptr)
        {
            cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh,
                g_renderInterface->gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex),
                TShaderDataOffset<SkeletonShaderData>(drawCalls.skeletons[i]));
        }
        
        if (!s_useBindlessTextures)
        {
            const uint32 textureMask = drawCallCollection.renderGroup.renderableAttributes.GetMaterialAttributes().textureMask;

            if (textureMask != 0)
            {
                RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(drawCalls.materials[i]));
                AssertDebug(materialProxy != nullptr);

                Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                FOR_EACH_BIT(textureMask, bit)
                {
                    const Name textureUniformName = Material::s_textureNames[bit];

                    cr << SetShaderUniform(numDrawCallUniforms++,
                        textureUniformName,
                        imageViews[materialProxy->boundTextureIndices[bit]]);
                }
            }
        }
        
        cr << CommitDrawState();

        if (!prevMesh || prevMesh != drawCalls.meshes[i])
        {
            cr << BindVertexBuffer(drawCalls.meshes[i]->GetVertexBuffer());
            cr << BindIndexBuffer(drawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(drawCalls.materials[i] != nullptr && drawCalls.materials[i]->IsReady());
            if (!drawCalls.materials[i]->GetTexture(MaterialTextureKey::Diffuse))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", drawCalls.materials[i]->GetName());
            }
#endif
        }

        if (UseIndirectRendering && drawCalls.drawCommandIndices[i] != ~0u)
        {
            cr << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                drawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            cr << DrawIndexed(drawCalls.numIndices[i], 1);
        }

        prevMesh = drawCalls.meshes[i];

        g_statDrawCalls++;
        g_statTriangles += drawCalls.numIndices[i] / 3;
    }

    const InstancedDrawCallStorage& instancedDrawCalls = drawCallCollection.instancedDrawCalls;

    for (size_t i = 0; i < instancedDrawCalls.Size(); i++)
    {
        uint32 numDrawCallUniforms = numShaderUniforms;

        EntityInstanceBatch* entityInstanceBatch = instancedDrawCalls.batches[i];
        AssertDebug(entityInstanceBatch != nullptr);
        
        const uint32 stride = drawCallCollection.batchAllocator->GetStructSize();

        cr << SetShaderUniform(numDrawCallUniforms++, "EntityInstanceBatchesBuffer"_sh,
            drawCallCollection.batchAllocator->GetGpuBufferHolder()->GetBuffer(frameIndex),
            ShaderDataOffset(entityInstanceBatch->batchIndex * stride, stride));

        const uint32 materialBoundIndex = Resources::GetBinding(instancedDrawCalls.materials[i]);
        AssertDebug(materialBoundIndex != ~0u);

        cr << SetShaderUniform(numDrawCallUniforms++, "MaterialsBuffer"_sh,
            g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex),
            TShaderDataOffset<MaterialShaderData>(materialBoundIndex));
                        
        if (instancedDrawCalls.skeletons[i] != nullptr)
        {
            cr << SetShaderUniform(numDrawCallUniforms++, "SkeletonsBuffer"_sh,
                g_renderInterface->gpuBuffers[GRB_SKELETONS]->GetBuffer(frameIndex),
                TShaderDataOffset<SkeletonShaderData>(instancedDrawCalls.skeletons[i]));
        }
        
        if (!s_useBindlessTextures)
        {
            const uint32 textureMask = drawCallCollection.renderGroup.renderableAttributes.GetMaterialAttributes().textureMask;

            if (textureMask != 0)
            {
                RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(instancedDrawCalls.materials[i]));
                AssertDebug(materialProxy != nullptr);

                Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                FOR_EACH_BIT(textureMask, bit)
                {
                    const Name textureUniformName = Material::s_textureNames[bit];

                    cr << SetShaderUniform(numDrawCallUniforms++,
                        textureUniformName,
                        imageViews[materialProxy->boundTextureIndices[bit]]);
                }
            }
        }
        
        cr << CommitDrawState();

        if (!prevMesh || prevMesh != instancedDrawCalls.meshes[i])
        {
            cr << BindVertexBuffer(instancedDrawCalls.meshes[i]->GetVertexBuffer());
            cr << BindIndexBuffer(instancedDrawCalls.meshes[i]->GetIndexBuffer());

#if HYP_MATERIAL_DEBUG
            AssertDebug(instancedDrawCalls.materials[i] != nullptr && instancedDrawCalls.materials[i]->IsReady());
            if (!instancedDrawCalls.materials[i]->GetTexture(MaterialTextureKey::Diffuse))
            {
                HYP_LOG(Rendering, Warning, "Rendering instanced draw call with material '{}' that has no albedo map bound!", instancedDrawCalls.materials[i]->GetName());
            }
#endif
        }

        if (UseIndirectRendering && instancedDrawCalls.drawCommandIndices[i] != ~0u)
        {
            cr << DrawIndexedIndirect(
                indirectRenderer->GetDrawState().GetIndirectBuffer(frameIndex),
                instancedDrawCalls.drawCommandIndices[i] * uint32(sizeof(IndirectDrawCommand)));
        }
        else
        {
            cr << DrawIndexed(instancedDrawCalls.numIndices[i], entityInstanceBatch->numEntities);
        }

        prevMesh = instancedDrawCalls.meshes[i];

        g_statDrawCalls++;
        g_statInstancedDrawCalls++;
        g_statTriangles += instancedDrawCalls.numIndices[i] / 3;
    }
}

template <class TCommandRecorder>
static void PerformRenderingImpl(
    Frame* frame,
    TCommandRecorder& cr,
    const RenderSetup& renderSetup,
    const DrawCallCollection& drawCallCollection,
    IndirectRenderer* indirectRenderer)
{
    static const bool s_indirectRenderingEnabled = g_renderInterface->GetRenderConfig().indirectRendering;

    const bool useIndirectRendering = s_indirectRenderingEnabled
        && drawCallCollection.renderGroup.flags[RenderGroupFlags::INDIRECT_RENDERING]
        && (renderSetup.passData && renderSetup.passData->cullData.depthPyramidImageView);

    const RenderableAttributeSet& renderableAttributes = drawCallCollection.renderGroup.renderableAttributes;
    const uint8 stencilReference = renderableAttributes.GetMaterialAttributes().stencilReference;

    cr << SetTopology(renderableAttributes.GetMeshAttributes().topology);
    cr << SetVertexAttributes(renderableAttributes.GetMeshAttributes().vertexAttributes);
    
    cr << SetCurrentViewport(renderSetup.viewport);
    
    cr << SetCurrentShader(ShaderDesc(
        renderableAttributes.GetMaterialAttributes().shaderName,
        renderableAttributes.GetMaterialAttributes().shaderProperties));

    cr << SetFillMode(renderableAttributes.GetMaterialAttributes().fillMode);
    cr << SetFaceCullMode(renderableAttributes.GetMaterialAttributes().cullFaces);
    
    cr << SetCurrentBlendFunction(renderableAttributes.GetMaterialAttributes().blendFunction);

    cr << SetDepthTest(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    cr << SetDepthWrite(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
    cr << SetDepthClamp(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP));

    if (renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
    {
        cr << SetDepthBias(
            renderableAttributes.GetMaterialAttributes().depthBias,
            renderableAttributes.GetMaterialAttributes().depthBiasSlope);
    }

    cr << SetStencilTest(bool(renderableAttributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST));
    cr << SetStencilFunction(renderableAttributes.GetMaterialAttributes().stencilFunction);

    if (stencilReference != 0)
    {
        // apply stencil state before render (write)
        cr << SetStencilState(stencilReference, 0x0, 0xFF);
    }

    if (useIndirectRendering)
    {
        RenderAll<true>(
            frame,
            cr,
            renderSetup,
            indirectRenderer,
            drawCallCollection);
    }
    else
    {
        RenderAll<false>(
            frame,
            cr,
            renderSetup,
            indirectRenderer,
            drawCallCollection);
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
    : parallelRenderingStates {},
      batchAllocator(nullptr),
      renderGroupFlags(RenderGroupFlags::DEFAULT)
{
}

RenderCollector::~RenderCollector()
{
    HYP_SCOPE;

    const bool isParallel = renderGroupFlags[RenderGroupFlags::PARALLEL_RENDERING];

    DeleteOnRenderThread([isParallel, attrs = std::move(previousAttributes), m = std::move(mappingsByBucket), states = parallelRenderingStates]() mutable
        {
            attrs.Clear(/* freeMemory */ true);

            Array<FixedArray<ParallelRenderingState::LocalQueue*, ParallelRenderingState::MaxBatches>> allLocalQueues;

            // Collect command recorders.
            for (auto& list : states)
            {
                if (list.head)
                {
                    ParallelRenderingState* state = list.head;

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
                            allLocalQueues.PushBack(state->sharedData->threadLocalRecorders);

                            state->sharedData->threadLocalRecorders = {};
                        }

                        ParallelRenderingState* nextState = state->next;

                        delete state;

                        state = nextState;
                    }
                }
            }

            if (allLocalQueues.Any())
            {
                const auto DestructCommandRecorders = [&allLocalQueues]() -> void
                {
                    const uint32 currRenderThreadThreadIndex = CurrentRenderThreadIndex();
                    Assert(currRenderThreadThreadIndex < ParallelRenderingState::MaxBatches);

                    for (FixedArray<ParallelRenderingState::LocalQueue*, ParallelRenderingState::MaxBatches>& queues : allLocalQueues)
                    {
                        ParallelRenderingState::LocalQueue* currQueue = queues[currRenderThreadThreadIndex];

                        if (currQueue != nullptr)
                        {
                            currQueue->~TCommandRecorder();
                        }
                    }
                };
                
                // Render thread == 0
                DestructCommandRecorders();
                
                if (isParallel)
                {
                    // we have to free up the memory for each local queue on individual threads,
                    // due to the use of ThreadAllocator:
                    Array<Task<void>> tasks;
                    tasks.Reserve(ParallelRenderingState::MaxBatches);

                    auto& poolThreads = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER).GetThreads();

                    for (uint32 i = 0; i < uint32(poolThreads.Size()); i++)
                    {
                        AssertDebug(poolThreads[i] != nullptr);

                        tasks.EmplaceBack(poolThreads[i]->GetScheduler().Enqueue([&DestructCommandRecorders] { DestructCommandRecorders(); }));
                    }

                    AwaitAll(tasks.ToSpan());
                }

                for (FixedArray<ParallelRenderingState::LocalQueue*, ParallelRenderingState::MaxBatches>& queues : allLocalQueues)
                {
                    for (ParallelRenderingState::LocalQueue* queue : queues)
                    {
                        // @NOTE: not PoolDelete(), we already destructed it on its own thread.
                        PoolFree(*g_renderPool, queue);
                    }
                }
            }

            for (auto& mappings : m)
            {
                for (auto& it : mappings)
                {
                    DrawCallCollection& drawCallCollection = it.second;
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

#if HYP_DEBUG_MODE
size_t RenderCollector::NumDrawCallsCollected() const
{
    size_t numDrawCalls = 0;

    for (const auto& mappings : mappingsByBucket)
    {
        for (const KeyValuePair<RenderableAttributeSet, DrawCallCollection>& it : mappings)
        {
            const DrawCallCollection& drawCallCollection = it.second;

            numDrawCalls += drawCallCollection.drawCalls.Size()
                + drawCallCollection.instancedDrawCalls.Size();
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
            DrawCallCollection& drawCallCollection = it.second;
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

ParallelRenderingState* RenderCollector::AcquireNextParallelRenderingState(uint8 index)
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

void RenderCollector::CommitParallelRenderingState(CommandRecorder& cr, uint8 index)
{
    HYP_SCOPE;
    
    ParallelRenderingState*& parallelRenderingStateHead = parallelRenderingStates[index].head;
    ParallelRenderingState*& parallelRenderingStateTail = parallelRenderingStates[index].tail;

    ParallelRenderingState* state = parallelRenderingStateHead;

    if (!state)
    {
        // non threaded -- reset draw states

        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetVertexAttributes(VertexAttributeSet::StaticMeshVertexAttributes);
        cr << SetTopology(TOP_TRIANGLES);
        cr << SetFillMode(FM_FILL);
        cr << SetFaceCullMode(FCM_BACK);
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        cr << SetDepthBias(0, 0.0f);
        cr << SetDepthClamp(false);
        cr << SetStencilTest(false);
    }

    while (state)
    {
        AssertDebug(state->taskBatch != nullptr);
        state->taskBatch->AwaitCompletion();

        state->renderThreadRecorder.Done();
        cr.Concat(state->renderThreadRecorder);
        state->renderThreadRecorder.Reset(/* freeMemory */ false);

        for (uint32 i = 0; i < ParallelRenderingState::MaxBatches; i++)
        {
            state->threadLocalRecorders[i]->Done();
            cr.Concat(*state->threadLocalRecorders[i]);
            state->threadLocalRecorders[i]->Reset(/* freeMemory */ false);
        }

        // end threaded commands -- reset draw states
        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetVertexAttributes(VertexAttributeSet::StaticMeshVertexAttributes);
        cr << SetTopology(TOP_TRIANGLES);
        cr << SetFillMode(FM_FILL);
        cr << SetFaceCullMode(FCM_BACK);
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        cr << SetDepthBias(0, 0.0f);
        cr << SetDepthClamp(false);
        cr << SetStencilTest(false);

        // Add render stats counts to the engine's render stats
        for (EngineStatsValueSet& valueSet : state->statValues)
        {
            g_engineStats->RecordValueSet(valueSet);

            valueSet = {}; // Reset counts after adding for next use
        }

        state->sharedData->Reset();

        state->drawCalls.Clear();
        state->instancedDrawCalls.Clear();
        state->drawCallPayload = {};

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
                DrawCallCollection& drawCallCollection = it.second;
                AssertDebug(drawCallCollection.IsValid());

                RenderGroup& renderGroup = drawCallCollection.renderGroup;
                AssertDebug(renderGroup.valid);

                IndirectRenderer* indirectRenderer = drawCallCollection.indirectRenderer;

                if (renderGroup.flags & RenderGroupFlags::OCCLUSION_CULLING)
                {
                    AssertDebug((renderGroup.flags & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));
                    AssertDebug(indirectRenderer != nullptr);

                    indirectRenderer->GetDrawState().ResetDrawState();

                    indirectRenderer->PushDrawCallsToIndirectState(drawCallCollection);
                    indirectRenderer->ExecuteCullShaderInBatches(frame, renderSetup);
                }
            }
        }
    }
}


bool RenderCollector::BeginRecordDrawCalls(
    Frame* frame,
    const RenderSetup& renderSetup,
    uint32 bucketBits)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Span<HashMap<RenderableAttributeSet, DrawCallCollection, NodeAllocator<RenderAllocator>>> groupsView;

    if (bucketBits == 0)
    {
        bucketBits = AllBucketsMask;
    }

    // If only one bit is set, we can skip the loop by directly accessing the RenderGroup
    if (ByteUtil::BitCount(bucketBits) == 1)
    {
        const uint32 renderBucketIndex = MathUtil::FastLog2_Pow2(bucketBits);

        auto& mappings = mappingsByBucket[renderBucketIndex];

        if (mappings.Empty())
        {
            return false;
        }

        groupsView = { &mappings, 1 };
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

        for (auto& it : mappings)
        {
            const RenderableAttributeSet& attributes = it.first;

            DrawCallCollection& drawCallCollection = it.second;
            AssertDebug(drawCallCollection.IsValid());

            RenderGroup& renderGroup = drawCallCollection.renderGroup;
            AssertDebug(renderGroup.valid);

            IndirectRenderer* indirectRenderer = drawCallCollection.indirectRenderer;

            if (!(renderGroup.flags & RenderGroupFlags::PARALLEL_RENDERING))
            {
                continue;
            }

            AssertDebug(renderGroup.parallelRenderingState == nullptr);

            if (!parallelRenderingState)
            {
                parallelRenderingState = AcquireNextParallelRenderingState(uint8(mappingIndex));

                AssertDebug(parallelRenderingState != nullptr);
            }

            renderGroup.parallelRenderingState = parallelRenderingState;

            AssertDebug(renderGroup.parallelRenderingState->taskBatch != nullptr);

            // @TODO refactor to use payload similar to before
            parallelRenderingState->taskBatch->AddTask([this, frame, renderSetup = renderSetup, &drawCallCollection, indirectRenderer]()
                {
                    PerformRendering(frame, renderSetup, drawCallCollection, indirectRenderer);
                });
            
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
    bool commit)
{
    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.view->IsReady());

    if (renderSetup.view->GetFlags() & ViewFlags::GBUFFER)
    {
        // Pass NULL framebuffer for GBuffer rendering, as it will be handled by DeferredRenderer outside of this scope.
        ExecuteDrawCalls(frame, renderSetup, nullptr, bucketBits, commit);
    }
    else
    {
        Framebuffer* framebuffer = renderSetup.framebuffer;
    
        if (!framebuffer)
        {
            framebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer();
        }

        AssertDebug(framebuffer != nullptr, "Must have a valid framebuffer for rendering");

        ExecuteDrawCalls(frame, renderSetup, framebuffer, bucketBits, commit);
    }
}

void RenderCollector::ExecuteDrawCalls(
    Frame* frame,
    const RenderSetup& renderSetup,
    Framebuffer* framebuffer,
    uint32 bucketBits,
    bool commit)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (bucketBits == 0)
    {
        bucketBits = AllBucketsMask;
    }

    Span<HashMap<RenderableAttributeSet, DrawCallCollection, NodeAllocator<RenderAllocator>>> groupsView;

    // If only one bit is set, we can skip the loop by directly accessing the RenderGroup
    if (ByteUtil::BitCount(bucketBits) == 1)
    {
        const uint32 renderBucketIndex = MathUtil::FastLog2_Pow2(bucketBits);

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
        frame->cr << SetCurrentFramebuffer(framebuffer);
    }

    // set these to null after rendering
    Array<ParallelRenderingState**, RenderTempAllocator> parallelRenderingStatesToNullify;
    parallelRenderingStatesToNullify.Reserve(32);

    for (auto& mappings : groupsView)
    {
        for (auto& it : mappings)
        {
            const RenderableAttributeSet& attributes = it.first;

            DrawCallCollection& drawCallCollection = it.second;
            AssertDebug(drawCallCollection.IsValid());

            const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

            if (!(bucketBits & (1u << uint32(rb))))
            {
                continue;
            }

            RenderGroup& renderGroup = drawCallCollection.renderGroup;
            AssertDebug(renderGroup.valid);

            IndirectRenderer* indirectRenderer = drawCallCollection.indirectRenderer;

            ParallelRenderingState* parallelRenderingState = nullptr;

            if (renderGroup.flags & RenderGroupFlags::PARALLEL_RENDERING)
            {
                parallelRenderingStatesToNullify.PushBack(&renderGroup.parallelRenderingState);

                if (renderGroup.parallelRenderingState != nullptr)
                {
                    // If PrepareAsyncDrawCalls() was used, parallelRenderingState would be non-null,
                    // therefore we skip enqueueing teh task batch if that is set and instead just
                    // will wait on the existing one
                    continue;
                }

                parallelRenderingState = AcquireNextParallelRenderingState(uint8(rb));
            }

            renderGroup.parallelRenderingState = parallelRenderingState;
            PerformRendering(frame, renderSetup, drawCallCollection, indirectRenderer);

            if (parallelRenderingState != nullptr)
            {
                AssertDebug(parallelRenderingState->taskBatch != nullptr);

                TaskSystem::GetInstance().EnqueueBatch(parallelRenderingState->taskBatch);
            }
        }
    }

    if (commit)
    {
        FOR_EACH_BIT(bucketBits, bit)
        {
            // Wait for all parallel rendering tasks to finish
            CommitParallelRenderingState(frame->cr, uint8(bit));
        }

        if (parallelRenderingStatesToNullify.Any())
        {
            for (ParallelRenderingState** pp : parallelRenderingStatesToNullify)
            {
                *pp = nullptr;
            }
        }
    }

    if (framebuffer)
    {
        frame->cr << SetCurrentFramebuffer(nullptr);
    }
}

// Called at start of frame on render thread
void RenderCollector::BuildDrawCalls(uint32 bucketBits)
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

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollection>::Iterator;
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

    for (IteratorType it : iterators)
    {
        DrawCallCollection& drawCallCollection = it->second;
        AssertDebug(drawCallCollection.IsValid());

        DrawCallCollection prevDrawCallCollection;
        drawCallCollection.TakeDrawCalls(prevDrawCallCollection);

        for (RenderProxyMesh* meshProxy : drawCallCollection.meshProxies)
        {
            AssertDebug(meshProxy->mesh != nullptr
                        && meshProxy->mesh->GetVertexBuffer() != nullptr
                        && meshProxy->mesh->GetIndexBuffer() != nullptr);

            AssertDebug(meshProxy->material != nullptr && meshProxy->material->IsReady());

            if (meshProxy->numInstances == 0)
            {
                continue;
            }

            DrawCallID drawCallId = DrawCallID(meshProxy->mesh->Id(), meshProxy->material->Id());

            if (!meshProxy->enableAutoInstancing && meshProxy->numInstances == 1)
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
            DrawCallCollection& drawCallCollection = it->second;
            AssertDebug(drawCallCollection.IsValid());

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
        for (const auto& it : mappings)
        {
            const DrawCallCollection& drawCallCollection = it.second;
            AssertDebug(drawCallCollection.IsValid());

            if (drawCallCollection.IsValid())
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
        for (const ObjId<Entity> id : changedIds)
        {
#if HYP_DEBUG_MODE
            // type check - cannot be a subclass of Entity, indices would get messed up
            static const TypeId s_entityTypeId = TypeId::ForType<Entity>();
            AssertDebug(id.GetTypeId() == s_entityTypeId, "Cannot include instance of Entity subclass in RenderGroup: {}", LookupTypeName(id.GetTypeId()));
#endif

            const uint32 idx = id.ToIndex();

            RenderableAttributeSet* cachedAttributes = previousAttributes.TryGet(id.ToIndex());
            AssertDebug(cachedAttributes != nullptr);

            // remove from prev
            auto& prevMappings = mappingsByBucket[uint32(cachedAttributes->GetMaterialAttributes().bucket)];

            auto it = prevMappings.Find(*cachedAttributes);
            Assert(it != prevMappings.End());

            DrawCallCollection* prevDrawCallCollection = &it->second;

            RenderProxyMesh* meshProxy = prevDrawCallCollection->meshProxies.Get(idx);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet newAttributes;
            GeometryPass::BuildAttributes(*meshProxy, newAttributes, overrideAttributes);

            const RenderBucket bucket = newAttributes.GetMaterialAttributes().bucket;

            AssertDebug(newAttributes.GetMeshAttributes().vertexAttributes != 0);

            if (newAttributes == *cachedAttributes)
            {
                // not changed, skip
                continue;
            }
            
            prevDrawCallCollection->meshProxies.EraseAt(idx);
            prevDrawCallCollection = nullptr;

            // Add proxy to group
            DrawCallCollection& newDrawCallCollection = mappingsByBucket[uint32(bucket)][newAttributes];

            RenderGroup& rg = newDrawCallCollection.renderGroup;
            AssertDebug(rg.parallelRenderingState == nullptr); // not handled properly? should be set to null after awaited

            if (!rg.valid)
            {
                rg = CreateRenderGroup(this, newDrawCallCollection, newAttributes);
                rg.valid = true;
            }

            AssertDebug(meshProxy->mesh != nullptr && meshProxy->material != nullptr);

            newDrawCallCollection.meshProxies.Set(idx, meshProxy);

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
            AssertDebug(id.GetTypeId() == TypeId::ForType<Entity>());
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
            const RenderBucket bucket = attributes.GetMaterialAttributes().bucket;

            auto& mappings = mappingsByBucket[uint32(bucket)];

            auto it = mappings.Find(attributes);
            Assert(it != mappings.End());

            DrawCallCollection& drawCallCollection = it->second;
            Assert(drawCallCollection.IsValid());

            AssertDebug(drawCallCollection.meshProxies.HasIndex(idx));
            drawCallCollection.meshProxies.EraseAt(idx);

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
            AssertDebug(id.GetTypeId() == s_entityTypeId, "Cannot include instance of Entity subclass in RenderGroup: {}", LookupTypeName(id.GetTypeId()));
#endif

            const RenderProxyMesh* meshProxy = renderProxyList.GetMeshEntities().GetProxy(id);
            AssertDebug(meshProxy != nullptr);

            RenderableAttributeSet attributes;
            GeometryPass::BuildAttributes(*meshProxy, attributes, overrideAttributes);

            const RenderBucket bucket = attributes.GetMaterialAttributes().bucket;

            // Add proxy to group
            DrawCallCollection& drawCallCollection = mappingsByBucket[uint32(bucket)][attributes];
            RenderGroup& rg = drawCallCollection.renderGroup;

            if (!rg.valid)
            {
                rg = CreateRenderGroup(this, drawCallCollection, attributes);
                rg.valid = true;
            }

            const uint32 idx = id.ToIndex();

            drawCallCollection.meshProxies.Set(idx, const_cast<RenderProxyMesh*>(meshProxy));
            previousAttributes.Set(idx, attributes);
        }
    }
}

void RenderCollector::PerformRendering(
    Frame* frame,
    const RenderSetup& renderSetup,
    const DrawCallCollection& drawCallCollection,
    IndirectRenderer* indirectRenderer)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData for rendering!");

    Framebuffer* framebuffer = renderSetup.framebuffer;
    
    if (!framebuffer)
    {
        framebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer();
    }

    AssertDebug(framebuffer != nullptr);

    if (drawCallCollection.drawCalls.Empty() && drawCallCollection.instancedDrawCalls.Empty())
    {
        // No draw calls to render
        return;
    }

    static const thread_local uint32 s_renderThreadIndex = CurrentRenderThreadIndex();

    if (drawCallCollection.renderGroup.flags & RenderGroupFlags::PARALLEL_RENDERING)
    {
        AssertDebug(drawCallCollection.renderGroup.parallelRenderingState != nullptr);

        auto* cr = drawCallCollection.renderGroup.parallelRenderingState->threadLocalRecorders[s_renderThreadIndex];
        AssertDebug(cr != nullptr);

        PerformRenderingImpl(frame, *cr, renderSetup, drawCallCollection, indirectRenderer);
    }
    else
    {
        PerformRenderingImpl(frame, frame->cr, renderSetup, drawCallCollection, indirectRenderer);
    }
    
    g_statRenderGroups++;
}

#pragma endregion RenderCollector

} // namespace Hyperion

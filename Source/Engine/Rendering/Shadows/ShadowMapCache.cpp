/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMapAllocator.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/TextureViewCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Core/Containers/FlatMap.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Scene/Light.hpp>
#include <Scene/View.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/PerspectiveCamera.hpp>
#include <Scene/Camera/OrthoCamera.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

namespace Hyperion {

CVar<float> g_cvShadowDepthBias("Rendering.ShadowDepthBias", 0.05f);
CVar<float> g_cvShadowDepthBiasDirectional("Rendering.ShadowDepthBiasDirectional", 0.05f);

// Set to true to create camera-specific shadow maps for CSM
// Will cause more shadow maps to be allocated, and specifically other non-main cameras
// (e.g EnvProbes) will likely not be able to use shadow maps due to running out of slots.
static constexpr bool IsCSMCameraDependent = false;

// How much does the depth bias constant get scaled per cascade index?
// This can combat some of the precision artifacts that are seen primarily with higher cascades numbers,
// as they the scene from farther distances.
static constexpr float DepthBiasScaleFactor[MaxShadowMapCascades] = {
    1.0f,
    1.5f,
    1.5f,
    1.75f
};

static constexpr EnumFlags<ViewFlags> DefaultShadowViewFlags = ViewFlags::SHADOW_VIEW
    | ViewFlags::SKIP_LIGHTS | ViewFlags::SKIP_CAMERAS
    | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_PARTICLE_VOLUMES | ViewFlags::SKIP_FOG_VOLUMES
    | ViewFlags::SKIP_ENV_PROBES;

static StaticShaderPropertyId s_propModeShadows { ShaderProperty(NAME("MODE_SHADOWS")) };

static const Name s_shadowMapCameraNames[MaxShadowMapCascades] = {
    NAME("ShadowMapCamera_Cascade0"),
    NAME("ShadowMapCamera_Cascade1"),
    NAME("ShadowMapCamera_Cascade2"),
    NAME("ShadowMapCamera_Cascade3")
};

static HYP_FORCE_INLINE bool IsShadowMapCameraDependent(Light& light)
{
    if constexpr (IsCSMCameraDependent)
    {
        // Only directional lights are view dependent due to it being centered
        // around the view's position.
        // So we must cache shadow map data per-view
        return light.GetLightType() == LightType::Directional;
    }
    else
    {
        return false;
    }
}

ShadowMapCacheKey MakeShadowMapCacheKey(Light* light, View* view)
{
    AssertDebug(light != nullptr);

    ShadowMapCacheKey key {};
    key.lightHash = uint32(BitCast<uint64>(light->Id()) % 0xFFFFFFFFu);

    if (IsShadowMapCameraDependent(*light))
    {
        AssertDebug(view != nullptr && view->GetCamera() != nullptr);

        key.cameraHash = uint32(BitCast<uint64>(view->GetCamera()->Id()) % 0x7FFFFFFFu);
        key.isCameraDependent = 1;
    }

    return key;
}

static FramebufferDesc GetFramebufferDesc(Light* light, ShaderDesc& outShaderDesc, ShadowMap& shadowMap)
{
    const ShadowMapAtlasElement& atlasElement = *shadowMap.GetAtlasElement();

    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = atlasElement.dimensions;
    framebufferDesc.offset = Vec2i(atlasElement.offsetCoords);

    outShaderDesc.name = NAME("DrawShadowMap");

    switch (light->GetLightType())
    {
    case LightType::Point:
    {
        framebufferDesc.numAttachments = 0;

        AttachmentDesc& depth = framebufferDesc.attachments[framebufferDesc.numAttachments++];
        depth.imageType = TextureType::Cubemap;
        depth.format = TextureFormat::D16;
        depth.loadOp = LoadOperation::LOAD;
        depth.storeOp = StoreOperation::STORE;

        outShaderDesc.name = NAME("DrawCubemap");
        outShaderDesc.properties = {};
        outShaderDesc.properties.Add(s_propModeShadows);

        break;
    }
    case LightType::Directional:
    {
        framebufferDesc.numAttachments = 0;

        AttachmentDesc& depth = framebufferDesc.attachments[framebufferDesc.numAttachments++];
        depth.format = TextureFormat::D16;
        depth.imageType = TextureType::Texture2D;
        depth.loadOp = LoadOperation::LOAD;
        depth.storeOp = StoreOperation::STORE;

        break;
    }
    default:
        // no shadow mapping impl
        break;
    }

    return framebufferDesc;
}

static Camera* CreateShadowCamera(Light* light, uint32 cascadeIndex)
{
    Camera* shadowMapCamera = new Camera(int(light->GetShadowMapDimensions().x), int(light->GetShadowMapDimensions().y));
    shadowMapCamera->SetName(s_shadowMapCameraNames[cascadeIndex]);

    switch (light->GetLightType())
    {
    case LightType::Directional:
        shadowMapCamera->AddCameraController(MakeHandle<OrthoCameraController>());
        break;
    case LightType::Point:
        shadowMapCamera->SetFOV(90.0f);
        shadowMapCamera->SetNearClip(0.01f);
        shadowMapCamera->SetFarClip(light->GetRadius());

        shadowMapCamera->AddCameraController(MakeHandle<PerspectiveCameraController>());

        break;
    default:
        break;
    }

    InitObject(shadowMapCamera);

    return shadowMapCamera;
}

static ViewDesc GetViewDesc(
    Light* light,
    bool isStatic,
    uint32 cascadeIndex,
    float depthRange,
    ShadowMap& shadowMap,
    Camera& camera)
{
    // If no valid range has been passed in, then set it based
    // on the camera
    if (depthRange <= 0.001f)
    {
        depthRange = (camera.GetFarClip() - camera.GetNearClip());
    }

    const bool isDirectional = (light->GetLightType() == LightType::Directional);
    const bool isOmni = (light->GetLightType() == LightType::Point);

    const bool hasBakedStaticShadows = (light->GetLightFlags() & LightFlags::BakeStaticShadows);
    const bool cacheStaticShadowMaps = !hasBakedStaticShadows && (light->GetLightFlags() & LightFlags::CacheStaticShadowMaps);
    const bool onlyStaticShadowMaps = (light->GetLightFlags() & LightFlags::OnlyDrawStaticShadowMaps);

    const bool splitStaticAndDynamic = cacheStaticShadowMaps || hasBakedStaticShadows || onlyStaticShadowMaps;

    const float depthBias = (isDirectional ? g_cvShadowDepthBiasDirectional.Get() : g_cvShadowDepthBias.Get());
    const float depthBiasScaled = depthBias * depthRange * (isDirectional ? DepthBiasScaleFactor[cascadeIndex] : 1.0f);

    ViewDesc viewDesc {};
    viewDesc.flags = DefaultShadowViewFlags | ViewFlags::EXTERNAL_RENDERTARGET; // use atlas as target

    if (isOmni)
    {
        viewDesc.flags |= ViewFlags::CUBEMAP_FACE_VIEW;

        viewDesc.viewIndex = cascadeIndex;
    }

    viewDesc.scenes = {};
    viewDesc.camera = &camera;

    ShaderDesc shaderDesc;
    viewDesc.framebufferDesc = GetFramebufferDesc(light, shaderDesc, shadowMap);

    MaterialAttributes materialAttributes {};
    materialAttributes.shaderName = shaderDesc.name;
    materialAttributes.shaderProperties = shaderDesc.properties;
    materialAttributes.flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST | MAF_DEPTH_BIAS | MAF_DEPTH_CLAMP;
    materialAttributes.depthBias = int32(MathUtil::Round(depthBiasScaled));
    materialAttributes.depthBiasSlope = 2.0f;
    materialAttributes.cullFaces = FCM_BACK;

    viewDesc.overrideAttributes = RenderableAttributeSet(MeshAttributes(), materialAttributes);

    viewDesc.flags &= ~ViewFlags::COLLECT_ALL_ENTITIES;

    if (splitStaticAndDynamic)
    {
        if (isStatic)
        {
            viewDesc.flags |= ViewFlags::COLLECT_STATIC_ENTITIES;
        }
        else
        {
            viewDesc.flags |= ViewFlags::COLLECT_DYNAMIC_ENTITIES;
        }
    }
    else if (!isStatic)
    {
        viewDesc.flags |= ViewFlags::COLLECT_ALL_ENTITIES;
    }

    // No parallel draw call collection for shadow maps
    viewDesc.flags |= ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION
                    | ViewFlags::NO_ASYNC_SHADER_LOADING;

    return viewDesc;
}

// Shadow maps cached per-light.
// Since Lights can have multiple shadow views that blit into one final shadow map
// we store the shadow maps here rather than on the per-view PassData
struct CachedShadowMapData
{
    FixedArray<ShadowMap*, 6> shadowMaps;

    Camera* camera = nullptr;

    // Max 6 (one per cubemap face)
    FixedArray<View*, 6> shadowViewsDynamic;
    FixedArray<View*, 6> shadowViewsStatic;

    volatile int64 lastUsedFrame;
};

class ShadowMapCacheImpl
{
public:
    ~ShadowMapCacheImpl()
    {
        TUniqueLock lock(mutex);

        FatArray<View*, FixedAllocator<MaxShadowMapCascades * 2>> allViews;

        for (auto& pair : cache)
        {
            CachedShadowMapData& entry = pair.second;

            for (View* view : entry.shadowViewsStatic)
            {
                if (view)
                {
                    allViews.PushBack(view);
                }
            }

            for (View* view : entry.shadowViewsDynamic)
            {
                if (view)
                {
                    allViews.PushBack(view);
                }
            }

            if (allViews.Any())
            {
                EnqueueDeletion(FunctionWrapper<Proc<void()>>([allViews = std::move(allViews)]()
                                                              {
                                                                  for (View* view : allViews)
                                                                  {
                                                                      view->Release();
                                                                  }
                                                              }));
            }

            if (entry.camera)
            {
                entry.camera->Release();
            }
        }
    }

    ShadowMapAllocator allocator;

    /// Cached (per-light/view combination) shadow map rendering data that is cleaned up when no longer used
    Map<ShadowMapCacheKey, CachedShadowMapData, RenderAllocator> cache;

    SlimArray<Camera*> deferredDeletionCameras;
    AtomicFlag hasDeferredDeletionCameras;

    SharedMutex mutex;
};

ShadowMapCache::ShadowMapCache()
    : m_impl(MakePimplWithAllocator<ShadowMapCacheImpl, RenderAllocator>())
{
}

ShadowMapCache::~ShadowMapCache() = default;

void ShadowMapCache::Initialize()
{
    m_impl->allocator.Initialize();
}

void ShadowMapCache::Shutdown()
{
    m_impl->allocator.Shutdown();
}

GpuImage* ShadowMapCache::GetAtlasImage() const
{
    return m_impl->allocator.GetAtlasTextureArray()->GetGpuImage();
}

GpuImageView* ShadowMapCache::GetAtlasImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_impl->allocator.GetAtlasTextureArray());
}

GpuImage* ShadowMapCache::GetPointLightShadowMapImage() const
{
    return m_impl->allocator.GetPointLightTextureArray()->GetGpuImage();
}

GpuImageView* ShadowMapCache::GetPointLightShadowMapImageView() const
{
    return RI.textureViewCache->GetOrCreate(m_impl->allocator.GetPointLightTextureArray());
}

HYP_NODISCARD View* ShadowMapCache::GetOrCreateShadowView(
    View* view,
    Light* light,
    uint32 cascadeIndex,
    float depthRange,
    bool isStatic) const
{
    Assert(view != nullptr && light != nullptr);

    View* outView = nullptr;

    const ShadowMapCacheKey key = MakeShadowMapCacheKey(light, view);

    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);
    TUniqueLock<SharedMutex> uniqueLock; // not locked yet

    // is 'cascadeIndex' actually the index of the cubemap face?
    const bool isOmni = (light->GetLightType() == LightType::Point);

    auto initShadowCascade = [this, cascadeIndex, light, isOmni](CachedShadowMapData& entry) -> ShadowMap*
    {
        static constexpr ShadowMapType LightTypeToShadowMapType[uint32(LightType::Max)] = {
            SMT_DIRECTIONAL, // Directional
            SMT_OMNI,        // Point
            SMT_SPOT,        // Spot
            SMT_SPOT         // AreaRect
        };

        const ShadowMapType shadowMapType = LightTypeToShadowMapType[uint32(light->GetLightType())];

        ShadowMap* newShadowMap = m_impl->allocator.AllocateShadowMap(shadowMapType, light->GetShadowMapDimensions());

        ShadowMap*& outShadowMap = isOmni
            ? entry.shadowMaps[0]
            : entry.shadowMaps[cascadeIndex];

        Assert(outShadowMap == nullptr, "Overwriting allocated shadow map!");

        return (outShadowMap = newShadowMap);
    };

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        CachedShadowMapData* entry = &it->second;
        AtomicExchange(&entry->lastUsedFrame, int64(GetFrameCounter()));

        auto* views = isStatic ? &entry->shadowViewsStatic : &entry->shadowViewsDynamic;

        if (!entry->camera || !entry->shadowMaps[isOmni ? 0 : cascadeIndex])
        {
            sharedLock.Reset();
            uniqueLock.Reset(m_impl->mutex);

            if (!entry->camera)
            {
                entry->camera = CreateShadowCamera(light, cascadeIndex);
            }

            if (!initShadowCascade(*entry))
            {
                return nullptr;
            }
        }

        outView = (*views)[cascadeIndex];

        if (outView != nullptr)
        {
            return outView;
        }

        sharedLock.Reset();
        uniqueLock.Reset(m_impl->mutex);

        it = m_impl->cache.Find(key);
        Assert(it != m_impl->cache.End());

        entry = &it->second;

        if (!entry->camera)
        {
            entry->camera = CreateShadowCamera(light, cascadeIndex);
        }

        views = isStatic ? &entry->shadowViewsStatic : &entry->shadowViewsDynamic;
        outView = (*views)[cascadeIndex];

        if (!outView)
        {
            AssertDebug(entry->camera != nullptr);

            ShadowMap* shadowMap = entry->shadowMaps[isOmni ? 0 : cascadeIndex];
            if (!shadowMap)
            {
                // shadow map was not allocated. abort the operation.
                return nullptr;
            }

            outView = new View(GetViewDesc(light, isStatic, cascadeIndex, depthRange, *shadowMap, *entry->camera));

            HYP_LOG(Rendering, Debug, "Create new shadow view for Light: {}", light->GetName());

            if (isStatic)
            {
                outView->SetName(NAME_FMT("ShadowMapView_{}_{}_Static", light->GetName(), view->GetName()));
            }
            else
            {
                outView->SetName(NAME_FMT("ShadowMapView_{}_{}", light->GetName(), view->GetName()));
            }

            InitObject(outView);

            (*views)[cascadeIndex] = outView;
        }
    }
    else
    {
        sharedLock.Reset();
        uniqueLock.Reset(m_impl->mutex);

        CachedShadowMapData& entry = m_impl->cache[key];
        AtomicExchange(&entry.lastUsedFrame, int64(GetFrameCounter()));

        if (!entry.camera)
        {
            entry.camera = CreateShadowCamera(light, cascadeIndex);
        }

        if (!entry.shadowViewsStatic[cascadeIndex])
        {
            if (!initShadowCascade(entry))
            {
                return nullptr;
            }
        }

        auto& views = isStatic ? entry.shadowViewsStatic : entry.shadowViewsDynamic;
        outView = views[cascadeIndex];

        if (!outView)
        {
            AssertDebug(entry.camera != nullptr);

            ShadowMap* shadowMap = entry.shadowMaps[isOmni ? 0 : cascadeIndex];

            if (!shadowMap)
            {
                // shadow map was not allocated. abort the operation.
                return nullptr;
            }

            outView = new View(GetViewDesc(light, isStatic, cascadeIndex, depthRange, *shadowMap, *entry.camera));

            HYP_LOG(Rendering, Debug, "Create new shadow view for Light: {}", light->GetName());

            if (isStatic)
            {
                outView->SetName(NAME_FMT("ShadowMapView_{}_{}_Static", light->GetName(), view->GetName()));
            }
            else
            {
                outView->SetName(NAME_FMT("ShadowMapView_{}_{}", light->GetName(), view->GetName()));
            }

            InitObject(outView);

            views[cascadeIndex] = outView;
        }
    }

    return outView;
}

View* ShadowMapCache::TryGetShadowView(
    View* view,
    Light* light,
    uint32 cascadeIndex,
    bool isStatic,
    bool isLazy) const
{
    const ShadowMapCacheKey key = MakeShadowMapCacheKey(light, view);

    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        auto& shadowViews = isStatic ? it->second.shadowViewsStatic : it->second.shadowViewsDynamic;

        return shadowViews[cascadeIndex];
    }

    if (isLazy)
    {
        // try to find one we can borrow from
    }

    return nullptr;
}

ShadowMap* ShadowMapCache::GetShadowMap(
    Light* light,
    View* view,
    uint32 cascadeIndex,
    View*& outShadowViewDynamic,
    View*& outShadowViewStatic) const
{
    const ShadowMapCacheKey key = MakeShadowMapCacheKey(light, view);

    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);

    const bool isOmni = (light->GetLightType() == LightType::Point);
    const bool cascadesAreCubeFaces = isOmni;

    outShadowViewDynamic = nullptr;
    outShadowViewStatic = nullptr;

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        CachedShadowMapData& entry = it->second;

        const uint32 shadowMapIndex = cascadesAreCubeFaces ? 0 : cascadeIndex;

        if (shadowMapIndex < entry.shadowMaps.Size())
        {
            outShadowViewDynamic = entry.shadowViewsDynamic[cascadeIndex];
            outShadowViewStatic = entry.shadowViewsStatic[cascadeIndex];

            AtomicExchange(&entry.lastUsedFrame, int64(GetFrameCounter()));

            return entry.shadowMaps[shadowMapIndex];
        }
    }

    return nullptr;
}

bool ShadowMapCache::Remove(const ShadowMapCacheKey& key)
{
    TUniqueLock lock(m_impl->mutex);

    auto it = m_impl->cache.Find(key);

    if (it == m_impl->cache.End())
    {
        AssertDebug(false, "Entry not found!");

        HYP_LOG(Rendering, Warning, "Failed to remove shadow map, entry not found!");

        return false;
    }

    CachedShadowMapData& entry = it->second;

    Array<View*> allViews;

    for (View* view : entry.shadowViewsStatic)
    {
        if (view)
        {
            allViews.PushBack(view);
        }
    }

    for (View* view : entry.shadowViewsDynamic)
    {
        if (view)
        {
            allViews.PushBack(view);
        }
    }

    if (allViews.Any())
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>(
            [allViews = std::move(allViews)]()
            {
                for (View* view : allViews)
                {
                    view->Release();
                }
            }));
    }

    if (entry.camera)
    {
        m_impl->deferredDeletionCameras.PushBack(entry.camera);
        m_impl->hasDeferredDeletionCameras.Store(true);
    }

    for (ShadowMap* shadowMap : entry.shadowMaps)
    {
        if (shadowMap)
        {
            bool success = m_impl->allocator.FreeShadowMap(shadowMap, /* clearTextureRegion */ true);
            AssertDebug(success, "Failed to remove shadow map from atlas!");

            if (!success)
            {
                HYP_LOG(Rendering, Warning, "Failed to remove shadow map from atlas");
            }
        }
    }

    m_impl->cache.Erase(it);

    return true;
}

void ShadowMapCache::Update()
{
    if (!m_impl->hasDeferredDeletionCameras.Load())
    {
        return;
    }

    TUniqueLock lock(m_impl->mutex);

    for (Camera* camera : m_impl->deferredDeletionCameras)
    {
        camera->Release();
    }

    m_impl->deferredDeletionCameras.Resize(0);

    m_impl->hasDeferredDeletionCameras.Store(false);
}

} // namespace Hyperion

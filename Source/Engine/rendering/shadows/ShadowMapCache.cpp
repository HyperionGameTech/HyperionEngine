/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/containers/Map.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion {

#define SHADOW_MAP_CACHE_MULTITHREADED 1

static constexpr EnumFlags<ViewFlags> DefaultShadowViewFlags = ViewFlags::SHADOW_VIEW
    | ViewFlags::SKIP_LIGHTS | ViewFlags::SKIP_CAMERAS
    | ViewFlags::SKIP_LIGHTMAP_VOLUMES | ViewFlags::SKIP_PARTICLE_VOLUMES | ViewFlags::SKIP_FOG_VOLUMES
    | ViewFlags::SKIP_ENV_PROBES | ViewFlags::SKIP_ENV_GRIDS;

static const ShaderPropertyId s_shadowMapFilterProperties[SMF_MAX] = {
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("STANDARD"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("PCF"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("CONTACT_HARDENED"))),
    InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("VSM")))
};

static const ShaderPropertyId s_propModeShadows = InternShaderProperty(ShaderProperty(NAME("MODE_SHADOWS")));

static const Name s_shadowMapCameraNames[MaxShadowMapCascades] = {
    NAME("ShadowMapCamera_Cascade0"),
    NAME("ShadowMapCamera_Cascade1"),
    NAME("ShadowMapCamera_Cascade2"),
    NAME("ShadowMapCamera_Cascade3")
};

static FramebufferDesc GetFramebufferDesc(
    Light* light,
    ShaderDesc& outShaderDesc,
    ShadowMap& shadowMap,
    EnumFlags<ViewFlags>& outViewFlags)
{
    outViewFlags = DefaultShadowViewFlags | ViewFlags::EXTERNAL_RENDERTARGET; // use atlas as target

    const ShadowMapAtlasElement& atlasElement = *shadowMap.GetAtlasElement();

    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = atlasElement.dimensions;
    framebufferDesc.offset = Vec2i(atlasElement.offsetCoords);

    const ShadowMapFilter shadowMapFilter = light->GetShadowMapFilter();

    outShaderDesc.name = NAME("DrawShadowMap");
    outShaderDesc.properties.Add(s_shadowMapFilterProperties[shadowMapFilter]);

    switch (light->GetLightType())
    {
    case LightType::Point:
    {
        // Frustum culling for cubemap views not currently supported.
        outViewFlags |= ViewFlags::NO_FRUSTUM_CULLING;

        framebufferDesc.numAttachments = 0;
        framebufferDesc.numLayers = 6;

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
    shadowMapCamera->SetIsTransient(true);

    switch (light->GetLightType())
    {
    case LightType::Directional:
        shadowMapCamera->AddCameraController(MakeHandle<OrthoCameraController>());
        break;
    case LightType::Point:
        shadowMapCamera->SetFOV(90.0f);
        shadowMapCamera->SetNearClip(0.01f);
        shadowMapCamera->SetFarClip(1000.0f);//light->GetRadius());

        shadowMapCamera->AddCameraController(MakeHandle<PerspectiveCameraController>());

        break;
    default:
        break;
    }

    InitObject(shadowMapCamera);

    return shadowMapCamera;
}

static ViewDesc GetViewDesc(Light* light, bool isStatic, uint32 cascadeIndex, ShadowMap& shadowMap, Camera& camera)
{
    ViewDesc viewDesc {};

    viewDesc.scenes = {};
    viewDesc.camera = &camera;

    ShaderDesc shaderDesc;
    viewDesc.framebufferDesc = GetFramebufferDesc(light, shaderDesc, shadowMap, viewDesc.flags);

    MaterialAttributes materialAttributes {};
    materialAttributes.shaderName = shaderDesc.name;
    materialAttributes.shaderProperties = shaderDesc.properties;
    materialAttributes.flags |= MAF_DEPTH_BIAS | MAF_DEPTH_CLAMP;
    materialAttributes.depthBias = 6;
    materialAttributes.depthBiasSlope = 2.0f;
    materialAttributes.cullFaces = light->GetShadowMapFilter() == SMF_VSM ? FCM_FRONT : FCM_BACK;

    viewDesc.overrideAttributes = RenderableAttributeSet(MeshAttributes(), materialAttributes);

    viewDesc.flags &= ~ViewFlags::COLLECT_ALL_ENTITIES;

    if (isStatic)
    {
        viewDesc.flags |= ViewFlags::COLLECT_STATIC_ENTITIES;
    }
    else
    {
        viewDesc.flags |= ViewFlags::COLLECT_DYNAMIC_ENTITIES;
    }

    // No parallel draw call collection for shadow maps
    viewDesc.flags |= ViewFlags::NO_PARALLEL_DRAW_CALL_COLLECTION;

    return viewDesc;
}

// Shadow maps cached per-light.
// Since Lights can have multiple shadow views that blit into one final shadow map
// we store the shadow maps here rather than on the per-view PassData
struct CachedShadowMapData
{
    Array<ShadowMap*, RenderAllocator> shadowMaps;

    Camera* camera = nullptr;

    Array<View*, FixedAllocator<MaxShadowMapCascades>> shadowViewsDynamic;
    Array<View*, FixedAllocator<MaxShadowMapCascades>> shadowViewsStatic;

    volatile int64 lastFrameUsed;
};

struct ShadowMapCacheKey
{
    Light* light;
    View* view;

    HYP_FORCE_INLINE bool operator==(const ShadowMapCacheKey& other)
    {
        return light == other.light
            && view == other.view;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(light)
            .Combine(view);
    }
};

class ShadowMapCacheImpl
{
public:
    ~ShadowMapCacheImpl()
    {
#if SHADOW_MAP_CACHE_MULTITHREADED
        TUniqueLock lock(mutex);
#endif

        for (auto& pair : cache)
        {
            CachedShadowMapData& entry = pair.second;

            Array<View*, FixedAllocator<MaxShadowMapCascades * 2>> allViews;

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
    TMap<ShadowMapCacheKey, CachedShadowMapData, RenderAllocator> cache;

#if SHADOW_MAP_CACHE_MULTITHREADED
    SharedMutex mutex;
#endif
};

ShadowMapCache::ShadowMapCache()
    : m_impl(MakePimpl<ShadowMapCacheImpl>())
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
    bool isStatic) const
{
    Assert(view != nullptr && light != nullptr);

    View* outView = nullptr;

    ShadowMapCacheKey key {};
    key.view = view;
    key.light = light;

#if SHADOW_MAP_CACHE_MULTITHREADED
    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);
    TUniqueLock<SharedMutex> uniqueLock; // not locked yet
#else
    AssertOnThread(g_renderThread);
#endif

    auto InitShadowCascade = [this, cascadeIndex, light](CachedShadowMapData& entry) -> ShadowMap*
    {
        ShadowMap* shadowMap = m_impl->allocator.AllocateShadowMap(
            light->GetLightType() == LightType::Point ? ShadowMapType::SMT_OMNI : ShadowMapType::SMT_DIRECTIONAL,
            light->GetLightType() == LightType::Directional ? SMF_CONTACT_HARDENED : SMF_STANDARD,
            light->GetShadowMapDimensions());

        if (shadowMap)
        {
            entry.shadowMaps.Resize(cascadeIndex + 1);
            entry.shadowMaps[cascadeIndex] = shadowMap;

            entry.shadowViewsStatic.Resize(cascadeIndex + 1);
            entry.shadowViewsDynamic.Resize(cascadeIndex + 1);
        }

        return shadowMap;
    };

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        CachedShadowMapData* entry = &it->second;
        AtomicExchange(&entry->lastFrameUsed, int64(GetFrameCounter()));

        auto* views = isStatic ? &entry->shadowViewsStatic : &entry->shadowViewsDynamic;

        if (cascadeIndex >= entry->shadowViewsStatic.Size() || !entry->camera)
        {
#if SHADOW_MAP_CACHE_MULTITHREADED
            sharedLock.Reset();
            uniqueLock.Reset(m_impl->mutex);
#endif

            if (!entry->camera)
            {
                entry->camera = CreateShadowCamera(light, cascadeIndex);
            }

            if (cascadeIndex >= entry->shadowViewsStatic.Size())
            {
                if (!InitShadowCascade(*entry))
                {
                    return nullptr;
                }
            }
        }

        outView = (*views)[cascadeIndex];

        if (outView != nullptr)
        {
            return outView;
        }

#if SHADOW_MAP_CACHE_MULTITHREADED
        sharedLock.Reset();
        uniqueLock.Reset(m_impl->mutex);
#endif

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
            AssertDebug(entry->shadowMaps[cascadeIndex] != nullptr);

            outView = new View(GetViewDesc(light, isStatic, cascadeIndex, *entry->shadowMaps[cascadeIndex], *entry->camera));

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
#if SHADOW_MAP_CACHE_MULTITHREADED
        sharedLock.Reset();
        uniqueLock.Reset(m_impl->mutex);
#endif

        CachedShadowMapData& entry = m_impl->cache[key];
        AtomicExchange(&entry.lastFrameUsed, int64(GetFrameCounter()));

        if (!entry.camera)
        {
            entry.camera = CreateShadowCamera(light, cascadeIndex);
        }

        if (cascadeIndex >= entry.shadowViewsStatic.Size())
        {
            if (!InitShadowCascade(entry))
            {
                return nullptr;
            }
        }

        auto& views = isStatic ? entry.shadowViewsStatic : entry.shadowViewsDynamic;
        outView = views[cascadeIndex];

        if (!outView)
        {
            AssertDebug(entry.camera != nullptr);
            AssertDebug(entry.shadowMaps[cascadeIndex] != nullptr);

            outView = new View(GetViewDesc(light, isStatic, cascadeIndex, *entry.shadowMaps[cascadeIndex], *entry.camera));

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
    bool isStatic) const
{
    ShadowMapCacheKey key {};
    key.view = view;
    key.light = light;

#if SHADOW_MAP_CACHE_MULTITHREADED
    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);
    TUniqueLock<SharedMutex> uniqueLock; // not locked yet
#else
    AssertOnThread(g_renderThread);
#endif

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        auto& shadowViews = isStatic ? it->second.shadowViewsStatic : it->second.shadowViewsDynamic;

        if (cascadeIndex < shadowViews.Size())
        {
            return shadowViews[cascadeIndex];
        }
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
    ShadowMapCacheKey key {};
    key.view = view;
    key.light = light;

#if SHADOW_MAP_CACHE_MULTITHREADED
    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);
#else
    AssertOnThread(g_renderThread);
#endif

    outShadowViewDynamic = nullptr;
    outShadowViewStatic = nullptr;

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        CachedShadowMapData& entry = it->second;

        if (cascadeIndex < entry.shadowMaps.Size())
        {
            outShadowViewDynamic = entry.shadowViewsDynamic[cascadeIndex];
            outShadowViewStatic = entry.shadowViewsStatic[cascadeIndex];

            AtomicExchange(&entry.lastFrameUsed, int64(GetFrameCounter()));

            return entry.shadowMaps[cascadeIndex];
        }
    }

    return nullptr;
}

bool ShadowMapCache::Remove(Light* light, View* view)
{
    if (!light || !view)
    {
        return false;
    }

    TUniqueLock lock(m_impl->mutex);

    ShadowMapCacheKey key {};
    key.view = view;
    key.light = light;

    auto it = m_impl->cache.Find(key);

    if (it == m_impl->cache.End())
    {
        return false;
    }

    CachedShadowMapData& entry = it->second;

    Array<View*, FixedAllocator<MaxShadowMapCascades * 2>> allViews;

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

    for (ShadowMap* shadowMap : entry.shadowMaps)
    {
        if (shadowMap)
        {
            bool success = m_impl->allocator.FreeShadowMap(shadowMap, /* clearTextureRegion */ true);

            if (!success)
            {
                HYP_LOG(Rendering, Warning, "Failed to remove shadow map from atlas for Light {} + View {}", light->Id(), view->Id());
            }
        }
    }

    m_impl->cache.Erase(it);

    return true;
}

} // namespace Hyperion

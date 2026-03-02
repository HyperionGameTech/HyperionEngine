/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/shadows/ShadowViewCache.hpp>
#include <rendering/shadows/ShadowMap.hpp>

#include <Core/containers/HashMap.hpp>

#include <Core/threading/SharedMutex.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/PerspectiveCamera.hpp>
#include <scene/camera/OrthoCamera.hpp>

namespace Hyperion {

#define SHADOW_VIEW_CACHE_MULTITHREADED 1

static constexpr TextureFormat PointLightShadowFormat = TextureFormat::RG16F;
static constexpr TextureFormat DirectionalLightShadowFormats[SMF_MAX] = {
    TextureFormat::RGBA8, // STANDARD
    TextureFormat::RGBA8, // PCF
    TextureFormat::RGBA8, // CONTACT_HARDENING
    TextureFormat::RG16F  // VSM
};

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

static RenderTargetDesc GetRenderTargetDesc(
    Light* light,
    ShaderDesc& outShaderDesc,
    EnumFlags<ViewFlags>& outViewFlags)
{
    outViewFlags = DefaultShadowViewFlags;

    const ShadowMapFilter shadowMapFilter = light->GetShadowMapFilter();

    RenderTargetDesc renderTargetDesc {};
    renderTargetDesc.extent = light->GetShadowMapDimensions();
    
    outShaderDesc.name = NAME("DrawShadowMap");
    outShaderDesc.properties.Add(s_shadowMapFilterProperties[shadowMapFilter]);

    switch (light->GetLightType())
    {
    case LightType::Point:
    {
        // Frustum culling for cubemap views not currently supported.
        outViewFlags |= ViewFlags::NO_FRUSTUM_CULLING | ViewFlags::COLLECT_ALL_ENTITIES;

        renderTargetDesc.numAttachments = 0;
        renderTargetDesc.numLayers = 6;

        // depth, depth^2 texture (for variance shadow map)
        AttachmentDesc& moments = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        moments.imageType = TextureType::Cubemap;
        moments.format = TextureFormat::RG16F;
        moments.loadOp = LoadOperation::CLEAR;
        moments.storeOp = StoreOperation::STORE;
        std::fill(std::begin(moments.clearColor), std::end(moments.clearColor), 1000.0f);

        AttachmentDesc& depth = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        depth.imageType = TextureType::Cubemap;
        depth.format = TextureFormat::D32F;
        depth.loadOp = LoadOperation::CLEAR;
        depth.storeOp = StoreOperation::STORE;

        outShaderDesc.name = NAME("DrawCubemap");

        outShaderDesc.properties = {};
        outShaderDesc.properties.Add(s_propModeShadows);

        break;
    }
    case LightType::Directional:
    {
        renderTargetDesc.numAttachments = 0;

        // depth, depth^2 texture (for variance shadow map)
        AttachmentDesc& moments = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        moments.format = DirectionalLightShadowFormats[shadowMapFilter];
        moments.imageType = TextureType::Texture2D;
        moments.loadOp = LoadOperation::CLEAR;
        moments.storeOp = StoreOperation::STORE;
        std::fill(std::begin(moments.clearColor), std::end(moments.clearColor), 1000.0f);

        AttachmentDesc& depth = renderTargetDesc.attachments[renderTargetDesc.numAttachments++];
        depth.format = TextureFormat::D32F;
        depth.imageType = TextureType::Texture2D;
        depth.loadOp = LoadOperation::CLEAR;
        depth.storeOp = StoreOperation::STORE;

        break;
    }
    default:
        // no shadow mapping impl
        break;
    }

    return renderTargetDesc;
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
        shadowMapCamera->SetNear(0.01f);
        shadowMapCamera->SetFar(light->GetRadius());

        shadowMapCamera->AddCameraController(MakeHandle<PerspectiveCameraController>());

        shadowMapCamera->SetDirection(Vec3f(0.0f, 0.0f, 1.0f));
        break;
    default:
        break;
    }

    InitObject(shadowMapCamera);

    return shadowMapCamera;
}

static ViewDesc GetViewDesc(Light* light, bool isStatic, uint32 cascadeIndex, Camera*& inOutCamera)
{
    if (!inOutCamera)
    {
        inOutCamera = CreateShadowCamera(light, cascadeIndex);
    }

    ViewDesc viewDesc {};

    viewDesc.viewport = {};
    viewDesc.viewport.extent = light->GetShadowMapDimensions();
    viewDesc.viewport.position = Vec2i::Zero();

    viewDesc.scenes = {};
    viewDesc.camera = inOutCamera;

    ShaderDesc shaderDesc;
    viewDesc.renderTargetDesc = GetRenderTargetDesc(light, shaderDesc, viewDesc.flags);
    
    RenderableAttributeSet overrideAttributes(
        MeshAttributes {},
        MaterialAttributes {
            .shaderName = shaderDesc.name,
            .shaderProperties = shaderDesc.properties,
            .cullFaces = light->GetShadowMapFilter() == SMF_VSM ? FCM_FRONT : FCM_BACK
        });

    viewDesc.overrideAttributes = overrideAttributes;

    if (light->GetLightFlags() & LightFlags::ShadowCacheStaticObjects)
    {
        viewDesc.flags &= ~ViewFlags::COLLECT_ALL_ENTITIES;

        if (isStatic)
        {
            viewDesc.flags |= ViewFlags::COLLECT_STATIC_ENTITIES;
        }
        else
        {
            viewDesc.flags |= ViewFlags::COLLECT_DYNAMIC_ENTITIES;
        }
    }

    return viewDesc;
}

struct ShadowViewCacheEntry
{
    Camera* camera;

    Array<View*> staticViews;
    Array<View*> dynamicViews;

    volatile int64 lastFrameUsed;
};

class ShadowViewCacheImpl
{
public:
    ~ShadowViewCacheImpl()
    {
#if SHADOW_VIEW_CACHE_MULTITHREADED
        TUniqueLock lock(mutex);
#endif

        for (auto& pair : cache)
        {
            ShadowViewCacheEntry& entry = pair.second;

            for (View* view : entry.staticViews)
            {
                if (view)
                {
                    view->Release();
                }
            }

            for (View* view : entry.dynamicViews)
            {
                if (view)
                {
                    view->Release();
                }
            }

            if (entry.camera)
            {
                entry.camera->Release();
            }
        }
    }

    HashMap<ShadowViewCacheKey, ShadowViewCacheEntry> cache;

#if SHADOW_VIEW_CACHE_MULTITHREADED
    SharedMutex mutex;
#endif
};

ShadowViewCache::ShadowViewCache()
    : m_impl(MakePimpl<ShadowViewCacheImpl>())
{
}

ShadowViewCache::~ShadowViewCache() = default;

HYP_NODISCARD View* ShadowViewCache::GetOrCreateShadowView(
    View* view,
    Light* light,
    uint32 cascadeIndex,
    bool isStatic) const
{
    Assert(view != nullptr && light != nullptr);

    View* outView = nullptr;

    ShadowViewCacheKey key {};
    key.view = view;
    key.light = light;;

#if SHADOW_VIEW_CACHE_MULTITHREADED
    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);
    TUniqueLock<SharedMutex> uniqueLock; // not locked yet
#else
    AssertOnThread(g_renderThread);
#endif

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        ShadowViewCacheEntry* entry = &it->second;
        AtomicExchange(&entry->lastFrameUsed, int64(GetFrameCounter()));

        auto* views = isStatic ? &entry->staticViews : &entry->dynamicViews;

        if (cascadeIndex >= entry->staticViews.Size() || !entry->camera)
        {
#if SHADOW_VIEW_CACHE_MULTITHREADED
            sharedLock.Reset();
            uniqueLock.Reset(m_impl->mutex);
#endif

            if (!entry->camera)
            {
                entry->camera = CreateShadowCamera(light, cascadeIndex);
            }

            if (cascadeIndex >= entry->staticViews.Size())
            {
                entry->staticViews.Resize(cascadeIndex + 1);
                entry->dynamicViews.Resize(cascadeIndex + 1);
            }
        }

        outView = (*views)[cascadeIndex];

        if (outView != nullptr)
        {
            return outView;
        }

#if SHADOW_VIEW_CACHE_MULTITHREADED
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

        views = isStatic ? &entry->staticViews : &entry->dynamicViews;
        outView = (*views)[cascadeIndex];

        if (!outView)
        {
            outView = new View(GetViewDesc(light, isStatic, cascadeIndex, entry->camera));
            InitObject(outView);

            (*views)[cascadeIndex] = outView;
        }
    }
    else
    {
#if SHADOW_VIEW_CACHE_MULTITHREADED
        sharedLock.Reset();
        uniqueLock.Reset(m_impl->mutex);
#endif

        ShadowViewCacheEntry& entry = m_impl->cache[key];
        AtomicExchange(&entry.lastFrameUsed, int64(GetFrameCounter()));

        if (cascadeIndex >= entry.staticViews.Size())
        {
            entry.staticViews.Resize(cascadeIndex + 1);
            entry.dynamicViews.Resize(cascadeIndex + 1);
        }

        auto& views = isStatic ? entry.staticViews : entry.dynamicViews;
        outView = views[cascadeIndex];

        if (!outView)
        {
            outView = new View(GetViewDesc(light, isStatic, cascadeIndex, entry.camera));
            InitObject(outView);

            views[cascadeIndex] = outView;
        }
    }

    return outView;
}

View* ShadowViewCache::TryGetShadowView(
    View* view,
    Light* light,
    uint32 cascadeIndex,
    bool isStatic) const
{
    ShadowViewCacheKey key {};
    key.view = view;
    key.light = light;;

#if SHADOW_VIEW_CACHE_MULTITHREADED
    TSharedLock<SharedMutex> sharedLock(m_impl->mutex);
    TUniqueLock<SharedMutex> uniqueLock; // not locked yet
#else
    AssertOnThread(g_renderThread);
#endif

    auto it = m_impl->cache.Find(key);

    if (it != m_impl->cache.End())
    {
        auto& shadowViews = isStatic ? it->second.staticViews : it->second.dynamicViews;

        if (cascadeIndex < shadowViews.Size())
        {
            return shadowViews[cascadeIndex];
        }
    }

    return nullptr;
}

} // namespace Hyperion

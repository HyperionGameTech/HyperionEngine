/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/EntityManager.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <EnvProbe.generated.inl>

namespace hyperion {

static constexpr EnumFlags<EnvProbeFlags> DefaultEnvProbeFlags[EPT_MAX] = {
    EPF_NONE,               // sky
    EPF_PARALLAX_CORRECTED, // reflection
    EPF_BAKED               // ambient
};

static FixedArray<Mat4f, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Mat4f, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Mat4f::LookAt(
            origin,
            origin + Texture::s_cubemapDirections[i].first,
            Texture::s_cubemapDirections[i].second);
    }

    return viewMatrices;
}

#pragma region EnvProbe

EnvProbe::EnvProbe()
    : EnvProbe(EPT_INVALID)
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType)
    : EnvProbe(envProbeType, BoundingBox(Vec3f(-25.0f), Vec3f(25.0f)), Vec2u { 256, 256 })
{
}

EnvProbe::EnvProbe(EnvProbeType envProbeType, const BoundingBox& aabb, const Vec2u& dimensions)
    : m_aabb(aabb),
      m_dimensions(dimensions),
      m_envProbeType(envProbeType),
      m_envProbeFlags(DefaultEnvProbeFlags[envProbeType]),
      m_cameraNear(0.05f),
      m_cameraFar(aabb.GetRadius()),
      m_needsRenderCounter(0),
      m_camera(nullptr),
      m_view(nullptr)
{
    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = !(m_envProbeFlags & EPF_BAKED);
}

void EnvProbe::SetEnvProbeFlags(EnumFlags<EnvProbeFlags> envProbeFlags)
{
    const uint32 changedFlags = envProbeFlags ^ m_envProbeFlags;

    if (!changedFlags)
    {
        return;
    }

    m_envProbeFlags = envProbeFlags;

    if (changedFlags & EPF_BAKED)
    {
        if (envProbeFlags[EPF_BAKED])
        {
            SetReceivesUpdate(false);
        }
        else
        {
            SetReceivesUpdate(true);
        }
    }

    SetNeedsRenderProxyUpdate();
}

bool EnvProbe::IsVisible(ObjId<Camera> cameraId) const
{
    return m_visibilityBits.Test(cameraId.ToIndex());
}

void EnvProbe::SetIsVisible(ObjId<Camera> cameraId, bool isVisible)
{
    const bool previousValue = m_visibilityBits.Test(cameraId.ToIndex());

    m_visibilityBits.Set(cameraId.ToIndex(), isVisible);

    if (isVisible != previousValue)
    {
        Invalidate();
    }
}

EnvProbe::~EnvProbe()
{
    AssertDebug(m_view == nullptr);

    SafeDelete(std::move(m_texture));

    if (m_camera)
    {
        RemoveChild(m_camera);
        m_camera = nullptr;
    }
}

void EnvProbe::Init()
{
    Entity::Init();

    if (!IsBaked())
    {
        Handle<Camera> camera = CreateObject<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            m_cameraNear, m_cameraFar);

        camera->SetName(NAME("EnvProbeCamera"));
        camera->SetViewMatrix(Mat4f::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));

        InitObject(camera);
        AddChild(camera);

        m_camera = camera;

        CreateView();

        if (ShouldComputePrefilteredEnvMap())
        {
            if (!m_texture)
            {
                m_texture = CreateObject<Texture>(TextureDesc {
                    TT_TEX2D,
                    TF_RGBA8,
                    Vec3u { m_dimensions, 1 },
                    TFM_LINEAR_MIPMAP,
                    TFM_LINEAR,
                    TWM_CLAMP_TO_EDGE,
                    1,
                    IU_STORAGE | IU_SAMPLED });

                m_texture->SetName(NAME_FMT("{}_{}_PrefilteredEnvMap", InstanceClass()->GetName(), GetUUID()));
            }
        }
    }

    InitObject(m_texture);

    SetReady(true);
}

void EnvProbe::OnAttachedToNode(Node* node)
{
    Entity::OnAttachedToNode(node);
}

void EnvProbe::OnDetachedFromNode(Node* node)
{
    Entity::OnDetachedFromNode(node);
}

void EnvProbe::OnAddedToWorld(World* world)
{
    Entity::OnAddedToWorld(world);

    SetNeedsRender(true);
}

void EnvProbe::OnRemovedFromWorld(World* world)
{
    Entity::OnRemovedFromWorld(world);
}

void EnvProbe::OnAddedToScene(Scene* scene)
{
    Entity::OnAddedToScene(scene);

    if (m_view)
    {
        m_view->AddScene(MakeStrongRef(scene));
    }

    Invalidate();
}

void EnvProbe::OnRemovedFromScene(Scene* scene)
{
    Entity::OnRemovedFromScene(scene);

    if (m_view)
    {
        m_view->RemoveScene(scene);
    }

    Invalidate();
}

void EnvProbe::OnTransformUpdated(const Transform& transform)
{
    Entity::OnTransformUpdated(transform);

    // set origin
    SetOrigin(transform.GetTranslation());
}

void EnvProbe::CreateView()
{
    if (IsBaked())
    {
        return;
    }

    AssertDebug(m_view == nullptr);
    AssertDebug(m_camera != nullptr);

    ViewOutputTargetDesc outputTargetDesc {
        .extent = Vec2u(m_dimensions),
        .attachments = {},
        .numViews = 6
    };

    if (IsReflectionProbe() || IsSkyProbe())
    {
        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_R10G10B10A2,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE });

        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_RG16F,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE });

        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_RG16F,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE,
            .clearColor = Vec4f(FLT16_MAX) });
    }

    outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
        .format = g_renderBackend->GetDefaultFormat(DIF_DEPTH),
        .imageType = TT_CUBEMAP,
        .loadOp = LoadOperation::CLEAR,
        .storeOp = StoreOperation::STORE });

    ShaderDefinition shaderDefinition;

    if (IsReflectionProbe())
    {
        shaderDefinition = ShaderDefinition(NAME("RenderToCubemap"),
            ShaderProperties(staticMeshVertexAttributes, { NAME("WRITE_NORMALS"), NAME("WRITE_MOMENTS") }));
    }
    else if (IsSkyProbe())
    {
        shaderDefinition = ShaderDefinition(NAME("RenderSky"), ShaderProperties(staticMeshVertexAttributes));
    }

    ViewDesc viewDesc {
        .flags = (OnlyCollectStaticEntities() ? ViewFlags::COLLECT_STATIC_ENTITIES : ViewFlags::COLLECT_ALL_ENTITIES)
            | ViewFlags::NO_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_PROBES
            | ViewFlags::SKIP_ENV_GRIDS
            | ViewFlags::NOT_MULTI_BUFFERED,
        .viewport = Viewport { .extent = m_dimensions, .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = {},
        .camera = m_camera,
        .overrideAttributes = RenderableAttributeSet(
            MeshAttributes {},
            MaterialAttributes {
                .shaderDefinition = shaderDefinition,
                .blendFunction = BlendFunction::AlphaBlending(),
                .cullFaces = FCM_NONE })
    };

    Handle<View> view = CreateObject<View>(viewDesc);
    InitObject(view);
    m_view = view;
}

void EnvProbe::SetAABB(const BoundingBox& aabb)
{
    HYP_SCOPE;

    if (m_aabb != aabb)
    {
        m_aabb = aabb;

        Invalidate();
    }
}

void EnvProbe::SetOrigin(const Vec3f& origin)
{
    HYP_SCOPE;

    if (IsAmbientProbe())
    {
        // ambient probes use the min point of the aabb as the origin,
        // so it can blend between 7 other probes
        const Vec3f extent = m_aabb.GetExtent();

        m_aabb.SetMin(origin);
        m_aabb.SetMax(origin + extent);
    }
    else
    {
        m_aabb.SetCenter(origin);
    }

    if (IsInitCalled() && !IsBaked())
    {
        AssertDebug(m_camera != nullptr);

        m_camera->SetViewMatrix(Mat4f::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));
    }

    Invalidate();

    SetNeedsRenderProxyUpdate();
}

void EnvProbe::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_gameThread);
    AssertReady();

    if (IsBaked())
    {
        return;
    }

    const BoundingBox worldAabb = GetWorldAABB();

    bool needsUpdate = false;

    Array<ObjId<Scene>, SceneTempAllocator> cacheKeysToRemove;
    cacheKeysToRemove.Reserve(m_cachedOctantHashCodes.Size());

    for (const KeyValuePair<ObjId<Scene>, HashCode>& kvp : m_cachedOctantHashCodes)
    {
        cacheKeysToRemove.PushBack(kvp.first);
    }

    for (const Handle<Scene>& scene : m_view->GetScenes())
    {
        auto applyFrustumCheck = [&]()
        {
            // don't bother with the check for sky
            if (IsA(SkyProbe::StaticClass()))
            {
                needsUpdate = true;
                return;
            }

            for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Camera>>())
            {
                Camera* camera = static_cast<Camera*>(entity);

                if (camera->GetFrustum().ContainsAABB(worldAabb))
                {
                    needsUpdate = true;
                    return;
                }
            };
        };

        cacheKeysToRemove.Erase(scene->Id());

        if (scene->GetSceneFlags() & SceneFlags::HAS_OCTREE)
        {
            HashCode octantHashCode = HashCode(0);

            const SceneOctree& octree = scene->GetOctree();

            SceneOctree const* octant = &octree;

            if (!octant)
            {
                continue;
            }

            if (OnlyCollectStaticEntities())
            {
                // clang-format off
                octantHashCode.Add(octant->GetOctantID().GetHashCode()
                    .Add(octant->GetEntryListHash<EntityTag::STATIC>())
                    .Add(octant->GetEntryListHash<EntityTag::LIGHT>()));
                // clang-format on
            }
            else
            {
                // clang-format off
                octantHashCode.Add(octree.GetOctantID().GetHashCode()
                    .Add(octree.GetEntryListHash<EntityTag::NONE>()));
                // clang-format on
            }

            auto it = m_cachedOctantHashCodes.Find(scene->Id());

            if (it == m_cachedOctantHashCodes.End())
            {
                if (!needsUpdate)
                    applyFrustumCheck();

                m_cachedOctantHashCodes[scene->Id()] = octantHashCode;

                continue;
            }

            if (it->second != octantHashCode)
            {
                if (!needsUpdate)
                    applyFrustumCheck();

                it->second = octantHashCode;

                continue;
            }
        }
        else
        {
            if (!needsUpdate)
                applyFrustumCheck();
        }
    }

    if (cacheKeysToRemove.Any())
    {
        for (ObjId<Scene> id : cacheKeysToRemove)
        {
            m_cachedOctantHashCodes.Erase(id);
        }
    }

    if (!needsUpdate)
    {
        return;
    }

    Assert(m_camera != nullptr);
    m_camera->Update(delta);

    GetWorld()->ProcessViewAsync(m_view);

    SetNeedsRender(true);
}

void EnvProbe::UpdateRenderProxy(RenderProxyEnvProbe* proxy)
{
    proxy->envProbe = WeakHandleFromThis();

    if (proxy->texture != m_texture)
    {
        // force texture to get rebound if we already have a texture but it has changed
        proxy->forceRebind = proxy->forceRebind || proxy->texture != nullptr;

        proxy->texture = m_texture;
    }

    EnvProbeShaderData& bufferData = proxy->bufferData;
    bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    bufferData.worldPosition = Vec4f(GetOrigin(), 1.0f);
    bufferData.cameraNear = m_cameraNear;
    bufferData.cameraFar = m_cameraFar;
    bufferData.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    bufferData.visibilityBits = m_visibilityBits.ToUInt64();
    bufferData.flags = uint32(m_envProbeFlags);

    const FixedArray<Mat4f, 6> viewMatrices = CreateCubemapMatrices(m_aabb, GetOrigin());

    Memory::MemCpy(bufferData.faceViewMatrices, viewMatrices.Data(), sizeof(EnvProbeShaderData::faceViewMatrices));
    Memory::MemCpy(bufferData.shData, &m_shData, sizeof(EnvProbeSphericalHarmonics::values));

    bufferData.positionInGrid = m_positionInGrid;
}

void EnvProbe::SetBakedTexture(const Handle<Texture>& texture)
{
    if (!IsBaked())
    {
        return;
    }

    if (m_texture != nullptr)
    {
        SafeDelete(std::move(m_texture));
    }

    m_texture = texture;

    if (IsInitCalled())
    {
        InitObject(m_texture);

        SetNeedsRenderProxyUpdate();
    }
}

#pragma endregion EnvProbe

#pragma region ReflectionProbe

#pragma endregion ReflectionProbe

#pragma region SkyProbe

void SkyProbe::Init()
{
    m_texture = CreateObject<Texture>(TextureDesc {
        TT_CUBEMAP,
        TF_RGBA16F, // @TODO smaller format
        Vec3u { m_dimensions.x, m_dimensions.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_texture->SetName(NAME_FMT("{}_SkyboxCubemap", Id()));

    EnvProbe::Init();
}

#pragma endregion SkyProbe

} // namespace hyperion

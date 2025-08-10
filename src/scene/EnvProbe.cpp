/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Matrix4, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemapDirections[i].first,
            Texture::cubemapDirections[i].second);
    }

    return viewMatrices;
}

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
      m_cameraNear(0.05f),
      m_cameraFar(aabb.GetRadius()),
      m_needsRenderCounter(0)
{
    m_entityInitInfo.canEverUpdate = true;
    m_entityInitInfo.receivesUpdate = !IsControlledByEnvGrid();
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
}

void EnvProbe::Init()
{
    Entity::Init();

    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]
        {
            DetachChild(m_camera);
            m_camera.Reset();
        }));

    if (!IsControlledByEnvGrid())
    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            m_cameraNear, m_cameraFar);

        m_camera->SetName(NAME("EnvProbeCamera"));
        m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));

        InitObject(m_camera);

        CreateView();
    }

    if (ShouldComputePrefilteredEnvMap())
    {
        if (!m_prefilteredEnvMap)
        {
            m_prefilteredEnvMap = CreateObject<Texture>(TextureDesc {
                TT_TEX2D,
                TF_RGBA8,
                Vec3u { 512, 512, 1 },
                TFM_LINEAR_MIPMAP,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE,
                1,
                IU_STORAGE | IU_SAMPLED });

            m_prefilteredEnvMap->SetName(NAME_FMT("{}_PrefilteredEnvMap", Id()));

            Assert(InitObject(m_prefilteredEnvMap));
        }
    }

    SetReady(true);
}

void EnvProbe::OnAttachedToNode(Node* node)
{
    Entity::OnAttachedToNode(node);

    AttachChild(m_camera);
}

void EnvProbe::OnDetachedFromNode(Node* node)
{
    Entity::OnDetachedFromNode(node);

    DetachChild(m_camera);
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

    if (m_view.IsValid())
    {
        m_view->AddScene(scene->HandleFromThis());
    }

    Invalidate();
}

void EnvProbe::OnRemovedFromScene(Scene* scene)
{
    Entity::OnRemovedFromScene(scene);

    if (m_view.IsValid())
    {
        m_view->RemoveScene(scene->HandleFromThis());
    }

    Invalidate();
}

void EnvProbe::CreateView()
{
    if (IsControlledByEnvGrid())
    {
        return;
    }

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
            .format = TF_R16,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE,
            .clearColor = MathUtil::Infinity<Vec4f>() });
    }
    else if (IsShadowProbe())
    {
        outputTargetDesc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_R16,
            .imageType = TT_CUBEMAP,
            .loadOp = LoadOperation::CLEAR,
            .storeOp = StoreOperation::STORE });
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
            ShaderProperties(staticMeshVertexAttributes, { NAME("ENV_PROBE"), NAME("WRITE_NORMALS"), NAME("WRITE_MOMENTS") }));
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

    m_view = CreateObject<View>(viewDesc);

    InitObject(m_view);
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

    Invalidate();
}

void EnvProbe::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (IsControlledByEnvGrid())
    {
        return;
    }

    HashCode octantHashCode = HashCode(0);

    for (const Handle<Scene>& scene : m_view->GetScenes())
    {
        AssertDebug(scene.IsValid());

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
    }

    if (m_octantHashCode == octantHashCode)
    {
        // return early so we don't need to set up async View collection
        return;
    }

    Assert(m_camera.IsValid());
    m_camera->Update(delta);

    GetWorld()->ProcessViewAsync(m_view);

    SetNeedsRender(true);

    m_octantHashCode = octantHashCode;
}

void EnvProbe::UpdateRenderProxy(RenderProxyEnvProbe* proxy)
{
    proxy->envProbe = WeakHandleFromThis();

    EnvProbeShaderData& bufferData = proxy->bufferData;
    bufferData.aabbMin = Vec4f(m_aabb.min, 1.0f);
    bufferData.aabbMax = Vec4f(m_aabb.max, 1.0f);
    bufferData.worldPosition = Vec4f(GetOrigin(), 1.0f);
    bufferData.cameraNear = m_cameraNear;
    bufferData.cameraFar = m_cameraFar;
    bufferData.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    bufferData.visibilityBits = m_visibilityBits.ToUInt64();
    bufferData.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | EnvProbeFlags::DIRTY;

    const FixedArray<Matrix4, 6> viewMatrices = CreateCubemapMatrices(m_aabb, GetOrigin());

    Memory::MemCpy(bufferData.faceViewMatrices, viewMatrices.Data(), sizeof(EnvProbeShaderData::faceViewMatrices));
    Memory::MemCpy(bufferData.sh.values, &m_shData, sizeof(EnvProbeSphericalHarmonics::values));

    bufferData.positionInGrid = m_positionInGrid;
}

#pragma region SkyProbe

void SkyProbe::Init()
{
    EnvProbe::Init();

    m_skyboxCubemap = CreateObject<Texture>(TextureDesc {
        TT_CUBEMAP,
        TF_R11G11B10F,
        Vec3u { m_dimensions.x, m_dimensions.y, 1 },
        TFM_LINEAR,
        TFM_LINEAR });

    m_prefilteredEnvMap->SetName(NAME_FMT("{}_SkyboxCubemap", Id()));

    InitObject(m_skyboxCubemap);
}

#pragma endregion SkyProbe

} // namespace hyperion

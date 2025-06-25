/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>
#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/Texture.hpp>

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion {

static const TextureFormat reflection_probe_format = TF_RGBA16F;
static const TextureFormat shadow_probe_format = TF_RG32F;

#pragma region Render commands

struct RENDER_COMMAND(RenderPointLightShadow)
    : RenderCommand
{
    RenderWorld* world;
    RenderEnvProbe* env_probe;

    RENDER_COMMAND(RenderPointLightShadow)(RenderWorld* world, RenderEnvProbe* env_probe)
        : world(world),
          env_probe(env_probe)
    {
        // world->IncRef();
        // env_probe->IncRef();
    }

    virtual ~RENDER_COMMAND(RenderPointLightShadow)() override
    {
        world->DecRef();
        env_probe->DecRef();
    }

    virtual RendererResult operator()() override
    {
        FrameBase* frame = g_rendering_api->GetCurrentFrame();
        RenderSetup render_setup { world, nullptr };

        env_probe->Render(frame, render_setup);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

EnvProbe::EnvProbe()
    : EnvProbe(EPT_INVALID)
{
}

EnvProbe::EnvProbe(EnvProbeType env_probe_type)
    : EnvProbe(env_probe_type, BoundingBox(Vec3f(-25.0f), Vec3f(25.0f)), Vec2u { 256, 256 })
{
}

EnvProbe::EnvProbe(EnvProbeType env_probe_type, const BoundingBox& aabb, const Vec2u& dimensions)
    : m_aabb(aabb),
      m_dimensions(dimensions),
      m_env_probe_type(env_probe_type),
      m_camera_near(0.05f),
      m_camera_far(aabb.GetRadius()),
      m_needs_render_counter(0),
      m_render_resource(nullptr)
{
    m_entity_init_info.receives_update = true;
}

bool EnvProbe::IsVisible(ID<Camera> camera_id) const
{
    return m_visibility_bits.Test(camera_id.ToIndex());
}

void EnvProbe::SetIsVisible(ID<Camera> camera_id, bool is_visible)
{
    const bool previous_value = m_visibility_bits.Test(camera_id.ToIndex());

    m_visibility_bits.Set(camera_id.ToIndex(), is_visible);

    if (is_visible != previous_value)
    {
        Invalidate();
    }
}

EnvProbe::~EnvProbe()
{
    if (m_render_resource != nullptr)
    {
        // TEMP!
        m_render_resource->DecRef();

        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }
}

void EnvProbe::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            m_camera.Reset();

            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    m_render_resource = AllocateResource<RenderEnvProbe>(this);

    if (!IsControlledByEnvGrid())
    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            m_camera_near, m_camera_far);

        m_camera->SetName(NAME("EnvProbeCamera"));
        m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));

        InitObject(m_camera);

        CreateView();

        m_render_resource->SetViewResourceHandle(TResourceHandle<RenderView>(m_view->GetRenderResource()));
    }

    if (ShouldComputePrefilteredEnvMap())
    {
        if (!m_prefiltered_env_map)
        {
            m_prefiltered_env_map = CreateObject<Texture>(TextureDesc {
                TT_TEX2D,
                TF_RGBA16F,
                Vec3u { 1024, 1024, 1 },
                TFM_LINEAR_MIPMAP,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE,
                1,
                IU_STORAGE | IU_SAMPLED });

            AssertThrow(InitObject(m_prefiltered_env_map));

            m_prefiltered_env_map->SetPersistentRenderResourceEnabled(true);
        }
    }

    EnvProbeShaderData buffer_data {};
    buffer_data.aabb_min = Vec4f(m_aabb.min, 1.0f);
    buffer_data.aabb_max = Vec4f(m_aabb.max, 1.0f);
    buffer_data.world_position = Vec4f(GetOrigin(), 1.0f);
    buffer_data.camera_near = m_camera_near;
    buffer_data.camera_far = m_camera_far;
    buffer_data.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    buffer_data.visibility_bits = m_visibility_bits.ToUInt64();
    buffer_data.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | EnvProbeFlags::DIRTY;

    m_render_resource->SetBufferData(buffer_data);

    // TEMP! Need to keep it alive since for right now we are reading renderresource's BufferData - will change w/ new proxy binding system!
    m_render_resource->IncRef();

    SetReady(true);
}

void EnvProbe::OnAddedToWorld(World* world)
{
    // if (m_view.IsValid())
    // {
    //     world->AddView(m_view);
    // }
}

void EnvProbe::OnRemovedFromWorld(World* world)
{
    // if (m_view.IsValid())
    // {
    //     world->RemoveView(m_view);
    // }
}

void EnvProbe::OnAddedToScene(Scene* scene)
{
    if (m_view.IsValid())
    {
        m_view->AddScene(scene->HandleFromThis());
    }

    Invalidate();
}

void EnvProbe::OnRemovedFromScene(Scene* scene)
{
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

    ViewOutputTargetDesc output_target_desc {
        .extent = Vec2u(m_dimensions),
        .attachments = {},
        .num_views = 6
    };

    if (IsReflectionProbe() || IsSkyProbe())
    {
        output_target_desc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = reflection_probe_format,
            .image_type = TT_CUBEMAP,
            .load_op = LoadOperation::CLEAR,
            .store_op = StoreOperation::STORE });

        output_target_desc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_RG16F,
            .image_type = TT_CUBEMAP,
            .load_op = LoadOperation::CLEAR,
            .store_op = StoreOperation::STORE });

        output_target_desc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = TF_RGBA16F,
            .image_type = TT_CUBEMAP,
            .load_op = LoadOperation::CLEAR,
            .store_op = StoreOperation::STORE,
            .clear_color = MathUtil::Infinity<Vec4f>() });
    }
    else if (IsShadowProbe())
    {
        output_target_desc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
            .format = shadow_probe_format,
            .image_type = TT_CUBEMAP,
            .load_op = LoadOperation::CLEAR,
            .store_op = StoreOperation::STORE });
    }

    output_target_desc.attachments.PushBack(ViewOutputTargetAttachmentDesc {
        .format = g_rendering_api->GetDefaultFormat(DIF_DEPTH),
        .image_type = TT_CUBEMAP,
        .load_op = LoadOperation::CLEAR,
        .store_op = StoreOperation::STORE });

    m_view = CreateObject<View>(ViewDesc {
        .flags = (OnlyCollectStaticEntities() ? ViewFlags::COLLECT_STATIC_ENTITIES : ViewFlags::COLLECT_ALL_ENTITIES)
            | ViewFlags::SKIP_FRUSTUM_CULLING
            | ViewFlags::SKIP_ENV_PROBES
            | ViewFlags::SKIP_ENV_GRIDS,
        .viewport = Viewport { .extent = m_dimensions, .position = Vec2i::Zero() },
        .output_target_desc = output_target_desc,
        .scenes = {},
        .camera = m_camera,
        .override_attributes = RenderableAttributeSet(
            MeshAttributes {},
            MaterialAttributes {
                .shader_definition = m_render_resource->GetShader()->GetCompiledShader()->GetDefinition(),
                .blend_function = BlendFunction::AlphaBlending(),
                .cull_faces = FCM_NONE }) });

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
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    AssertThrow(GetScene() != nullptr);

    const Octree& octree = GetScene()->GetOctree();

    // Ambient probes do not use their own render list,
    // instead they are attached to a grid which shares one
    if (!IsControlledByEnvGrid())
    {
        HashCode octant_hash_code = HashCode(0);

        if (OnlyCollectStaticEntities())
        {
            Octree const* octant = &GetScene()->GetOctree();

            if (!IsSkyProbe())
            {
                octree.GetFittingOctant(m_aabb, octant);

                if (!octant)
                {
                    HYP_LOG(EnvProbe, Warning, "No containing octant found for EnvProbe {} with AABB: {}", GetID(), m_aabb);
                }
            }

            if (octant)
            {
                octant_hash_code = octant->GetOctantID().GetHashCode().Add(octant->GetEntryListHash<EntityTag::STATIC>()).Add(octant->GetEntryListHash<EntityTag::LIGHT>());
            }
        }
        else
        {
            octant_hash_code = octree.GetOctantID().GetHashCode().Add(octree.GetEntryListHash<EntityTag::NONE>());
        }

        if (m_octant_hash_code == octant_hash_code && octant_hash_code.Value() != 0)
        {
            return;
        }

        AssertThrow(m_camera.IsValid());
        m_camera->Update(delta);

        m_view->UpdateVisibility();
        m_view->Update(delta);

        auto diff = m_view->GetLastCollectionResult();

        if (diff.NeedsUpdate() || m_octant_hash_code != octant_hash_code)
        {
            HYP_LOG(EnvProbe, Debug, "EnvProbe {} with AABB: {} has {} added, {} removed and {} changed entities", GetID(), m_aabb,
                diff.num_added, diff.num_removed, diff.num_changed);

            SetNeedsRender(true);
        }

        m_octant_hash_code = octant_hash_code;
    }

    EnvProbeShaderData buffer_data {};
    buffer_data.aabb_min = Vec4f(m_aabb.min, 1.0f);
    buffer_data.aabb_max = Vec4f(m_aabb.max, 1.0f);
    buffer_data.world_position = Vec4f(GetOrigin(), 1.0f);
    buffer_data.camera_near = m_camera_near;
    buffer_data.camera_far = m_camera_far;
    buffer_data.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    buffer_data.visibility_bits = m_visibility_bits.ToUInt64();
    buffer_data.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | EnvProbeFlags::DIRTY;

    m_render_resource->SetBufferData(buffer_data);

    if (IsShadowProbe())
    {
        GetWorld()->GetRenderResource().IncRef();
        m_render_resource->IncRef();
        PUSH_RENDER_COMMAND(RenderPointLightShadow, &GetWorld()->GetRenderResource(), m_render_resource);
    }
}

void EnvProbe::UpdateRenderProxy(IRenderProxy* proxy)
{
    RenderProxyEnvProbe* proxy_casted = static_cast<RenderProxyEnvProbe*>(proxy);
    proxy_casted->env_probe = WeakHandle<EnvProbe>(WeakHandleFromThis());

    EnvProbeShaderData& buffer_data = proxy_casted->buffer_data;
    buffer_data.aabb_min = Vec4f(m_aabb.min, 1.0f);
    buffer_data.aabb_max = Vec4f(m_aabb.max, 1.0f);
    buffer_data.world_position = Vec4f(GetOrigin(), 1.0f);
    buffer_data.camera_near = m_camera_near;
    buffer_data.camera_far = m_camera_far;
    buffer_data.dimensions = Vec2u { m_dimensions.x, m_dimensions.y };
    buffer_data.visibility_bits = m_visibility_bits.ToUInt64();
    buffer_data.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | EnvProbeFlags::DIRTY;
}

} // namespace hyperion

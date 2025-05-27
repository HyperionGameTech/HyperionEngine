/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(BindEnvProbe)
    : renderer::RenderCommand
{
    EnvProbeType env_probe_type;
    WeakHandle<EnvProbe> probe_weak;

    RENDER_COMMAND(BindEnvProbe)(EnvProbeType env_probe_type, const WeakHandle<EnvProbe>& probe_weak)
        : env_probe_type(env_probe_type),
          probe_weak(probe_weak)
    {
    }

    virtual ~RENDER_COMMAND(BindEnvProbe)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<EnvProbe> probe = probe_weak.Lock();

        if (!probe.IsValid())
        {
            HYPERION_RETURN_OK;
        }

        g_engine->GetRenderState()->BindEnvProbe(env_probe_type, TResourceHandle<EnvProbeRenderResource>(probe->GetRenderResource()));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindEnvProbe)
    : renderer::RenderCommand
{
    EnvProbeType env_probe_type;
    WeakHandle<EnvProbe> probe_weak;

    RENDER_COMMAND(UnbindEnvProbe)(EnvProbeType env_probe_type, const WeakHandle<EnvProbe>& probe_weak)
        : env_probe_type(env_probe_type),
          probe_weak(probe_weak)
    {
    }

    virtual ~RENDER_COMMAND(UnbindEnvProbe)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<EnvProbe> probe = probe_weak.Lock();

        if (!probe.IsValid())
        {
            HYPERION_RETURN_OK;
        }

        g_engine->GetRenderState()->UnbindEnvProbe(env_probe_type, &probe->GetRenderResource());

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

EnvProbe::EnvProbe()
    : EnvProbe(
          Handle<Scene> {},
          BoundingBox::Empty(),
          Vec2u { 1, 1 },
          EnvProbeType::ENV_PROBE_TYPE_INVALID)
{
}

EnvProbe::EnvProbe(
    const Handle<Scene>& parent_scene,
    const BoundingBox& aabb,
    const Vec2u& dimensions,
    EnvProbeType env_probe_type)
    : HypObject(),
      m_parent_scene(parent_scene),
      m_aabb(aabb),
      m_dimensions(dimensions),
      m_env_probe_type(env_probe_type),
      m_camera_near(0.05f),
      m_camera_far(aabb.GetRadius()),
      m_needs_update(true),
      m_needs_render_counter(0),
      m_render_resource(nullptr)
{
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
        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }
}

void EnvProbe::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            m_camera.Reset();

            if (m_render_resource != nullptr)
            {
                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }
        }));

    AssertThrow(m_parent_scene.IsValid());

    m_render_resource = AllocateResource<EnvProbeRenderResource>(this);
    m_render_resource->SetSceneResourceHandle(TResourceHandle<SceneRenderResource>(m_parent_scene->GetRenderResource()));

    if (!IsControlledByEnvGrid())
    {
        m_camera = CreateObject<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            m_camera_near, m_camera_far);

        m_camera->SetName(NAME("EnvProbeCamera"));
        m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));

        InitObject(m_camera);

        m_render_resource->SetCameraResourceHandle(TResourceHandle<CameraRenderResource>(m_camera->GetRenderResource()));

        CreateView();

        m_render_resource->SetViewResourceHandle(TResourceHandle<ViewRenderResource>(m_view->GetRenderResource()));
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

    SetReady(true);
}

void EnvProbe::CreateView()
{
    if (IsControlledByEnvGrid())
    {
        return;
    }

    m_view = CreateObject<View>(ViewDesc {
        .viewport = Viewport { .extent = Vec2i(m_dimensions), .position = Vec2i::Zero() },
        .scene = m_parent_scene,
        .camera = m_camera,
        .entity_collection_flags = (OnlyCollectStaticEntities() ? ViewEntityCollectionFlags::COLLECT_STATIC : ViewEntityCollectionFlags::COLLECT_ALL) | ViewEntityCollectionFlags::SKIP_FRUSTUM_CULLING,
        .override_attributes = RenderableAttributeSet(
            MeshAttributes {},
            MaterialAttributes {
                .shader_definition = m_render_resource->GetShader()->GetCompiledShader()->GetDefinition(),
                .blend_function = BlendFunction::AlphaBlending(),
                .cull_faces = FaceCullMode::NONE }) });

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

void EnvProbe::SetParentScene(const Handle<Scene>& parent_scene)
{
    HYP_SCOPE;

    if (m_parent_scene == parent_scene)
    {
        return;
    }

    m_parent_scene = parent_scene;

    if (IsInitCalled())
    {
        if (parent_scene.IsValid())
        {
            InitObject(parent_scene);

            CreateView();

            if (m_view.IsValid())
            {
                m_render_resource->SetViewResourceHandle(TResourceHandle<ViewRenderResource>(m_view->GetRenderResource()));
            }

            m_render_resource->SetSceneResourceHandle(TResourceHandle<SceneRenderResource>(parent_scene->GetRenderResource()));
        }
        else
        {
            m_render_resource->SetViewResourceHandle(TResourceHandle<ViewRenderResource>());
            m_render_resource->SetSceneResourceHandle(TResourceHandle<SceneRenderResource>());

            m_view.Reset();
        }
    }

    Invalidate();
}

void EnvProbe::EnqueueBind() const
{
    AssertReady();

    if (!IsControlledByEnvGrid())
    {
        PUSH_RENDER_COMMAND(BindEnvProbe, GetEnvProbeType(), WeakHandleFromThis());
    }
}

void EnvProbe::EnqueueUnbind() const
{
    if (!IsReady())
    {
        return;
    }

    if (!IsControlledByEnvGrid())
    {
        PUSH_RENDER_COMMAND(UnbindEnvProbe, GetEnvProbeType(), WeakHandleFromThis());
    }
}

void EnvProbe::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    // Ambient probes do not use their own render list,
    // instead they are attached to a grid which shares one
    if (!IsControlledByEnvGrid())
    {
        HashCode octant_hash_code = HashCode(0);

        if (OnlyCollectStaticEntities())
        {
            Octree const* octree = &m_parent_scene->GetOctree();

            if (!IsSkyProbe())
            {
                octree->GetFittingOctant(m_aabb, octree);

                if (!octree)
                {
                    HYP_LOG(EnvProbe, Warning, "No containing octant found for EnvProbe #{} with AABB: {}", GetID().Value(), m_aabb);
                }
            }

            if (octree)
            {
                octant_hash_code = octree->GetOctantID().GetHashCode().Add(octree->GetEntryListHash<EntityTag::STATIC>()).Add(octree->GetEntryListHash<EntityTag::LIGHT>());
            }
        }
        else
        {
            octant_hash_code = m_parent_scene->GetOctree().GetOctantID().GetHashCode().Add(m_parent_scene->GetOctree().GetEntryListHash<EntityTag::NONE>());
        }

        if (m_octant_hash_code == octant_hash_code && octant_hash_code.Value() != 0)
        {
            return;
        }

        AssertThrow(m_camera.IsValid());
        m_camera->Update(delta);

        AssertThrow(m_view.IsValid());
        m_view->Update(delta);

        typename RenderProxyTracker::Diff diff = m_view->GetLastCollectionResult();

        if (diff.NeedsUpdate() || m_octant_hash_code != octant_hash_code)
        {
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
}

} // namespace hyperion

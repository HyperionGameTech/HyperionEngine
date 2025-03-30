/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EnvProbe.hpp>

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(BindEnvProbe) : renderer::RenderCommand
{
    EnvProbeType            env_probe_type;
    WeakHandle<EnvProbe>    probe_weak;

    RENDER_COMMAND(BindEnvProbe)(EnvProbeType env_probe_type, const WeakHandle<EnvProbe> &probe_weak)
        : env_probe_type(env_probe_type),
          probe_weak(probe_weak)
    {
    }

    virtual ~RENDER_COMMAND(BindEnvProbe)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<EnvProbe> probe = probe_weak.Lock();

        if (!probe.IsValid()) {
            HYPERION_RETURN_OK;
        }

        g_engine->GetRenderState()->BindEnvProbe(env_probe_type, TResourceHandle<EnvProbeRenderResource>(probe->GetRenderResource()));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindEnvProbe) : renderer::RenderCommand
{
    EnvProbeType            env_probe_type;
    WeakHandle<EnvProbe>    probe_weak;

    RENDER_COMMAND(UnbindEnvProbe)(EnvProbeType env_probe_type, const WeakHandle<EnvProbe> &probe_weak)
        : env_probe_type(env_probe_type),
          probe_weak(probe_weak)
    {
    }

    virtual ~RENDER_COMMAND(UnbindEnvProbe)() override = default;

    virtual RendererResult operator()() override
    {
        Handle<EnvProbe> probe = probe_weak.Lock();

        if (!probe.IsValid()) {
            HYPERION_RETURN_OK;
        }

        g_engine->GetRenderState()->UnbindEnvProbe(env_probe_type, &probe->GetRenderResource());

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

EnvProbe::EnvProbe()
    : EnvProbe(
          Handle<Scene> { },
          BoundingBox::Empty(),
          Vec2u { 1, 1 },
          EnvProbeType::ENV_PROBE_TYPE_INVALID
      )
{
}

EnvProbe::EnvProbe(
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb,
    const Vec2u &dimensions,
    EnvProbeType env_probe_type
) : HypObject(),
    m_parent_scene(parent_scene),
    m_aabb(aabb),
    m_dimensions(dimensions),
    m_env_probe_type(env_probe_type),
    m_camera_near(0.001f),
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

    if (is_visible != previous_value) {
        SetNeedsUpdate(true);
    }
}

EnvProbe::~EnvProbe()
{
    if (m_render_resource != nullptr) {
        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }
}

void EnvProbe::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        m_camera.Reset();

        if (m_render_resource != nullptr) {
            FreeResource(m_render_resource);

            m_render_resource = nullptr;
        }
    }));

    if (!IsControlledByEnvGrid()) {
        AssertThrow(m_parent_scene.IsValid());

        m_camera = CreateObject<Camera>(
            90.0f,
            -int(m_dimensions.x), int(m_dimensions.y),
            m_camera_near, m_camera_far
        );

        m_camera->SetName(NAME("EnvProbeCamera"));
        m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));

        InitObject(m_camera);
    }

    m_render_resource = AllocateResource<EnvProbeRenderResource>(this);

    EnvProbeShaderData buffer_data { };
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

void EnvProbe::SetAABB(const BoundingBox &aabb)
{
    if (m_aabb != aabb) {
        m_aabb = aabb;

        SetNeedsUpdate(true);
    }
}

void EnvProbe::SetOrigin(const Vec3f &origin)
{
    if (IsAmbientProbe()) {
        // ambient probes use the min point of the aabb as the origin,
        // so it can blend between 7 other probes
        const Vec3f extent = m_aabb.GetExtent();

        m_aabb.SetMin(origin);
        m_aabb.SetMax(origin + extent);
    } else {
        m_aabb.SetCenter(origin);
    }

    if (IsInitCalled()) {
        SetNeedsUpdate(true);
    }
}

void EnvProbe::EnqueueBind() const
{
    AssertReady();

    if (!IsControlledByEnvGrid()) {
        PUSH_RENDER_COMMAND(BindEnvProbe, GetEnvProbeType(), WeakHandleFromThis());
    }
}

void EnvProbe::EnqueueUnbind() const
{
    AssertReady();

    if (!IsControlledByEnvGrid()) {
        PUSH_RENDER_COMMAND(UnbindEnvProbe, GetEnvProbeType(), WeakHandleFromThis());
    }
}

void EnvProbe::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    // Check if octree has changes, and we need to re-render.

    Octree const *octree = &m_parent_scene->GetOctree();
    octree->GetFittingOctant(m_aabb, octree);
    
    HashCode octant_hash_code = octree->GetOctantID().GetHashCode();
    
    if (OnlyCollectStaticEntities()) {
        // Static entities and lights changing affect the probe
        octant_hash_code = octant_hash_code
            .Add(octree->GetEntryListHash<EntityTag::STATIC>())
            .Add(octree->GetEntryListHash<EntityTag::LIGHT>());
    } else {
        octant_hash_code = octant_hash_code
            .Add(octree->GetEntryListHash<EntityTag::NONE>());
    }

    if (m_octant_hash_code != octant_hash_code) {
        SetNeedsUpdate(true);

        m_octant_hash_code = octant_hash_code;
    }

    if (!NeedsUpdate()) {
        return;
    }

    // Ambient probes do not use their own render list,
    // instead they are attached to a grid which shares one
    if (!IsControlledByEnvGrid()) {
        AssertThrow(m_camera.IsValid());

        m_camera->Update(delta);

        if (OnlyCollectStaticEntities()) {
            m_parent_scene->CollectStaticEntities(
                m_render_resource->GetRenderCollector(),
                m_camera,
                RenderableAttributeSet(
                    MeshAttributes { },
                    MaterialAttributes {
                        .shader_definition  = m_render_resource->GetShader()->GetCompiledShader()->GetDefinition(),
                        .blend_function     = BlendFunction::AlphaBlending(),
                        .cull_faces         = FaceCullMode::NONE
                    }
                ),
                true // skip frustum culling
            );
        } else {
            // // Calculate visibility of entities in the octree
            // m_parent_scene->GetOctree().CalculateVisibility(m_camera.Get());

            m_parent_scene->CollectEntities(
                m_render_resource->GetRenderCollector(),
                m_camera,
                RenderableAttributeSet(
                    MeshAttributes { },
                    MaterialAttributes {
                        .shader_definition  = m_render_resource->GetShader()->GetCompiledShader()->GetDefinition(),
                        .blend_function     = BlendFunction::AlphaBlending(),
                        .cull_faces         = FaceCullMode::NONE
                    }
                ),
                true // skip frustum culling (for now, until Camera can have multiple frustums for cubemaps)
            );
        }
    }

    EnvProbeShaderData buffer_data { };
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

    SetNeedsUpdate(false);
    SetNeedsRender(true);
}

namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, 1, sizeof(EnvProbeShaderData) * max_env_probes, false);
HYP_DESCRIPTOR_SSBO(Scene, CurrentEnvProbe, 1, sizeof(EnvProbeShaderData), true);

} // namespace renderer

} // namespace hyperion

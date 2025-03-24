/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EnvProbe.hpp>

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderLight.hpp>
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

static const InternalFormat reflection_probe_format = InternalFormat::RGBA8;
static const InternalFormat shadow_probe_format = InternalFormat::RG32F;

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
          EnvProbeType::ENV_PROBE_TYPE_INVALID,
          nullptr
      )
{
}

EnvProbe::EnvProbe(
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb,
    const Vec2u &dimensions,
    EnvProbeType env_probe_type
) : EnvProbe(
        parent_scene,
        aabb,
        dimensions,
        env_probe_type,
        nullptr
    )
{
}

EnvProbe::EnvProbe(
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb,
    const Vec2u &dimensions,
    EnvProbeType env_probe_type,
    const ShaderRef &custom_shader
) : HypObject(),
    m_parent_scene(parent_scene),
    m_aabb(aabb),
    m_dimensions(dimensions),
    m_env_probe_type(env_probe_type),
    m_shader(custom_shader),
    m_camera_near(0.001f),
    m_camera_far(aabb.GetRadius()),
    m_needs_update(true),
    m_needs_render_counter(0),
    m_is_rendered { false },
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
    SafeRelease(std::move(m_framebuffer));

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
        m_texture.Reset();
        m_shader.Reset();

        SafeRelease(std::move(m_framebuffer));

        if (m_render_resource != nullptr) {
            FreeResource(m_render_resource);

            m_render_resource = nullptr;
        }
    }));

    if (!IsControlledByEnvGrid()) {
        if (IsShadowProbe()) {
            m_texture = CreateObject<Texture>(
                TextureDesc {
                    ImageType::TEXTURE_TYPE_CUBEMAP,
                    shadow_probe_format,
                    Vec3u { m_dimensions.x, m_dimensions.y, 1 },
                    FilterMode::TEXTURE_FILTER_NEAREST,
                    FilterMode::TEXTURE_FILTER_NEAREST
                }
            );
        } else {
            m_texture = CreateObject<Texture>(
                TextureDesc {
                    ImageType::TEXTURE_TYPE_CUBEMAP,
                    reflection_probe_format,
                    Vec3u { m_dimensions.x, m_dimensions.y, 1 },
                    IsReflectionProbe() ? FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP : FilterMode::TEXTURE_FILTER_LINEAR,
                    FilterMode::TEXTURE_FILTER_LINEAR
                }
            );
        }

        AssertThrow(InitObject(m_texture));

        CreateShader();
        CreateFramebuffer();

        AssertThrow(m_parent_scene.IsValid());

        {
            m_camera = CreateObject<Camera>(
                90.0f,
                -int(m_dimensions.x), int(m_dimensions.y),
                m_camera_near, m_camera_far
            );

            m_camera->SetName(NAME("EnvProbeCamera"));
            m_camera->SetViewMatrix(Matrix4::LookAt(Vec3f(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vec3f(0.0f, 1.0f, 0.0f)));
            m_camera->SetFramebuffer(m_framebuffer);

            InitObject(m_camera);
        }
    }

    m_render_resource = AllocateResource<EnvProbeRenderResource>(
        this,
        !IsControlledByEnvGrid()
            ? TResourceHandle<CameraRenderResource>(m_camera->GetRenderResource())
            : TResourceHandle<CameraRenderResource>()
    );
    
    EnvProbeShaderData buffer_data { };
    buffer_data.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | (NeedsRender() ? EnvProbeFlags::DIRTY : EnvProbeFlags::NONE);
    buffer_data.world_position = Vec4f(GetOrigin(), 1.0f);
    buffer_data.aabb_min = Vec4f(m_aabb.GetMin(), 1.0f);
    buffer_data.aabb_max = Vec4f(m_aabb.GetMax(), 1.0f);

    m_render_resource->SetBufferData(buffer_data);

    SetReady(true);
}

void EnvProbe::CreateShader()
{
    // If a custom shader is provided, use that instead.
    if (m_shader.IsValid()) {
        return;
    }

    switch (m_env_probe_type) {
    case EnvProbeType::ENV_PROBE_TYPE_REFLECTION:
        m_shader = g_shader_manager->GetOrCreate({
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_REFLECTION" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_SKY:
        m_shader = g_shader_manager->GetOrCreate({
            NAME("RenderSky"),
            ShaderProperties(static_mesh_vertex_attributes)
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_SHADOW:
        m_shader = g_shader_manager->GetOrCreate({
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_SHADOWS" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_AMBIENT:
        // Do nothing
        return;
    default:
        AssertThrow(false);
    }
}

void EnvProbe::CreateFramebuffer()
{
    m_framebuffer = MakeRenderObject<Framebuffer>(
        m_dimensions,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    InternalFormat format = InternalFormat::NONE;

    if (IsReflectionProbe() || IsSkyProbe()) {
        format = reflection_probe_format;
    } else if (IsShadowProbe()) {
        format = shadow_probe_format;
    }

    AssertThrowMsg(format != InternalFormat::NONE, "Invalid attachment format for EnvProbe");

    AttachmentRef color_attachment = m_framebuffer->AddAttachment(
        0,
        format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    if (IsShadowProbe()) {
        color_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());
    }

    m_framebuffer->AddAttachment(
        1,
        g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    DeferCreate(m_framebuffer, g_engine->GetGPUDevice());
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
        AssertThrow(m_shader.IsValid());

        m_camera->Update(delta);

        if (OnlyCollectStaticEntities()) {
            m_parent_scene->CollectStaticEntities(
                m_render_resource->GetRenderCollector(),
                m_camera,
                RenderableAttributeSet(
                    MeshAttributes { },
                    MaterialAttributes {
                        .shader_definition  = m_shader->GetCompiledShader()->GetDefinition(),
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
                        .shader_definition  = m_shader->GetCompiledShader()->GetDefinition(),
                        .blend_function     = BlendFunction::AlphaBlending(),
                        .cull_faces         = FaceCullMode::NONE
                    }
                ),
                true // skip frustum culling (for now, until Camera can have multiple frustums for cubemaps)
            );
        }
    }

    EnvProbeShaderData buffer_data { };
    buffer_data.flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
        | (NeedsRender() ? EnvProbeFlags::DIRTY : EnvProbeFlags::NONE);
    buffer_data.world_position = Vec4f(GetOrigin(), 1.0f);
    buffer_data.aabb_min = Vec4f(m_aabb.GetMin(), 1.0f);
    buffer_data.aabb_max = Vec4f(m_aabb.GetMax(), 1.0f);

    m_render_resource->SetBufferData(buffer_data);

    SetNeedsUpdate(false);
    SetNeedsRender(true);
}

namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, 1, sizeof(EnvProbeShaderData) * max_env_probes, false);
HYP_DESCRIPTOR_SSBO(Scene, CurrentEnvProbe, 1, sizeof(EnvProbeShaderData), true);

} // namespace renderer

} // namespace hyperion

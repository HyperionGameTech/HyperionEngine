/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/EnvProbe.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <Engine.hpp>

namespace hyperion {

static const InternalFormat reflection_probe_format = InternalFormat::RGBA16F;
static const InternalFormat shadow_probe_format = InternalFormat::RG32F;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox &aabb, const Vec3f &origin);

#pragma region Render commands

struct RENDER_COMMAND(UpdateEnvProbeDrawProxy) : renderer::RenderCommand
{
    EnvProbe            &env_probe;
    EnvProbeDrawProxy   proxy;

    RENDER_COMMAND(UpdateEnvProbeDrawProxy)(EnvProbe &env_probe, const EnvProbeDrawProxy &proxy)
        : env_probe(env_probe),
          proxy(proxy)
    {
    }

    virtual RendererResult operator()() override
    {
        // update m_draw_proxy on render thread.
        env_probe.m_proxy = proxy;

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(BindEnvProbe) : renderer::RenderCommand
{
    EnvProbeType    env_probe_type;
    ID<EnvProbe>    id;

    RENDER_COMMAND(BindEnvProbe)(EnvProbeType env_probe_type, ID<EnvProbe> id)
        : env_probe_type(env_probe_type),
          id(id)
    {
    }

    virtual ~RENDER_COMMAND(BindEnvProbe)() override = default;

    virtual RendererResult operator()() override
    {
        g_engine->GetRenderState()->BindEnvProbe(env_probe_type, id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindEnvProbe) : renderer::RenderCommand
{
    EnvProbeType    env_probe_type;
    ID<EnvProbe>    id;

    RENDER_COMMAND(UnbindEnvProbe)(EnvProbeType env_probe_type, ID<EnvProbe> id)
        : env_probe_type(env_probe_type),
          id(id)
    {
    }

    virtual ~RENDER_COMMAND(UnbindEnvProbe)() override = default;

    virtual RendererResult operator()() override
    {
        g_engine->GetRenderState()->UnbindEnvProbe(env_probe_type, id);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox &aabb, const Vec3f &origin)
{
    FixedArray<Matrix4, 6> view_matrices;

    for (uint32 i = 0; i < 6; i++) {
        view_matrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemap_directions[i].first,
            Texture::cubemap_directions[i].second
        );
    }

    return view_matrices;
}

void EnvProbe::UpdateRenderData(
    uint32 texture_slot,
    uint32 grid_slot,
    const Vec3u &grid_size
)
{
    const BoundingBox &aabb = m_proxy.aabb;
    const Vec3f world_position = m_proxy.world_position;

    const FixedArray<Matrix4, 6> view_matrices = CreateCubemapMatrices(aabb, world_position);

    EnvProbeShaderData data {
        .face_view_matrices = {
            view_matrices[0],
            view_matrices[1],
            view_matrices[2],
            view_matrices[3],
            view_matrices[4],
            view_matrices[5]
        },
        .aabb_max           = Vec4f(aabb.max, 1.0f),
        .aabb_min           = Vec4f(aabb.min, 1.0f),
        .world_position     = Vec4f(world_position, 1.0f),
        .texture_index      = texture_slot,
        .flags              = m_proxy.flags,
        .camera_near        = m_proxy.camera_near,
        .camera_far         = m_proxy.camera_far,
        .dimensions         = m_dimensions,
        .position_in_grid   = grid_slot != ~0u
            ? Vec4i {
                  int32(grid_slot % grid_size.x),
                  int32((grid_slot % (grid_size.x * grid_size.y)) / grid_size.x),
                  int32(grid_slot / (grid_size.x * grid_size.y)),
                  int32(grid_slot)
              }
            : Vec4i::Zero(),
        .position_offset    = Vec4i::Zero()
    };

    g_engine->GetRenderData()->env_probes->Set(GetID().ToIndex(), data);
}

EnvProbe::EnvProbe() : EnvProbe(
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
    m_is_rendered { false }
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
}

void EnvProbe::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        m_render_collector.Reset();
        m_camera.Reset();
        m_texture.Reset();
        m_shader.Reset();

        SafeRelease(std::move(m_framebuffer));
    }));

    m_proxy = EnvProbeDrawProxy {
        .id = GetID(),
        .aabb = m_aabb,
        .world_position = GetOrigin(),
        .camera_near = m_camera_near,
        .camera_far = m_camera_far,
        .flags = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
            | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
            | (NeedsRender() ? EnvProbeFlags::DIRTY : EnvProbeFlags::NONE),
        .grid_slot = m_grid_slot
    };

    m_view_matrices = CreateCubemapMatrices(m_aabb, GetOrigin());

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
            m_camera->SetViewMatrix(Matrix4::LookAt(Vector3(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vector3(0.0f, 1.0f, 0.0f)));
            m_camera->SetFramebuffer(m_framebuffer);

            InitObject(m_camera);

            m_render_collector.SetCamera(m_camera);
        }
    }

    //SetNeedsUpdate(false);

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

    m_framebuffer->AddAttachment(
        0,
        format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

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
        PUSH_RENDER_COMMAND(BindEnvProbe, GetEnvProbeType(), GetID());
    }
}

void EnvProbe::EnqueueUnbind() const
{
    AssertReady();

    if (!IsControlledByEnvGrid()) {
        PUSH_RENDER_COMMAND(UnbindEnvProbe, GetEnvProbeType(), GetID());
    }
}

void EnvProbe::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);
    AssertReady();

    // Check if octree has changes, and we need to re-render.

    Octree const *octree = &m_parent_scene->GetOctree();
    m_parent_scene->GetOctree().GetFittingOctant(m_aabb, octree);
    
    HashCode octant_hash;
    
    if (OnlyCollectStaticEntities()) {
        // Static entities and lights changing affect the probe
        octant_hash = octree->GetEntryListHash<EntityTag::STATIC>()
            .Add(octree->GetEntryListHash<EntityTag::LIGHT>());
    } else {
        octant_hash = octree->GetEntryListHash<EntityTag::NONE>();
    }

    if (m_octant_hash_code != octant_hash) {
        SetNeedsUpdate(true);

        m_octant_hash_code = octant_hash;
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
                m_render_collector,
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
                m_render_collector,
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

    PUSH_RENDER_COMMAND(UpdateEnvProbeDrawProxy, *this, EnvProbeDrawProxy {
        .id             = GetID(),
        .aabb           = m_aabb,
        .world_position = GetOrigin(),
        .camera_near    = m_camera_near,
        .camera_far     = m_camera_far,
        .flags          = (IsReflectionProbe() ? EnvProbeFlags::PARALLAX_CORRECTED : EnvProbeFlags::NONE)
                        | (IsShadowProbe() ? EnvProbeFlags::SHADOW : EnvProbeFlags::NONE)
                        | EnvProbeFlags::DIRTY,
        .grid_slot      = m_grid_slot
    });

    SetNeedsUpdate(false);
    SetNeedsRender(true);
}

void EnvProbe::Render(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    if (IsControlledByEnvGrid()) {
        HYP_LOG(EnvProbe, Warning, "EnvProbe #{} is controlled by an EnvGrid, but Render() is being called!", GetID().Value());

        return;
    }
    
    // @FIXME!

    //if (!NeedsRender()) {
    //    return;
    //}

    AssertThrow(m_texture.IsValid());

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint32 frame_index = frame->GetFrameIndex();

    RendererResult result;

    EnvProbeIndex probe_index;

    const auto &env_probes = g_engine->GetRenderState()->bound_env_probes[GetEnvProbeType()];

    {
        const auto it = env_probes.Find(GetID());

        if (it != env_probes.End()) {
            if (!it->second.HasValue()) {
                HYP_LOG(EnvProbe, Warning, "Env Probe #{} (type: {}) has no value set for texture slot!",
                    GetID().Value(), GetEnvProbeType());

                return;
            }

            // don't care about position in grid if it is not controlled by one.
            // set it such that the uint32 value of probe_index
            // would be the held value.
            probe_index = EnvProbeIndex(
                Vec3u { 0, 0, it->second.Get() },
                Vec3u { 0, 0, 0 }
            );
        }

        if (probe_index == ~0u) {
            HYP_LOG(EnvProbe, Warning, "Env Probe #{} (type: {}) not found in render state bound env probe IDs",
                GetID().Value(), GetEnvProbeType());

            return;
        }
    }

    BindToIndex(probe_index);
    
    TResourceHandle<LightRenderResources> *light_render_resources_handle = nullptr;

    if (m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_SKY) {
        // Find a directional light to use for the sky
        // @TODO Support selecting a specific light for the EnvProbe

        auto &directional_lights = g_engine->GetRenderState()->bound_lights[uint32(LightType::DIRECTIONAL)];

        if (directional_lights.Any()) {
            light_render_resources_handle = &directional_lights[0];
        }

        if (!light_render_resources_handle) {
            HYP_LOG(EnvProbe, Warning, "No directional light found for Sky EnvProbe #{}", GetID().Value());
        }
    }

    {
        g_engine->GetRenderState()->SetActiveEnvProbe(GetID());

        if (light_render_resources_handle != nullptr) {
            g_engine->GetRenderState()->SetActiveLight(**light_render_resources_handle);
        }

        g_engine->GetRenderState()->BindScene(m_parent_scene.Get());
        g_engine->GetRenderState()->BindCamera(m_camera.Get());

        m_render_collector.CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT)),
            nullptr
        );

        m_render_collector.ExecuteDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT))
        );

        g_engine->GetRenderState()->UnbindCamera(m_camera.Get());
        g_engine->GetRenderState()->UnbindScene(m_parent_scene.Get());

        if (light_render_resources_handle != nullptr) {
            g_engine->GetRenderState()->UnsetActiveLight();
        }

        g_engine->GetRenderState()->UnsetActiveEnvProbe();
    }

    const ImageRef &framebuffer_image = m_framebuffer->GetAttachment(0)->GetImage();

    framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    m_texture->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

    m_texture->GetImage()->Blit(command_buffer, framebuffer_image);

    if (m_texture->HasMipmaps()) {
        HYPERION_PASS_ERRORS(
            m_texture->GetImage()->GenerateMipmaps(g_engine->GetGPUDevice(), command_buffer),
            result
        );
    }

    framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    m_texture->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    m_is_rendered.Set(true, MemoryOrder::RELEASE);

    HYPERION_ASSERT_RESULT(result);

    SetNeedsRender(false);
}

void EnvProbe::UpdateRenderData(bool set_texture)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    AssertThrow(m_bound_index.GetProbeIndex() != ~0u);

    if (IsControlledByEnvGrid()) {
        AssertThrow(m_grid_slot != ~0u);
    }

    const uint32 texture_slot = IsControlledByEnvGrid() ? ~0u : m_bound_index.GetProbeIndex();

    { // build proxy flags
        const EnumFlags<ShadowFlags> shadow_flags = IsShadowProbe() ? ShadowFlags::VSM : ShadowFlags::NONE;

        if (NeedsRender()) {
            m_proxy.flags |= EnvProbeFlags::DIRTY;
        }

        m_proxy.flags |= uint32(shadow_flags) << 3;
    }

    UpdateRenderData(
        texture_slot,
        IsControlledByEnvGrid() ? m_grid_slot : ~0u,
        IsControlledByEnvGrid() ? m_bound_index.grid_size : Vec3u { }
    );

    // update cubemap texture in array of bound env probes
    if (set_texture) {
        AssertThrow(texture_slot != ~0u);
        AssertThrow(m_texture.IsValid());

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            switch (GetEnvProbeType()) {
            case ENV_PROBE_TYPE_REFLECTION:
            case ENV_PROBE_TYPE_SKY: // fallthrough
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("EnvProbeTextures"), texture_slot, m_texture->GetImageView());

                break;
            case ENV_PROBE_TYPE_SHADOW:
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                    ->SetElement(NAME("PointLightShadowMapTextures"), texture_slot, m_texture->GetImageView());

                break;
            default:
                break;
            }
        }
    }
}

void EnvProbe::BindToIndex(const EnvProbeIndex &probe_index)
{
    bool set_texture = true;

    if (IsControlledByEnvGrid()) {
        if (probe_index.GetProbeIndex() >= max_bound_ambient_probes) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound ambient probes", probe_index.GetProbeIndex());
        }

        set_texture = false;
    } else if (IsReflectionProbe() || IsSkyProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_reflection_probes) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound reflection probes", probe_index.GetProbeIndex());

            set_texture = false;
        }
    } else if (IsShadowProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_point_shadow_maps) {
            HYP_LOG(EnvProbe, Warning, "Probe index ({}) out of range of max bound shadow map probes", probe_index.GetProbeIndex());

            set_texture = false;
        }
    }

    m_bound_index = probe_index;

    if (IsReady()) {
        if (Threads::IsOnThread(ThreadName::THREAD_RENDER)) {
            UpdateRenderData(set_texture);
        } else {
            struct RENDER_COMMAND(UpdateEnvProbeRenderData) : renderer::RenderCommand
            {
                EnvProbe    &env_probe;
                bool        set_texture;

                RENDER_COMMAND(UpdateEnvProbeRenderData)(EnvProbe &env_probe, bool set_texture)
                    : env_probe(env_probe),
                      set_texture(set_texture)
                {
                }

                virtual ~RENDER_COMMAND(UpdateEnvProbeRenderData)() override = default;

                virtual RendererResult operator()() override
                {
                    env_probe.UpdateRenderData(set_texture);

                    return RendererResult { };
                }
            };

            PUSH_RENDER_COMMAND(UpdateEnvProbeRenderData, *this, set_texture);

            HYP_SYNC_RENDER();
        }
    }
}

namespace renderer {

HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, 1, sizeof(EnvProbeShaderData) * max_env_probes, false);
HYP_DESCRIPTOR_SSBO(Scene, CurrentEnvProbe, 1, sizeof(EnvProbeShaderData), true);

} // namespace renderer

} // namespace hyperion

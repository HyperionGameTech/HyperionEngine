/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/EnvProbe.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <Engine.hpp>

namespace hyperion {

class EnvProbe;

using renderer::Result;

static const InternalFormat reflection_probe_format = InternalFormat::RGBA16F;
static const InternalFormat shadow_probe_format = InternalFormat::RG32F;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox &aabb, const Vec3f &origin);

#pragma region Render commands

Result RENDER_COMMAND(UpdateEnvProbeDrawProxy)::operator()()
{
    // update m_draw_proxy on render thread.
    env_probe.m_draw_proxy = draw_proxy;

    HYPERION_RETURN_OK;
}

struct RENDER_COMMAND(BindEnvProbe) : renderer::RenderCommand
{
    EnvProbeType env_probe_type;
    ID<EnvProbe> id;

    RENDER_COMMAND(BindEnvProbe)(EnvProbeType env_probe_type, ID<EnvProbe> id)
        : env_probe_type(env_probe_type),
          id(id)
    {
    }

    virtual ~RENDER_COMMAND(BindEnvProbe)() override = default;

    virtual Result operator()() override
    {
        g_engine->GetRenderState().BindEnvProbe(env_probe_type, id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindEnvProbe) : renderer::RenderCommand
{
    EnvProbeType env_probe_type;
    ID<EnvProbe> id;

    RENDER_COMMAND(UnbindEnvProbe)(EnvProbeType env_probe_type, ID<EnvProbe> id)
        : env_probe_type(env_probe_type),
          id(id)
    {
    }

    virtual ~RENDER_COMMAND(UnbindEnvProbe)() override = default;

    virtual Result operator()() override
    {
        g_engine->GetRenderState().UnbindEnvProbe(env_probe_type, id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyCubemapRenderPass) : renderer::RenderCommand
{
    EnvProbe &env_probe;

    RENDER_COMMAND(DestroyCubemapRenderPass)(EnvProbe &env_probe)
        : env_probe(env_probe)
    {
    }

    virtual ~RENDER_COMMAND(DestroyCubemapRenderPass)() override = default;

    virtual Result operator()() override
    {
        Result result;

        // empty

        return result;
    }
};

#pragma endregion Render commands

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox &aabb, const Vec3f &origin)
{
    FixedArray<Matrix4, 6> view_matrices;

    for (uint i = 0; i < 6; i++) {
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
    Extent3D grid_size
)
{
    const BoundingBox &aabb = GetDrawProxy().aabb;
    const Vec3f world_position = GetDrawProxy().world_position;

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
        .flags              = GetDrawProxy().flags,
        .camera_near        = GetDrawProxy().camera_near,
        .camera_far         = GetDrawProxy().camera_far,
        .position_in_grid   = grid_slot != ~0u
            ? Vec4i {
                  int32(grid_slot % grid_size.width),
                  int32((grid_slot % (grid_size.width * grid_size.height)) / grid_size.width),
                  int32(grid_slot / (grid_size.width * grid_size.height)),
                  int32(grid_slot)
              }
            : Vec4i::Zero(),
        .position_offset    = Vec4i::Zero()
    };

    g_engine->GetRenderData()->env_probes.Set(GetID().ToIndex(), data);
}

EnvProbe::EnvProbe(
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb,
    const Extent2D &dimensions,
    EnvProbeType env_probe_type
) : EnvProbe(
    parent_scene,
    aabb,
    dimensions,
    env_probe_type,
    Handle<Shader>::empty
)
{
}

EnvProbe::EnvProbe(
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb,
    const Extent2D &dimensions,
    EnvProbeType env_probe_type,
    Handle<Shader> custom_shader
) : BasicObject(),
    m_parent_scene(parent_scene),
    m_aabb(aabb),
    m_dimensions(dimensions),
    m_env_probe_type(env_probe_type),
    m_shader(std::move(custom_shader)),
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
    m_render_list.Reset();
    m_camera.Reset();

    if (m_framebuffer) {
        PUSH_RENDER_COMMAND(DestroyCubemapRenderPass, *this);
    }
    
    m_texture.Reset();
    m_shader.Reset();

    HYP_SYNC_RENDER();
}

void EnvProbe::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    m_draw_proxy = EnvProbeDrawProxy {
        .id = GetID(),
        .aabb = m_aabb,
        .world_position = GetOrigin(),
        .camera_near = m_camera_near,
        .camera_far = m_camera_far,
        .flags = (IsReflectionProbe() ? ENV_PROBE_FLAGS_PARALLAX_CORRECTED : ENV_PROBE_FLAGS_NONE)
            | (IsShadowProbe() ? ENV_PROBE_FLAGS_SHADOW : ENV_PROBE_FLAGS_NONE)
            | (NeedsRender() ? ENV_PROBE_FLAGS_DIRTY : ENV_PROBE_FLAGS_NONE),
        .grid_slot = m_grid_slot
    };

    m_view_matrices = CreateCubemapMatrices(m_aabb, GetOrigin());

    if (!IsControlledByEnvGrid()) {
        if (IsReflectionProbe() || IsSkyProbe()) {
            m_texture = CreateObject<Texture>(TextureCube(
                m_dimensions,
                reflection_probe_format,
                FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                nullptr
            ));
        } else if (IsShadowProbe()) {
            m_texture = CreateObject<Texture>(TextureCube(
                m_dimensions,
                shadow_probe_format,
                FilterMode::TEXTURE_FILTER_NEAREST,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                nullptr
            ));
        }

        AssertThrow(InitObject(m_texture));

        CreateShader();
        CreateFramebuffer();

        AssertThrow(m_parent_scene.IsValid());

        {
            m_camera = CreateObject<Camera>(
                90.0f,
                -int(m_dimensions.width), int(m_dimensions.height),
                m_camera_near, m_camera_far
            );

            m_camera->SetViewMatrix(Matrix4::LookAt(Vector3(0.0f, 0.0f, 1.0f), m_aabb.GetCenter(), Vector3(0.0f, 1.0f, 0.0f)));
            m_camera->SetFramebuffer(m_framebuffer);

            InitObject(m_camera);

            m_render_list.SetCamera(m_camera);
        }
    }

    //SetNeedsUpdate(false);

    SetReady(true);
}

void EnvProbe::CreateShader()
{
    // If a custom shader is provided, use that instead.
    if (m_shader.IsValid()) {
        InitObject(m_shader);

        return;
    }

    switch (m_env_probe_type) {
    case EnvProbeType::ENV_PROBE_TYPE_REFLECTION:
        m_shader = g_shader_manager->GetOrCreate({
            HYP_NAME(RenderToCubemap),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_REFLECTION" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_SKY:
        m_shader = g_shader_manager->GetOrCreate({
            HYP_NAME(RenderSky),
            ShaderProperties(static_mesh_vertex_attributes)
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_SHADOW:
        m_shader = g_shader_manager->GetOrCreate({
            HYP_NAME(RenderToCubemap),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_SHADOWS" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_AMBIENT:
        // Do nothing
        return;
    default:
        AssertThrow(false);
    }

    InitObject(m_shader);
}

void EnvProbe::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        m_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    InternalFormat format = reflection_probe_format;

    if (IsShadowProbe()) {
        format = shadow_probe_format;
    }

    m_framebuffer->AddAttachment(
        0,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            m_dimensions,
            format,
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    m_framebuffer->AddAttachment(
        1,
        MakeRenderObject<Image>(renderer::FramebufferImageCube(
            m_dimensions,
            g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        )),
        RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    InitObject(m_framebuffer);
}

void EnvProbe::EnqueueBind() const
{
    Threads::AssertOnThread(~ThreadName::THREAD_RENDER);

    AssertReady();

    if (!IsControlledByEnvGrid()) {
        PUSH_RENDER_COMMAND(BindEnvProbe, GetEnvProbeType(), GetID());
    }
}

void EnvProbe::EnqueueUnbind() const
{
    Threads::AssertOnThread(~ThreadName::THREAD_RENDER);

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
                m_render_list,
                m_camera,
                RenderableAttributeSet(
                    MeshAttributes { },
                    MaterialAttributes {
                        .shader_definition  = m_shader->GetCompiledShader().GetDefinition(),
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
                m_render_list,
                m_camera,
                RenderableAttributeSet(
                    MeshAttributes { },
                    MaterialAttributes {
                        .shader_definition  = m_shader->GetCompiledShader().GetDefinition(),
                        .blend_function     = BlendFunction::AlphaBlending(),
                        .cull_faces         = FaceCullMode::NONE
                    }
                ),
                true // skip frustum culling (for now, until Camera can have multiple frustums for cubemaps)
            );
        }

        m_render_list.UpdateRenderGroups();
    }

    PUSH_RENDER_COMMAND(UpdateEnvProbeDrawProxy, *this, EnvProbeDrawProxy {
        .id             = GetID(),
        .aabb           = m_aabb,
        .world_position = GetOrigin(),
        .camera_near    = m_camera_near,
        .camera_far     = m_camera_far,
        .flags          = (IsReflectionProbe() ? ENV_PROBE_FLAGS_PARALLAX_CORRECTED : ENV_PROBE_FLAGS_NONE)
                        | (IsShadowProbe() ? ENV_PROBE_FLAGS_SHADOW : ENV_PROBE_FLAGS_NONE)
                        | ENV_PROBE_FLAGS_DIRTY,
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
        DebugLog(
            LogType::Warn,
            "EnvProbe #%u is controlled by an EnvGrid, but Render() is being called!\n",
            GetID().Value()
        );

        return;
    }

    if (!NeedsRender()) {
        return;
    }

    AssertThrow(m_texture.IsValid());

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    auto result = renderer::Result { };

    EnvProbeIndex probe_index;

    const auto &env_probes = g_engine->GetRenderState().bound_env_probes[GetEnvProbeType()];

    {
        const auto it = env_probes.Find(GetID());

        if (it != env_probes.End()) {
            if (!it->second.HasValue()) {
                DebugLog(
                    LogType::Warn,
                    "Env Probe #%u (type: %u) has no value set for texture slot!\n",
                    GetID().Value(),
                    GetEnvProbeType()
                );

                return;
            }

            // don't care about position in grid if it is not controlled by one.
            // set it such that the uint value of probe_index
            // would be the held value.
            probe_index = EnvProbeIndex(
                Extent3D { 0, 0, it->second.Get() },
                Extent3D { 0, 0, 0 }
            );
        }

        if (probe_index == ~0u) {
            DebugLog(
                LogType::Warn,
                "Env Probe #%u (type: %u) not found in render state bound env probe IDs\n",
                GetID().Value(),
                GetEnvProbeType()
            );

            return;
        }
    }

    BindToIndex(probe_index);

    {
        g_engine->GetRenderState().SetActiveEnvProbe(GetID());
        g_engine->GetRenderState().BindScene(m_parent_scene.Get());
        g_engine->GetRenderState().BindCamera(m_camera.Get());

        m_render_list.CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT)),
            nullptr
        );

        m_render_list.ExecuteDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT))
        );

        g_engine->GetRenderState().UnbindCamera();
        g_engine->GetRenderState().UnbindScene();
        g_engine->GetRenderState().UnsetActiveEnvProbe();
    }

    const ImageRef &framebuffer_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

    m_texture->GetImage()->Blit(command_buffer, framebuffer_image);

    if (IsReflectionProbe() || IsSkyProbe()) {
        HYPERION_PASS_ERRORS(
            m_texture->GetImage()->GenerateMipmaps(g_engine->GetGPUDevice(), command_buffer),
            result
        );
    }

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

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

    const uint texture_slot = IsControlledByEnvGrid() ? ~0u : m_bound_index.GetProbeIndex();

    {
        const ShadowFlags shadow_flags = IsShadowProbe() ? SHADOW_FLAGS_VSM : SHADOW_FLAGS_NONE;

        if (NeedsRender()) {
            m_draw_proxy.flags |= ENV_PROBE_FLAGS_DIRTY;
        }

        m_draw_proxy.flags |= shadow_flags << 3;
    }

    UpdateRenderData(
        texture_slot,
        IsControlledByEnvGrid() ? m_grid_slot : ~0u,
        IsControlledByEnvGrid() ? m_bound_index.grid_size : Extent3D { }
    );

    // update cubemap texture in array of bound env probes
    if (set_texture) {
        AssertThrow(texture_slot != ~0u);
        AssertThrow(m_texture.IsValid());

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            switch (GetEnvProbeType()) {
            case ENV_PROBE_TYPE_REFLECTION:
            case ENV_PROBE_TYPE_SKY: // fallthrough
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Scene), frame_index)
                    ->SetElement(HYP_NAME(EnvProbeTextures), texture_slot, m_texture->GetImageView());

                break;
            case ENV_PROBE_TYPE_SHADOW:
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Scene), frame_index)
                    ->SetElement(HYP_NAME(PointLightShadowMapTextures), texture_slot, m_texture->GetImageView());

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
            DebugLog(LogType::Warn, "Probe index (%u) out of range of max bound ambient probes\n", probe_index.GetProbeIndex());
        }

        set_texture = false;
    } else if (IsReflectionProbe() || IsSkyProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_reflection_probes) {
            DebugLog(LogType::Warn, "Probe index (%u) out of range of max bound reflection probes\n", probe_index.GetProbeIndex());

            set_texture = false;
        }
    } else if (IsShadowProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_point_shadow_maps) {
            DebugLog(LogType::Warn, "Probe index (%u) out of range of max bound shadow map probes\n", probe_index.GetProbeIndex());

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

                virtual Result operator()() override
                {
                    env_probe.UpdateRenderData(set_texture);

                    return Result { };
                }
            };

            PUSH_RENDER_COMMAND(UpdateEnvProbeRenderData, *this, set_texture);

            HYP_SYNC_RENDER();
        }
    }
}

} // namespace hyperion

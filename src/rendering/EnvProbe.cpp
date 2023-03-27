#include "EnvProbe.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

class EnvProbe;

struct RenderCommand_CreateCubemapImages;
struct RenderCommand_DestroyCubemapRenderPass;

using renderer::Result;
using renderer::DescriptorSet;
using renderer::Descriptor;

static const Extent2D num_tiles = { 4, 4 };
static const InternalFormat reflection_probe_format = InternalFormat::R11G11B10F;
static const InternalFormat shadow_probe_format = InternalFormat::RG32F;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox &aabb);

#pragma region Render commands

Result RENDER_COMMAND(UpdateEnvProbeDrawProxy)::operator()()
{
    // update m_draw_proxy on render thread.
    env_probe.m_draw_proxy = draw_proxy;
    env_probe.m_view_matrices = CreateCubemapMatrices(draw_proxy.aabb);

    HYPERION_RETURN_OK;
}

struct RENDER_COMMAND(BindEnvProbe) : RenderCommand
{
    EnvProbeType env_probe_type;
    ID<EnvProbe> id;

    RENDER_COMMAND(BindEnvProbe)(EnvProbeType env_probe_type, ID<EnvProbe> id)
        : env_probe_type(env_probe_type),
          id(id)
    {
    }

    virtual Result operator()()
    {
        g_engine->GetRenderState().BindEnvProbe(env_probe_type, id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindEnvProbe) : RenderCommand
{
    EnvProbeType env_probe_type;
    ID<EnvProbe> id;

    RENDER_COMMAND(UnbindEnvProbe)(EnvProbeType env_probe_type, ID<EnvProbe> id)
        : env_probe_type(env_probe_type),
          id(id)
    {
    }

    virtual Result operator()()
    {
        g_engine->GetRenderState().UnbindEnvProbe(env_probe_type, id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyCubemapRenderPass) : RenderCommand
{
    EnvProbe &env_probe;

    RENDER_COMMAND(DestroyCubemapRenderPass)(EnvProbe &env_probe)
        : env_probe(env_probe)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        if (env_probe.m_framebuffer != nullptr) {
            for (auto &attachment : env_probe.m_attachments) {
                env_probe.m_framebuffer->RemoveAttachmentUsage(attachment.get());
            }
        }

        for (auto &attachment : env_probe.m_attachments) {
            HYPERION_PASS_ERRORS(attachment->Destroy(g_engine->GetGPUInstance()->GetDevice()), result);
        }

        env_probe.m_attachments.clear();

        return result;
    }
};

struct RENDER_COMMAND(CreateSHData) : RenderCommand
{
    GPUBufferRef sh_tiles_buffer;

    RENDER_COMMAND(CreateSHData)(
        const GPUBufferRef &sh_tiles_buffer
    ) : sh_tiles_buffer(sh_tiles_buffer)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(sh_tiles_buffer->Create(g_engine->GetGPUDevice(), sizeof(SHTile) * num_tiles.Size() * 6));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateComputeSHDescriptorSets) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateComputeSHDescriptorSets)(const FixedArray<DescriptorSetRef, max_frames_in_flight> &descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (auto &descriptor_set : descriptor_sets) {
            HYPERION_BUBBLE_ERRORS(descriptor_set->Create(g_engine->GetGPUDevice(), &g_engine->GetGPUInstance()->GetDescriptorPool()));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox &aabb)
{
    FixedArray<Matrix4, 6> view_matrices;

    const Vector3 origin = aabb.GetCenter();

    for (UInt i = 0; i < 6; i++) {
        view_matrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemap_directions[i].first,
            Texture::cubemap_directions[i].second
        );
    }

    return view_matrices;
}

void EnvProbe::UpdateEnvProbeShaderData(
    ID<EnvProbe> id,
    const EnvProbeDrawProxy &proxy,
    UInt32 texture_slot,
    UInt32 grid_slot,
    Extent3D grid_size
)
{
    const FixedArray<Matrix4, 6> view_matrices = CreateCubemapMatrices(proxy.aabb);

    EnvProbeShaderData data {
        .face_view_matrices = {
            ShaderMat4(view_matrices[0]),
            ShaderMat4(view_matrices[1]),
            ShaderMat4(view_matrices[2]),
            ShaderMat4(view_matrices[3]),
            ShaderMat4(view_matrices[4]),
            ShaderMat4(view_matrices[5])
        },
        .aabb_max = Vector4(proxy.aabb.max, 1.0f),
        .aabb_min = Vector4(proxy.aabb.min, 1.0f),
        .world_position = Vector4(proxy.world_position, 1.0f),
        .texture_index = texture_slot,
        .flags = proxy.flags,
        .camera_near = proxy.camera_near,
        .camera_far = proxy.camera_far,
        .position_in_grid = grid_slot != ~0u
            ? ShaderVec4<Int32> {
                  Int32(grid_slot % grid_size.width),
                  Int32((grid_slot % (grid_size.width * grid_size.height)) / grid_size.width),
                  Int32(grid_slot / (grid_size.width * grid_size.height)),
                  Int32(grid_slot)
              }
            : ShaderVec4<Int32> { 0, 0, 0, 0 },
        .position_offset = { 0, 0, 0, 0 }
    };

    g_engine->GetRenderData()->env_probes.Set(id.ToIndex(), data);
}

EnvProbe::EnvProbe(
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb,
    const Extent2D &dimensions,
    EnvProbeType env_probe_type
) : EngineComponentBase(),
    m_parent_scene(parent_scene),
    m_aabb(aabb),
    m_dimensions(dimensions),
    m_env_probe_type(env_probe_type),
    m_camera_near(0.001f),
    m_camera_far(aabb.GetRadius()),
    m_needs_update(true),
    m_needs_render_counter(0),
    m_is_rendered { false }
{
}

EnvProbe::~EnvProbe()
{
    Teardown();
}

void EnvProbe::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    m_draw_proxy = EnvProbeDrawProxy {
        .id = m_id,
        .aabb = m_aabb,
        .world_position = m_aabb.GetCenter(),
        .camera_near = m_camera_near,
        .camera_far = m_camera_far,
        .flags = (IsReflectionProbe() ? ENV_PROBE_FLAGS_PARALLAX_CORRECTED : ENV_PROBE_FLAGS_NONE)
            | (IsShadowProbe() ? ENV_PROBE_FLAGS_SHADOW : ENV_PROBE_FLAGS_NONE)
            | (NeedsRender() ? ENV_PROBE_FLAGS_DIRTY : ENV_PROBE_FLAGS_NONE),
        .grid_slot = m_grid_slot
    };

    m_view_matrices = CreateCubemapMatrices(m_aabb);

    if (!IsAmbientProbe()) {
        if (IsReflectionProbe()) {
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
                -Int(m_dimensions.width), Int(m_dimensions.height),
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

    OnTeardown([this]() {
        m_render_list.Reset();
        m_camera.Reset();

        if (m_framebuffer) {
            PUSH_RENDER_COMMAND(DestroyCubemapRenderPass, *this);
        }

        SetReady(false);

        m_texture.Reset();
        m_shader.Reset();

        HYP_SYNC_RENDER();
    });
}

void EnvProbe::CreateShader()
{
    switch (m_env_probe_type) {
    case EnvProbeType::ENV_PROBE_TYPE_REFLECTION:
        m_shader = g_shader_manager->GetOrCreate({
            HYP_NAME(CubemapRenderer),
            ShaderProperties(renderer::static_mesh_vertex_attributes, { "MODE_REFLECTION" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_SHADOW:
        m_shader = g_shader_manager->GetOrCreate({
            HYP_NAME(CubemapRenderer),
            ShaderProperties(renderer::static_mesh_vertex_attributes, { "MODE_SHADOWS" })
        });

        break;
    case EnvProbeType::ENV_PROBE_TYPE_AMBIENT:
        // Do nothing
        return;
    }

    InitObject(m_shader);
}

void EnvProbe::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        m_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    InternalFormat format = reflection_probe_format;

    if (IsShadowProbe()) {
        format = shadow_probe_format;
    }

    m_framebuffer->AddAttachment(
        0,
        RenderObjects::Make<Image>(renderer::FramebufferImageCube(
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
        RenderObjects::Make<Image>(renderer::FramebufferImageCube(
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
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    if (!IsAmbientProbe()) {
        PUSH_RENDER_COMMAND(BindEnvProbe, GetEnvProbeType(), m_id);
    }
}

void EnvProbe::EnqueueUnbind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    if (!IsAmbientProbe()) {
        PUSH_RENDER_COMMAND(UnbindEnvProbe, GetEnvProbeType(), m_id);
    }
}

void EnvProbe::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();
    
    // Check if octree has changes, and we need to re-render.

    const bool is_rendered = m_is_rendered.Get(MemoryOrder::ACQUIRE);

    Octree const *octree = nullptr;

    HashCode octant_hash;
    
    if (g_engine->GetWorld()->GetOctree().GetFittingOctant(m_aabb, octree)) {
        AssertThrow(octree != nullptr);

        octant_hash = octree->GetNodesHash();
    }

    if (m_octant_hash_code != octant_hash) {
        SetNeedsUpdate(true);

        DebugLog(LogType::Debug, "Probe #%u octree hash changed (%llu != %llu)\n", GetID().Value(), octant_hash.Value(), m_octant_hash_code.Value());

        m_octant_hash_code = octant_hash;
    }
    
    if (!NeedsUpdate()) {
        return;
    }

    // Ambient probes do not use their own render list,
    // instead they are attached to a grid which shares one
    if (!IsAmbientProbe()) {
        AssertThrow(m_camera.IsValid());
        AssertThrow(m_shader.IsValid());

        m_camera->Update(delta);

        m_parent_scene->CollectEntities(
            m_render_list,
            m_camera,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT) | (1 << BUCKET_SKYBOX)),
            RenderableAttributeSet(
                MeshAttributes { },
                MaterialAttributes {
                    .bucket = BUCKET_INTERNAL,
                    .cull_faces = FaceCullMode::FRONT
                },
                m_shader->GetCompiledShader().GetDefinition()
            ),
            true // skip frustum culling
        );

        m_render_list.UpdateRenderGroups();
    }

    PUSH_RENDER_COMMAND(UpdateEnvProbeDrawProxy, *this, EnvProbeDrawProxy {
        .id = m_id,
        .aabb = m_aabb,
        .world_position = m_aabb.GetCenter(),
        .camera_near = m_camera_near,
        .camera_far = m_camera_far,
        .flags = (IsReflectionProbe() ? ENV_PROBE_FLAGS_PARALLAX_CORRECTED : ENV_PROBE_FLAGS_NONE)
            | (IsShadowProbe() ? ENV_PROBE_FLAGS_SHADOW : ENV_PROBE_FLAGS_NONE)
            | ENV_PROBE_FLAGS_DIRTY,
        .grid_slot = m_grid_slot
    });

    SetNeedsUpdate(false);
    SetNeedsRender(true);
}

void EnvProbe::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (IsAmbientProbe()) {
        return;
    }

    if (!NeedsRender()) {
        return;
    }

    AssertThrow(m_texture.IsValid());

    CommandBuffer *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

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

            // don't care about position in grid,
            // set it such that the UInt value of probe_index
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

    UpdateRenderData(probe_index);

    {
        g_engine->GetRenderState().SetActiveEnvProbe(GetID());
        g_engine->GetRenderState().BindScene(m_parent_scene.Get());
        g_engine->GetRenderState().BindCamera(m_camera.Get());

        m_render_list.CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT) | (1 << BUCKET_SKYBOX)),
            nullptr
        );

        m_render_list.ExecuteDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_TRANSLUCENT) | (1 << BUCKET_SKYBOX))
        );

        g_engine->GetRenderState().UnbindCamera();
        g_engine->GetRenderState().UnbindScene();
        g_engine->GetRenderState().UnsetActiveEnvProbe();
    }

    Image *framebuffer_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

    m_texture->GetImage()->Blit(command_buffer, framebuffer_image);

    if (GetEnvProbeType() == ENV_PROBE_TYPE_REFLECTION) {
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
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(m_bound_index.GetProbeIndex() != ~0u);

    if (IsAmbientProbe()) {
        AssertThrow(m_grid_slot != ~0u);
    }

    const UInt texture_slot = IsAmbientProbe() ? ~0u : m_bound_index.GetProbeIndex();

    {
        const ShadowFlags shadow_flags = IsShadowProbe() ? SHADOW_FLAGS_VSM : SHADOW_FLAGS_NONE;

        if (NeedsRender()) {
            m_draw_proxy.flags |= ENV_PROBE_FLAGS_DIRTY;
        } 

        m_draw_proxy.flags |= shadow_flags << 3;
    }

    UpdateEnvProbeShaderData(
        GetID(),
        m_draw_proxy,
        texture_slot,
        IsAmbientProbe() ? m_grid_slot : ~0u,
        IsAmbientProbe() ? m_bound_index.grid_size : Extent3D { }
    );

    // update cubemap texture in array of bound env probes
    if (set_texture) {
        AssertThrow(texture_slot != ~0u);
        AssertThrow(m_texture.IsValid());

        const auto &descriptor_pool = g_engine->GetGPUInstance()->GetDescriptorPool();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            DescriptorKey descriptor_key;

            switch (GetEnvProbeType()) {
            case ENV_PROBE_TYPE_REFLECTION:
                descriptor_key = DescriptorKey::ENV_PROBE_TEXTURES;

                break;
            case ENV_PROBE_TYPE_SHADOW:
                descriptor_key = DescriptorKey::POINT_SHADOW_MAPS;

                break;
            default:
                descriptor_key = DescriptorKey::UNUSED;
                break;
            }

            AssertThrow(descriptor_key != DescriptorKey::UNUSED);

            DescriptorSet *descriptor_set = descriptor_pool.GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
            Descriptor *descriptor = descriptor_set->GetOrAddDescriptor<renderer::ImageDescriptor>(descriptor_key);

            descriptor->SetElementSRV(
                texture_slot,
                m_texture->GetImageView()
            );
        }
    }
}

void EnvProbe::UpdateRenderData(const EnvProbeIndex &probe_index)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    bool set_texture = true;

    if (IsAmbientProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_ambient_probes) {
            DebugLog(LogType::Warn, "Probe index (%u) out of range of max bound ambient probes\n", probe_index.GetProbeIndex());
        }

        set_texture = false;
    } else if (IsReflectionProbe()) {
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

    UpdateRenderData(set_texture);
}

} // namespace hyperion::v2

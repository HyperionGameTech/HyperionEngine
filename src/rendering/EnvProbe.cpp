#include "EnvProbe.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

class EnvProbe;

struct RenderCommand_CreateCubemapImages;
struct RenderCommand_DestroyCubemapRenderPass;

using renderer::Result;

static const Extent2D num_tiles = { 4, 4 };

struct alignas(16) SHTile
{
    ShaderVec4<Float> coeffs_weights[9];
};

static_assert(sizeof(SHTile) == 144);

#pragma region Render commands

struct RENDER_COMMAND(BindEnvProbe) : RenderCommand
{
    ID<EnvProbe> id;
    bool is_ambient_probe;

    RENDER_COMMAND(BindEnvProbe)(ID<EnvProbe> id, bool is_ambient_probe)
        : id(id),
          is_ambient_probe(is_ambient_probe)
    {
    }

    virtual Result operator()()
    {
        if (is_ambient_probe) {
            Engine::Get()->GetRenderState().BindAmbientProbe(id);
        } else {
            Engine::Get()->GetRenderState().BindReflectionProbe(id);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnbindEnvProbe) : RenderCommand
{
    ID<EnvProbe> id;

    RENDER_COMMAND(UnbindEnvProbe)(ID<EnvProbe> id)
        : id(id)
    {
    }

    virtual Result operator()()
    {
        Engine::Get()->GetRenderState().UnbindEnvProbe(id);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UpdateEnvProbeDrawProxy) : RenderCommand
{
    EnvProbe &env_probe;
    EnvProbeDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateEnvProbeDrawProxy)(EnvProbe &env_probe, EnvProbeDrawProxy &&draw_proxy)
        : env_probe(env_probe),
          draw_proxy(std::move(draw_proxy))
    {
    }

    virtual Result operator()()
    {
        // update m_draw_proxy on render thread.
        env_probe.m_draw_proxy = draw_proxy;

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateCubemapBuffers) : RenderCommand
{
    EnvProbe &env_probe;

    RENDER_COMMAND(CreateCubemapBuffers)(EnvProbe &env_probe)
        : env_probe(env_probe)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_PASS_ERRORS(
                env_probe.m_cubemap_render_uniform_buffers[frame_index]->Create(Engine::Get()->GetGPUDevice(), sizeof(CubemapUniforms)),
                result
            );

            env_probe.m_cubemap_render_uniform_buffers[frame_index]->Copy(Engine::Get()->GetGPUDevice(), sizeof(CubemapUniforms), &env_probe.m_cubemap_uniforms);
        }

        return result;
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
            HYPERION_PASS_ERRORS(attachment->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()), result);
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
        HYPERION_BUBBLE_ERRORS(sh_tiles_buffer->Create(Engine::Get()->GetGPUDevice(), sizeof(SHTile) * num_tiles.Size() * 6));

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
            HYPERION_BUBBLE_ERRORS(descriptor_set->Create(Engine::Get()->GetGPUDevice(), &Engine::Get()->GetGPUInstance()->GetDescriptorPool()));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

const FixedArray<std::pair<Vector3, Vector3>, 6> EnvProbe::cubemap_directions = {
    std::make_pair(Vector3(1, 0, 0), Vector3(0, 1, 0)),
    std::make_pair(Vector3(-1, 0, 0),  Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 1, 0),  Vector3(0, 0, -1)),
    std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, 1)),
    std::make_pair(Vector3(0, 0, 1), Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 0, -1),  Vector3(0, 1, 0)),
};

EnvProbe::EnvProbe(
    const Handle<Scene> &parent_scene,
    const BoundingBox &aabb,
    const Extent2D &dimensions,
    bool is_ambient_probe
) : EngineComponentBase(),
    m_parent_scene(parent_scene),
    m_aabb(aabb),
    m_dimensions(dimensions),
    m_is_ambient_probe(is_ambient_probe),
    m_needs_update(true)
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
        .flags = m_is_ambient_probe
            ? EnvProbeFlags::ENV_PROBE_NONE
            : EnvProbeFlags::ENV_PROBE_PARALLAX_CORRECTED
    };

    m_needs_update = false;

    const Vector3 origin = m_aabb.GetCenter();

    {
        for (UInt i = 0; i < 6; i++) {
            m_view_matrices[i] = Matrix4::LookAt(
                origin,
                origin + cubemap_directions[i].first,
                cubemap_directions[i].second
            );
        }
    }

    if (IsAmbientProbe()) {
        CreateSHData();
    } else {
        m_texture = CreateObject<Texture>(TextureCube(
            m_dimensions,
            InternalFormat::RGBA8_SRGB,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        InitObject(m_texture);

        CreateShader();
        CreateFramebuffer();
        CreateImagesAndBuffers();

        AssertThrow(m_parent_scene.IsValid());

        m_scene = CreateObject<Scene>(CreateObject<Camera>(
            90.0f,
            -Int(m_dimensions.width), Int(m_dimensions.height),
            0.001f, m_aabb.GetExtent().Max() * 0.5f
        ));

        m_scene->GetCamera()->SetViewMatrix(Matrix4::LookAt(Vector3(0.0f, 0.0f, 1.0f), origin, Vector3(0.0f, 1.0f, 0.0f)));
        m_scene->GetCamera()->SetFramebuffer(m_framebuffer);

        m_scene->SetName("Cubemap renderer scene");
        m_scene->SetOverrideRenderableAttributes(RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::BACK
            },
            m_shader->GetID()
        ));

        // set ID so scene can index into env probes
        m_scene->SetCustomID(GetID());
        m_scene->SetParentScene(m_parent_scene);

        InitObject(m_scene);

        Engine::Get()->GetWorld()->AddScene(m_scene);
    }

    SetReady(true);

    OnTeardown([this]() {
        if (m_scene) {
            Engine::Get()->GetWorld()->RemoveScene(m_scene->GetID());
            m_scene.Reset();
        }

        if (m_framebuffer) {
            PUSH_RENDER_COMMAND(DestroyCubemapRenderPass, *this);
        }

        SetReady(false);

        m_texture.Reset();
        m_shader.Reset();

        SafeRelease(std::move(m_cubemap_render_uniform_buffers));

        HYP_SYNC_RENDER();
    });
}

void EnvProbe::CreateImagesAndBuffers()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_cubemap_render_uniform_buffers[frame_index] = RenderObjects::Make<GPUBuffer>(UniformBuffer());
    }
    
    PUSH_RENDER_COMMAND(CreateCubemapBuffers, *this);
}

void EnvProbe::CreateShader()
{
    m_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader(
        "CubemapRenderer",
        ShaderProps({ "LIGHTING", "SHADOWS", "TONEMAP" })
    ));

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

    AttachmentUsage *attachment_usage;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentUsage(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    m_framebuffer->AddAttachmentUsage(attachment_usage);

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_dimensions,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentUsage(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    m_framebuffer->AddAttachmentUsage(attachment_usage);

    // attachment should be created in render thread?
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
    }

    InitObject(m_framebuffer);
}

void EnvProbe::CreateSHData()
{
    m_sh_tiles_buffer = RenderObjects::Make<GPUBuffer>(StorageBuffer());
    
    PUSH_RENDER_COMMAND(
        CreateSHData,
        m_sh_tiles_buffer
    );

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_compute_sh_descriptor_sets[frame_index] = RenderObjects::Make<DescriptorSet>();

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(0)
            ->SetElementSRV(0, &Engine::Get()->GetPlaceholderData().GetImageViewCube1x1R8());

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(1)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_sh_tiles_buffer);

        m_compute_sh_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageImageDescriptor>(3)
            ->SetElementUAV(0, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[0].image_view)
            ->SetElementUAV(1, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[1].image_view)
            ->SetElementUAV(2, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[2].image_view)
            ->SetElementUAV(3, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[3].image_view)
            ->SetElementUAV(4, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[4].image_view)
            ->SetElementUAV(5, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[5].image_view)
            ->SetElementUAV(6, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[6].image_view)
            ->SetElementUAV(7, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[7].image_view)
            ->SetElementUAV(8, Engine::Get()->shader_globals->spherical_harmonics_grid.textures[8].image_view);
    }

    PUSH_RENDER_COMMAND(CreateComputeSHDescriptorSets, m_compute_sh_descriptor_sets);
    
    m_clear_sh = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ComputeSH", {{ "MODE_CLEAR" }})),
        Array<const DescriptorSet *> { m_compute_sh_descriptor_sets[0].Get() }
    );

    InitObject(m_clear_sh);

    m_compute_sh = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ComputeSH", {{ "MODE_BUILD_COEFFICIENTS" }})),
        Array<const DescriptorSet *> { m_compute_sh_descriptor_sets[0].Get() }
    );

    InitObject(m_compute_sh);

    m_finalize_sh = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ComputeSH", {{ "MODE_FINALIZE" }})),
        Array<const DescriptorSet *> { m_compute_sh_descriptor_sets[0].Get() }
    );

    InitObject(m_finalize_sh);
}

void EnvProbe::EnqueueBind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    PUSH_RENDER_COMMAND(BindEnvProbe, m_id, IsAmbientProbe());
}

void EnvProbe::EnqueueUnbind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    PUSH_RENDER_COMMAND(UnbindEnvProbe, m_id);
}

void EnvProbe::Update()
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    if (!m_needs_update) {
        return;
    }

    PUSH_RENDER_COMMAND(UpdateEnvProbeDrawProxy, *this, EnvProbeDrawProxy {
        .id = m_id,
        .aabb = m_aabb,
        .world_position = m_aabb.GetCenter(),
        .flags = m_is_ambient_probe
            ? EnvProbeFlags::ENV_PROBE_NONE
            : EnvProbeFlags::ENV_PROBE_PARALLAX_CORRECTED
    });

    m_needs_update = false;
}

void EnvProbe::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (IsAmbientProbe()) {
        return;
    }

    AssertThrow(m_texture.IsValid());

    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

    EnvProbeIndex probe_index;

    {
        const auto it = Engine::Get()->GetRenderState().bound_env_probes.Find(GetID());

        if (it != Engine::Get()->GetRenderState().bound_env_probes.End() && it->second.HasValue()) {
            // don't care about position in grid,
            // set it such that the UInt value of probe_index
            // would be the held value.
            probe_index = EnvProbeIndex(
                Extent3D { 0, 0, it->second.Get() },
                Extent3D { 0, 0, 0 }
            );
        }
    }

    if (probe_index == ~0u) {
        DebugLog(LogType::Warn, "Env Probe #%u not found in render state bound env probe IDs\n", GetID().Value());

        return;
    }

    UpdateRenderData(probe_index);

    {
        Engine::Get()->GetRenderState().SetActiveEnvProbe(GetID());

        m_scene->Render(frame);

        Engine::Get()->GetRenderState().UnsetActiveEnvProbe();
    }

    Image *framebuffer_image = m_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);
    m_texture->GetImage()->Blit(command_buffer, framebuffer_image);

    HYPERION_PASS_ERRORS(
        m_texture->GetImage()->GenerateMipmaps(Engine::Get()->GetGPUDevice(), command_buffer),
        result
    );

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    m_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    HYPERION_ASSERT_RESULT(result);
}

void EnvProbe::ComputeSH(Frame *frame, const Image *image, const ImageView *image_view)
{
    AssertThrow(IsAmbientProbe());

    EnvProbeIndex bound_index = m_bound_index;

    AssertThrowMsg(bound_index != ~0u, "Probe not bound to an index!");

    // ambient probes have +1 for their bound index, so we subtract that to get the actual index
    bound_index.position[2] -= 1;

    const UInt probe_index = bound_index.GetProbeIndex();
    const Extent3D &grid_image_extent = Engine::Get()->shader_globals->spherical_harmonics_grid.textures[0].image->GetExtent();

    AssertThrowMsg(probe_index < grid_image_extent.Size(), "Out of bounds!");

    Extent3D position_in_grid {
        probe_index % grid_image_extent.depth,
        (probe_index / grid_image_extent.depth) % grid_image_extent.height,
        probe_index / (grid_image_extent.height * grid_image_extent.depth)
    };

    AssertThrow(image != nullptr);
    AssertThrow(image_view != nullptr);

    struct alignas(128) {
        ShaderVec4<UInt32> probe_grid_position;
        ShaderVec2<UInt32> cubemap_dimensions;
    } push_constants;
    
    push_constants.probe_grid_position = { bound_index.position[0], bound_index.position[1], bound_index.position[2], 0 };//{ position_in_grid[0], position_in_grid[1], position_in_grid[2], 0 };
    push_constants.cubemap_dimensions = { image->GetExtent().width, image->GetExtent().height };
    
    m_compute_sh_descriptor_sets[frame->GetFrameIndex()]
        ->GetDescriptor(0)
        ->SetElementSRV(0, image_view);

    m_compute_sh_descriptor_sets[frame->GetFrameIndex()]
        ->ApplyUpdates(Engine::Get()->GetGPUDevice());

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_clear_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * GetID().ToIndex())}
    );

    m_clear_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_clear_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_compute_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * GetID().ToIndex())}
    );

    m_compute_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_compute_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });

    m_sh_tiles_buffer->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);

    for (auto &texture : Engine::Get()->shader_globals->spherical_harmonics_grid.textures) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::UNORDERED_ACCESS);
    }

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_finalize_sh->GetPipeline(),
        m_compute_sh_descriptor_sets[frame->GetFrameIndex()],
        0,
        FixedArray { UInt32(sizeof(SH9Buffer) * GetID().ToIndex())}
    );

    m_finalize_sh->GetPipeline()->Bind(frame->GetCommandBuffer(), &push_constants, sizeof(push_constants));
    m_finalize_sh->GetPipeline()->Dispatch(frame->GetCommandBuffer(), Extent3D { 1, 1, 1 });
    
    for (auto &texture : Engine::Get()->shader_globals->spherical_harmonics_grid.textures) {
        texture.image->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }
}

void EnvProbe::UpdateRenderData(const EnvProbeIndex &probe_index)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (IsAmbientProbe()) {
        if (probe_index.GetProbeIndex() >= max_bound_ambient_probes) {
            DebugLog(LogType::Warn, "Probe index (%u) out of range of max bound ambient probes\n", probe_index.GetProbeIndex());

            return;
        }
    } else {
        if (probe_index.GetProbeIndex() >= max_bound_reflection_probes) {
            DebugLog(LogType::Warn, "Probe index (%u) out of range of max bound reflection probes\n", probe_index.GetProbeIndex());

            return;
        }
    }

    m_bound_index = probe_index;
    
    const UInt texture_slot = IsAmbientProbe() ? ~0u : m_bound_index.GetProbeIndex();

    EnvProbeShaderData data {
        .face_view_matrices = {
            ShaderMat4(GetViewMatrices()[0]),
            ShaderMat4(GetViewMatrices()[1]),
            ShaderMat4(GetViewMatrices()[2]),
            ShaderMat4(GetViewMatrices()[3]),
            ShaderMat4(GetViewMatrices()[4]),
            ShaderMat4(GetViewMatrices()[5])
        },
        .aabb_max = Vector4(m_draw_proxy.aabb.max, 1.0f),
        .aabb_min = Vector4(m_draw_proxy.aabb.min, 1.0f),
        .world_position = Vector4(m_draw_proxy.world_position, 1.0f),
        .texture_index = texture_slot,
        .flags = UInt32(m_draw_proxy.flags)
    };

    Engine::Get()->GetRenderData()->env_probes.Set(GetID().ToIndex(), data);

    // ambient probes have no need to update texture at the binding slot.
    if (IsAmbientProbe()) {
        return;
    }

    // update cubemap texture in array of bound env probes
    if (texture_slot != ~0u) {
        AssertThrow(m_texture.IsValid());

        const auto &descriptor_pool = Engine::Get()->GetGPUInstance()->GetDescriptorPool();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = descriptor_pool.GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::ENV_PROBE_TEXTURES);

            descriptor->SetElementSRV(
                texture_slot,
                m_texture->GetImageView()
            );
        }
    }
}

} // namespace hyperion::v2

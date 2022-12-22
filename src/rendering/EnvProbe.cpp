#include "EnvProbe.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

class EnvProbe;

struct RenderCommand_CreateCubemapImages;
struct RenderCommand_DestroyCubemapRenderPass;

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(BindEnvProbe) : RenderCommand
{
    ID<EnvProbe> id;

    RENDER_COMMAND(BindEnvProbe)(ID<EnvProbe> id)
        : id(id)
    {
    }

    virtual Result operator()()
    {
        Engine::Get()->GetRenderState().BindEnvProbe(id);

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

#pragma endregion

const FixedArray<std::pair<Vector3, Vector3>, 6> EnvProbe::cubemap_directions = {
    std::make_pair(Vector3(1, 0, 0), Vector3(0, 1, 0)),
    std::make_pair(Vector3(-1, 0, 0),  Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 1, 0),  Vector3(0, 0, -1)),
    std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, 1)),
    std::make_pair(Vector3(0, 0, 1), Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 0, -1),  Vector3(0, 1, 0)),
};

EnvProbe::EnvProbe(const Handle<Scene> &parent_scene, const BoundingBox &aabb, const Extent2D &dimensions)
    : EngineComponentBase(),
      m_parent_scene(parent_scene),
      m_aabb(aabb),
      m_dimensions(dimensions),
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

    m_texture = CreateObject<Texture>(TextureCube(
        m_dimensions,
        InternalFormat::RGBA8_SRGB,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    InitObject(m_texture);

    // CreateShader();
    // CreateFramebuffer();
    // CreateImagesAndBuffers();

    // m_scene = CreateObject<Scene>(CreateObject<Camera>(
    //     90.0f,
    //     cubemap_dimensions.width, cubemap_dimensions.height,
    //     0.01f, 1000.0f
    // ));

    // m_scene->GetCamera()->SetViewMatrix(Matrix4::LookAt(Vector3(0.0f, 0.0f, 1.0f), origin, Vector3(0.0f, 1.0f, 0.0f)));
    // m_scene->GetCamera()->SetFramebuffer(m_framebuffer);

    // m_scene->SetName("Cubemap renderer scene");
    // m_scene->SetOverrideRenderableAttributes(RenderableAttributeSet(
    //     MeshAttributes { },
    //     MaterialAttributes {
    //         .bucket = BUCKET_INTERNAL,
    //         .cull_faces = FaceCullMode::BACK
    //     },
    //     m_shader->GetID()
    // ));

    // // set ID so scene can index into env probes
    // m_scene->SetCustomID(GetID());
    // m_scene->SetParentScene(std::move(m_parent_scene));

    // InitObject(m_scene);
    // Engine::Get()->GetWorld()->AddScene(m_scene);


    SetReady(true);
    Update();

    OnTeardown([this]() {
        // Engine::Get()->GetWorld()->RemoveScene(m_scene->GetID());
        // m_scene.Reset();

        // PUSH_RENDER_COMMAND(DestroyCubemapRenderPass, *this);

        SetReady(false);

        Engine::Get()->SafeReleaseHandle<Texture>(std::move(m_texture));
        Engine::Get()->SafeReleaseHandle<Shader>(std::move(m_shader));

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

    DebugLog(
        LogType::Debug,
        "Added cubemap uniform buffers\n"
    );
}

void EnvProbe::CreateShader()
{
    m_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("CubemapRenderer"));
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

void EnvProbe::EnqueueBind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    RenderCommands::Push<RENDER_COMMAND(BindEnvProbe)>(m_id);
}

void EnvProbe::EnqueueUnbind() const
{
    Threads::AssertOnThread(~THREAD_RENDER);
    AssertReady();

    RenderCommands::Push<RENDER_COMMAND(UnbindEnvProbe)>(m_id);
}

void EnvProbe::Update()
{
    AssertReady();

    // if (!m_needs_update) {
    //     return;
    // }

    RenderCommands::Push<RENDER_COMMAND(UpdateEnvProbeDrawProxy)>(*this, EnvProbeDrawProxy {
        .id = m_id,
        .aabb = m_aabb,
        .world_position = m_aabb.GetCenter(),
        .flags = EnvProbeFlags::ENV_PROBE_PARALLAX_CORRECTED
    });

    m_needs_update = false;
}

void EnvProbe::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

    UInt probe_index = ~0u;

    {
        const auto it = Engine::Get()->GetRenderState().bound_env_probes.Find(GetID());

        if (it != Engine::Get()->GetRenderState().bound_env_probes.End() && it->second.HasValue()) {
            probe_index = it->second.Get();
        }
    }

    if (probe_index == ~0u) {
        return;
    }

    UpdateRenderData(probe_index);

    m_scene->Render(frame);

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

void EnvProbe::UpdateRenderData(UInt probe_index)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    // find the texture slot of this env probe (if it exists)
    AssertThrow(probe_index < max_bound_env_probes);

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
        .texture_index = probe_index,
        .flags = UInt32(m_draw_proxy.flags)
    };

    Engine::Get()->GetRenderData()->env_probes.Set(GetID().ToIndex(), data);

    // update cubemap texture in array of bound env probes
    if (probe_index != ~0u) {
        AssertThrow(probe_index < max_bound_env_probes);
        AssertThrow(m_texture.IsValid());

        const auto &descriptor_pool = Engine::Get()->GetGPUInstance()->GetDescriptorPool();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set = descriptor_pool.GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::ENV_PROBE_TEXTURES);

            descriptor->SetElementSRV(
                probe_index,
                m_texture ? m_texture->GetImageView() : &Engine::Get()->GetPlaceholderData().GetImageViewCube1x1R8()
            );
        }
    }
}

} // namespace hyperion::v2

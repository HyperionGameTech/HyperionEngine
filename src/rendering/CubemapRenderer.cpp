#include "CubemapRenderer.hpp"

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <scene/camera/PerspectiveCamera.hpp>


#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

struct RENDER_COMMAND(CreateCubemapImages) : RenderCommandBase2
{
    CubemapRenderer &cubemap_renderer;

    RENDER_COMMAND(CreateCubemapImages)(CubemapRenderer &cubemap_renderer)
        : cubemap_renderer(cubemap_renderer)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_ASSERT_RESULT(cubemap_renderer.m_cubemap_render_uniform_buffers[frame_index]->Create(Engine::Get()->GetDevice(), sizeof(CubemapUniforms)));
            cubemap_renderer.m_cubemap_render_uniform_buffers[frame_index]->Copy(Engine::Get()->GetDevice(), sizeof(CubemapUniforms), &cubemap_renderer.m_cubemap_uniforms);

            auto *descriptor_set = Engine::Get()->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set
                ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::CUBEMAP_UNIFORMS)
                ->SetElementBuffer(cubemap_renderer.GetComponentIndex(), cubemap_renderer.m_cubemap_render_uniform_buffers[frame_index].Get());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyCubemapRenderPass) : RenderCommandBase2
{
    CubemapRenderer &cubemap_renderer;

    RENDER_COMMAND(DestroyCubemapRenderPass)(CubemapRenderer &cubemap_renderer)
        : cubemap_renderer(cubemap_renderer)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            if (cubemap_renderer.m_framebuffers[i] != nullptr) {
                for (auto &attachment : cubemap_renderer.m_attachments) {
                    cubemap_renderer.m_framebuffers[i]->RemoveAttachmentRef(attachment.get());
                }

                if (cubemap_renderer.m_renderer_instance != nullptr) {
                    cubemap_renderer.m_renderer_instance->RemoveFramebuffer(cubemap_renderer.m_framebuffers[i]->GetID());
                }
            }
        }

        if (cubemap_renderer.m_render_pass != nullptr) {
            for (auto &attachment : cubemap_renderer.m_attachments) {
                cubemap_renderer.m_render_pass->GetRenderPass().RemoveAttachmentRef(attachment.get());
            }
        }
        for (auto &attachment : cubemap_renderer.m_attachments) {
            HYPERION_PASS_ERRORS(attachment->Destroy(Engine::Get()->GetInstance()->GetDevice()), result);
        }

        cubemap_renderer.m_attachments.clear();

        return result;
    }
};

struct RENDER_COMMAND(DestroyCubemapUniformBuffers) : RenderCommandBase2
{
    UInt component_index;
    FixedArray<UniquePtr<UniformBuffer>, 2> uniform_buffers;

    RENDER_COMMAND(DestroyCubemapUniformBuffers)(UInt component_index, FixedArray<UniquePtr<UniformBuffer>, 2> &&uniform_buffers)
        : component_index(component_index),
          uniform_buffers(std::move(uniform_buffers))
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_PASS_ERRORS(
                uniform_buffers[i]->Destroy(Engine::Get()->GetDevice()),
                result
            );

            auto *descriptor_set = Engine::Get()->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

            descriptor_set
                ->GetDescriptor(DescriptorKey::CUBEMAP_UNIFORMS)
                ->SetElementBuffer(component_index, Engine::Get()->GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(Engine::Get()->GetDevice(), sizeof(CubemapUniforms)));
        }

        return result;
    }
};

const FixedArray<std::pair<Vector3, Vector3>, 6> CubemapRenderer::cubemap_directions = {
    std::make_pair(Vector3(-1, 0, 0), Vector3(0, 1, 0)),
    std::make_pair(Vector3(1, 0, 0),  Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 1, 0),  Vector3(0, 0, -1)),
    std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, 1)),
    std::make_pair(Vector3(0, 0, 1),  Vector3(0, 1, 0)),
    std::make_pair(Vector3(0, 0, -1), Vector3(0, 1, 0)),
};

CubemapRenderer::CubemapRenderer(
    const Extent2D &cubemap_dimensions,
    const Vector3 &origin,
    FilterMode filter_mode
) : EngineComponentBase(),
    RenderComponent(5),
    m_cubemap_dimensions(cubemap_dimensions),
    m_aabb(BoundingBox(origin - 150.0f, origin + 150.0f)),
    m_filter_mode(filter_mode)
{
}

CubemapRenderer::CubemapRenderer(
    const Extent2D &cubemap_dimensions,
    const BoundingBox &aabb,
    FilterMode filter_mode
) : EngineComponentBase(),
    RenderComponent(5),
    m_cubemap_dimensions(cubemap_dimensions),
    m_aabb(aabb),
    m_filter_mode(filter_mode)
{
}


CubemapRenderer::~CubemapRenderer()
{
    Teardown();
}

void CubemapRenderer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    CreateShader();
    CreateRenderPass();
    CreateFramebuffers();
    CreateImagesAndBuffers();
    CreateRendererInstance();

    m_scene = Engine::Get()->CreateHandle<Scene>(Handle<Camera>());
    Engine::Get()->InitObject(m_scene);

    // testing global skybox
    auto tex = Engine::Get()->CreateHandle<Texture>(new TextureCube(
        Engine::Get()->GetAssetManager().LoadMany<Texture>(
            "textures/chapel/posx.jpg",
            "textures/chapel/negx.jpg",
            "textures/chapel/posy.jpg",
            "textures/chapel/negy.jpg",
            "textures/chapel/posz.jpg",
            "textures/chapel/negz.jpg"
        )
    ));
    tex->GetImage().SetIsSRGB(true);

    m_env_probe = Engine::Get()->CreateHandle<EnvProbe>(

        // TEMP
        std::move(tex),
        
        // Handle<Texture>(m_cubemaps[0]), // TODO
        m_aabb
    );
    Engine::Get()->InitObject(m_env_probe);

    HYP_FLUSH_RENDER_QUEUE();

    SetReady(true);

    OnTeardown([this]() {
        RenderCommands::Push<RENDER_COMMAND(DestroyCubemapRenderPass)>(*this);

        for (auto &cubemap : m_cubemaps) {
            Engine::Get()->SafeReleaseHandle<Texture>(std::move(cubemap));
        }

        Engine::Get()->SafeReleaseHandle<Shader>(std::move(m_shader));

        m_env_probe.Reset();
        m_scene.Reset();

        HYP_FLUSH_RENDER_QUEUE();

        SetReady(false);

        RenderCommands::Push<RENDER_COMMAND(DestroyCubemapUniformBuffers)>(
            GetComponentIndex(),
            std::move(m_cubemap_render_uniform_buffers)
        );
    });
}

// called from game thread
void CubemapRenderer::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);

    AssertThrow(m_env_probe.IsValid());
    GetParent()->GetScene()->AddEnvProbe(m_env_probe);

    for (auto &it : GetParent()->GetScene()->GetEntities()) {
        auto &entity = it.second;

        if (!entity) {
            continue;
        }

        if (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes &
            m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes) {

            m_renderer_instance->AddEntity(Handle<Entity>(it.second));
        }
    }
}

void CubemapRenderer::OnRemoved()
{
    AssertReady();

    AssertThrow(GetParent()->GetScene() != nullptr);
    AssertThrow(m_env_probe.IsValid());

    GetParent()->GetScene()->RemoveEnvProbe(m_env_probe->GetID());
}

void CubemapRenderer::OnEntityAdded(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes &
        m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes) {
        m_renderer_instance->AddEntity(Handle<Entity>(entity));
    }
}

void CubemapRenderer::OnEntityRemoved(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_renderer_instance->RemoveEntity(Handle<Entity>(entity));
}

void CubemapRenderer::OnEntityRenderableAttributesChanged(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes &
        m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes) {
        m_renderer_instance->AddEntity(Handle<Entity>(entity));
    } else {
        m_renderer_instance->RemoveEntity(Handle<Entity>(entity));
    }
}

void CubemapRenderer::OnUpdate( GameCounter::TickUnit delta)
{
    //m_env_probe->Update;
}

void CubemapRenderer::OnRender( Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

    m_framebuffers[frame_index]->BeginCapture(command_buffer);

    m_renderer_instance->GetPipeline()->push_constants = {
        .render_component_data = {
            .index = GetComponentIndex()
        }
    };

    Engine::Get()->render_state.BindScene(m_scene.Get());
    m_renderer_instance->Render(frame);
    Engine::Get()->render_state.UnbindScene();

    m_framebuffers[frame_index]->EndCapture(command_buffer);

    auto *framebuffer_image = m_framebuffers[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetAttachment()->GetImage();

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
    m_cubemaps[frame_index]->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);
    m_cubemaps[frame_index]->GetImage().Blit(command_buffer, framebuffer_image);

    if (m_filter_mode == FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP) {
        HYPERION_PASS_ERRORS(
            m_cubemaps[frame_index]->GetImage().GenerateMipmaps(Engine::Get()->GetDevice(), command_buffer),
            result
        );
    }

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    m_cubemaps[frame_index]->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

    HYPERION_ASSERT_RESULT(result);
}

void CubemapRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

void CubemapRenderer::CreateImagesAndBuffers()
{
    const auto origin = m_aabb.GetCenter();

    {
        for (UInt i = 0; i < 6; i++) {
            m_cubemap_uniforms.projection_matrices[i] = Matrix4::Perspective(
                90.0f,
                m_cubemap_dimensions.width, m_cubemap_dimensions.height,
                0.015f, m_aabb.GetExtent().Max()
            );

            m_cubemap_uniforms.view_matrices[i] = Matrix4::LookAt(
                origin,
                origin + cubemap_directions[i].first,
                cubemap_directions[i].second
            );
        }

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_cubemap_render_uniform_buffers[frame_index] = UniquePtr<UniformBuffer>::Construct();

            m_cubemaps[frame_index] = Engine::Get()->CreateHandle<Texture>(new TextureCube(
                m_cubemap_dimensions,
                InternalFormat::RGBA8_SRGB,
                m_filter_mode,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                nullptr
            ));

            Engine::Get()->InitObject(m_cubemaps[frame_index]);
        }
    }

    RenderCommands::Push<RENDER_COMMAND(CreateCubemapImages)>(*this);

    DebugLog(
        LogType::Debug,
        "Added cubemap uniform buffers\n"
    );
}

void CubemapRenderer::CreateRendererInstance()
{
    auto renderer_instance = std::make_unique<RendererInstance>(
        Handle<Shader>(m_shader),
        Handle<RenderPass>(m_render_pass),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
            },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST
                    | MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE
            }
        )
    );

    for (auto &framebuffer: m_framebuffers) {
        renderer_instance->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }

    m_renderer_instance = Engine::Get()->AddRendererInstance(std::move(renderer_instance));
    Engine::Get()->InitObject(m_renderer_instance);
}

void CubemapRenderer::CreateShader()
{
    m_shader = Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("CubemapRenderer"));
    Engine::Get()->InitObject(m_shader);
}

void CubemapRenderer::CreateRenderPass()
{
    m_render_pass = Engine::Get()->CreateHandle<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_cubemap_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        Engine::Get()->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_cubemap_dimensions,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        Engine::Get()->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    // attachment should be created in render thread?
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetInstance()->GetDevice()));
    }

    Engine::Get()->InitObject(m_render_pass);
}

void CubemapRenderer::CreateFramebuffers()
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = Engine::Get()->CreateHandle<Framebuffer>(
            m_cubemap_dimensions,
            Handle<RenderPass>(m_render_pass)
        );

        /* Add all attachments from the renderpass */
        for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
            m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        Engine::Get()->InitObject(m_framebuffers[i]);
    }
}

} // namespace hyperion::v2

#include "CubemapRenderer.hpp"

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <camera/PerspectiveCamera.hpp>


#include <rendering/Environment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

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
    Image::FilterMode filter_mode
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
    Image::FilterMode filter_mode
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

void CubemapRenderer::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](Engine *engine) {
        m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(nullptr));

        CreateShader(engine);
        CreateRenderPass(engine);
        CreateFramebuffers(engine);
        CreateImagesAndBuffers(engine);
        CreateGraphicsPipelines(engine);

        HYP_FLUSH_RENDER_QUEUE(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](Engine *engine) {
            // destroy descriptors

            for (UInt i = 0; i < max_frames_in_flight; i++) {
                if (m_framebuffers[i] != nullptr) {
                    for (auto &attachment : m_attachments) {
                        m_framebuffers[i]->RemoveAttachmentRef(attachment.get());
                    }

                    if (m_pipeline != nullptr) {
                        m_pipeline->RemoveFramebuffer(m_framebuffers[i]->GetId());
                    }
                }
            }

            if (m_render_pass != nullptr) {
                for (auto &attachment : m_attachments) {
                    m_render_pass->GetRenderPass().RemoveAttachmentRef(attachment.get());
                }
            }

            engine->render_scheduler.Enqueue([this, engine](...) {
                auto result = renderer::Result::OK;

                for (UInt i = 0; i < max_frames_in_flight; i++) {
                    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                        .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

                    descriptor_set->RemoveDescriptor(DescriptorKey::CUBEMAP_UNIFORMS);
                    descriptor_set->RemoveDescriptor(DescriptorKey::CUBEMAP_TEST);
                }

                for (auto &attachment : m_attachments) {
                    HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
                }

                m_attachments.clear();

                HYPERION_PASS_ERRORS(m_cubemap_render_uniform_buffer.Destroy(engine->GetDevice()), result);
                HYPERION_PASS_ERRORS(m_env_probe_uniform_buffer.Destroy(engine->GetDevice()), result);

                HYPERION_RETURN_OK;
            });


            m_framebuffers = {};
            m_cubemaps = {};
            m_shader.Reset();
            m_render_pass.Reset();
            m_pipeline.Reset();
            m_scene.Reset();

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

// called from game thread
void CubemapRenderer::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);

    for (auto &it : GetParent()->GetScene()->GetSpatials()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        if (entity->GetRenderableAttributes().vertex_attributes &
            m_pipeline->GetRenderableAttributes().vertex_attributes) {

            m_pipeline->AddSpatial(it.second.IncRef());
        }
    }
}

void CubemapRenderer::OnEntityAdded(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (spatial->GetRenderableAttributes().vertex_attributes &
        m_pipeline->GetRenderableAttributes().vertex_attributes) {
        m_pipeline->AddSpatial(spatial.IncRef());
    }
}

void CubemapRenderer::OnEntityRemoved(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_pipeline->RemoveSpatial(spatial.IncRef());
}

void CubemapRenderer::OnEntityRenderableAttributesChanged(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (spatial->GetRenderableAttributes().vertex_attributes &
        m_pipeline->GetRenderableAttributes().vertex_attributes) {
        m_pipeline->AddSpatial(spatial.IncRef());
    } else {
        m_pipeline->RemoveSpatial(spatial.IncRef());
    }
}

void CubemapRenderer::OnUpdate(Engine *engine, GameCounter::TickUnit delta)
{
    // Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    // Scene is owned by World, don't Update() it.
    // we need to make Camera be a component so we can use Camera ID
    // in bit flags for octree visibility.
    // m_scene->Update(engine, delta);
}

void CubemapRenderer::OnRender(Engine *engine, Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;

    m_framebuffers[frame_index]->BeginCapture(command_buffer);

    m_pipeline->GetPipeline()->push_constants = {
        .render_component_data = {
            .index = GetComponentIndex()
        }
    };

    engine->render_state.BindScene(m_scene);
    m_pipeline->Render(engine, frame);
    engine->render_state.UnbindScene();

    m_framebuffers[frame_index]->EndCapture(command_buffer);

    auto *framebuffer_image = m_framebuffers[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetAttachment()->GetImage();

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::COPY_SRC);
    m_cubemaps[frame_index]->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::COPY_DST);
    m_cubemaps[frame_index]->GetImage().Blit(command_buffer, framebuffer_image);

    if (m_filter_mode == Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP) {
        HYPERION_PASS_ERRORS(
            m_cubemaps[frame_index]->GetImage().GenerateMipmaps(engine->GetDevice(), command_buffer),
            result
        );
    }

    framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);
    m_cubemaps[frame_index]->GetImage().GetGPUImage()->InsertBarrier(command_buffer, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    HYPERION_ASSERT_RESULT(result);
}

void CubemapRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

void CubemapRenderer::CreateImagesAndBuffers(Engine *engine)
{
    const auto origin = m_aabb.GetCenter();

    CubemapUniforms cubemap_uniforms{};

    for (UInt i = 0; i < 6; i++) {
        cubemap_uniforms.projection_matrices[i] = Matrix4::Perspective(
            90.0f,
            m_cubemap_dimensions.width, m_cubemap_dimensions.height,
            0.015f, m_aabb.GetDimensions().Max()
        );

        cubemap_uniforms.view_matrices[i] = Matrix4::LookAt(
            origin,
            origin + cubemap_directions[i].first,
            cubemap_directions[i].second
        );
    }

    HYPERION_ASSERT_RESULT(m_cubemap_render_uniform_buffer.Create(engine->GetDevice(), sizeof(CubemapUniforms)));
    m_cubemap_render_uniform_buffer.Copy(engine->GetDevice(), sizeof(CubemapUniforms), &cubemap_uniforms);

    EnvProbeShaderData env_probe {
        .aabb_max       = m_aabb.max.ToVector4(),
        .aabb_min       = m_aabb.min.ToVector4(),
        .world_position = origin.ToVector4(),
        .texture_index  = GetComponentIndex()
    };

    HYPERION_ASSERT_RESULT(m_env_probe_uniform_buffer.Create(engine->GetDevice(), sizeof(EnvProbeShaderData)));
    m_env_probe_uniform_buffer.Copy(engine->GetDevice(), sizeof(EnvProbeShaderData), &env_probe);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_cubemaps[i] = engine->resources.textures.Add(std::make_unique<TextureCube>(
            m_cubemap_dimensions,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB,
            m_filter_mode,
            Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        m_cubemaps[i].Init();
    }

    // create descriptors
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::CUBEMAP_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = GetComponentIndex(),
                .buffer        = &m_cubemap_render_uniform_buffer
            });

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::CUBEMAP_TEST)
            ->SetSubDescriptor({
                .element_index = GetComponentIndex(),
                .image_view    = &m_cubemaps[i]->GetImageView()
            });

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::ENV_PROBES)
            ->SetSubDescriptor({
               .element_index = GetComponentIndex(),
               .buffer        = &m_env_probe_uniform_buffer
           });
    }

    DebugLog(
        LogType::Debug,
        "Added cubemap uniform buffers\n"
    );
}

void CubemapRenderer::CreateGraphicsPipelines(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        m_shader.IncRef(),
        m_render_pass.IncRef(),
        RenderableAttributeSet{
            .bucket            = BUCKET_INTERNAL,
            .vertex_attributes = renderer::static_mesh_vertex_attributes |
                                 renderer::skeleton_vertex_attributes
        }
    );

    pipeline->SetDepthWrite(true);
    pipeline->SetDepthTest(true);
    pipeline->SetFaceCullMode(FaceCullMode::BACK);
    pipeline->SetMultiviewIndex(0);

    for (auto &framebuffer: m_framebuffers) {
        pipeline->AddFramebuffer(framebuffer.IncRef());
    }

    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    m_pipeline.Init();
}

void CubemapRenderer::CreateShader(Engine *engine)
{
    std::vector<SubShader> sub_shaders = {
        {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/cubemap_renderer.vert.spv")).Read()}},
        {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/cubemap_renderer.frag.spv")).Read()}}
    };

    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(sub_shaders));
    m_shader->Init(engine);
}

void CubemapRenderer::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->resources.render_passes.Add(std::make_unique<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    ));

    AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_cubemap_dimensions,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB,
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_cubemap_dimensions,
            engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass.Init();
}

void CubemapRenderer::CreateFramebuffers(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
            m_cubemap_dimensions,
            m_render_pass.IncRef()
        ));

        /* Add all attachments from the renderpass */
        for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
            m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        m_framebuffers[i].Init();
    }
}

} // namespace hyperion::v2

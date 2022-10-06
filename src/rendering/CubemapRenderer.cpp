#include "CubemapRenderer.hpp"

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>
#include <camera/PerspectiveCamera.hpp>


#include <rendering/RenderEnvironment.hpp>
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

    CreateShader(engine);
    CreateRenderPass(engine);
    CreateFramebuffers(engine);
    CreateImagesAndBuffers(engine);
    CreateRendererInstance(engine);

    m_scene = engine->CreateHandle<Scene>(Handle<Camera>());
    engine->InitObject(m_scene);// testing global skybox

    /*engine->assets.Load<Texture>(
               "textures/chapel/posx.jpg",
               "textures/chapel/negx.jpg",
               "textures/chapel/posy.jpg",
               "textures/chapel/negy.jpg",
               "textures/chapel/posz.jpg",
               "textures/chapel/negz.jpg"
            );*/

    Sleep(50);

    m_env_probe = engine->resources->env_probes.Add(new EnvProbe(
        // TEMP
        //std::move(tex),

        Handle<Texture>(m_cubemaps[0]), // TODO
        m_aabb
    ));
    
    HYP_FLUSH_RENDER_QUEUE(engine);

    SetReady(true);

    OnTeardown([this]() {
        auto *engine = GetEngine();
#if 0
        // m_framebuffers = {};
        // m_render_pass.Reset();
        // m_renderer_instance.Reset();

        engine->GetRenderScheduler().Enqueue([this, engine](...) {
            for (UInt i = 0; i < max_frames_in_flight; i++) {
                if (m_framebuffers[i] != nullptr) {
                    for (auto &attachment : m_attachments) {
                        m_framebuffers[i]->RemoveAttachmentRef(attachment.get());
                    }

                    if (m_renderer_instance != nullptr) {
                        m_renderer_instance->RemoveFramebuffer(m_framebuffers[i]->GetID());
                    }
                }
            }

            if (m_render_pass != nullptr) {
                for (auto &attachment : m_attachments) {
                    m_render_pass->GetRenderPass().RemoveAttachmentRef(attachment.get());
                }
            }

            HYPERION_RETURN_OK;
        });

        engine->GetRenderScheduler().Enqueue([this, engine](...) {
            auto result = renderer::Result::OK;

            for (UInt i = 0; i < max_frames_in_flight; i++) {
                auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                    .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

                descriptor_set
                    ->GetDescriptor(DescriptorKey::CUBEMAP_UNIFORMS)
                    ->SetSubDescriptor({
                        .element_index = GetComponentIndex(),
                        .buffer = engine->GetPlaceholderData().GetOrCreateBuffer<UniformBuffer>(engine->GetDevice(), sizeof(CubemapUniforms))
                    });

                // HYPERION_PASS_ERRORS(
                //     m_cubemap_render_uniform_buffers[i]->Destroy(engine->GetDevice()),
                //     result
                // );
            }

            for (auto &attachment : m_attachments) {
                HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
            }

            m_attachments.clear();

            return result;
        });

        for (auto &cubemap : m_cubemaps) {
            engine->SafeReleaseHandle<Texture>(std::move(cubemap));
        }

        engine->SafeReleaseHandle<Shader>(std::move(m_shader));

        m_env_probe.Reset();
        m_scene.Reset();

        HYP_FLUSH_RENDER_QUEUE(engine);

        engine->GetRenderScheduler().Enqueue([this, engine, cubemap_render_uniform_buffers = FixedArray { std::move(m_cubemap_render_uniform_buffers[0]), std::move(m_cubemap_render_uniform_buffers[1]) }](...) mutable {
            auto result = renderer::Result::OK;

            for (UInt i = 0; i < max_frames_in_flight; i++) {
                HYPERION_PASS_ERRORS(
                    cubemap_render_uniform_buffers[i]->Destroy(engine->GetDevice()),
                    result
                );
            }

            return result;
        });
#endif

        SetReady(false);
    });
}

// called from game thread
void CubemapRenderer::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    
#if 0
    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);
    AssertThrow(m_env_probe != nullptr);

    GetParent()->AddEnvProbe(m_env_probe.IncRef());

    for (auto &it : GetParent()->GetScene()->GetEntities()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        if (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes &
            m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes) {

            m_renderer_instance->AddEntity(Handle<Entity>(it.second));
        }
    }
#endif
}

void CubemapRenderer::OnRemoved()
{
    AssertReady();

    AssertThrow(GetParent()->GetScene() != nullptr);
    AssertThrow(m_env_probe != nullptr);

    //GetParent()->RemoveEnvProbe(m_env_probe.IncRef());
}

void CubemapRenderer::OnEntityAdded(Handle<Entity> &entity)
{
#if 0
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes &
        m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes) {
        //m_renderer_instance->AddEntity(Handle<Entity>(entity));
    }
#endif
}

void CubemapRenderer::OnEntityRemoved(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    //m_renderer_instance->RemoveEntity(Handle<Entity>(entity));
}

void CubemapRenderer::OnEntityRenderableAttributesChanged(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

#if 0
    if (entity->GetRenderableAttributes().mesh_attributes.vertex_attributes &
        m_renderer_instance->GetRenderableAttributes().mesh_attributes.vertex_attributes) {
        m_renderer_instance->AddEntity(Handle<Entity>(entity));
    } else {
        m_renderer_instance->RemoveEntity(Handle<Entity>(entity));
    }
#endif
}

void CubemapRenderer::OnUpdate(Engine *engine, GameCounter::TickUnit delta)
{
    //m_env_probe->Update(engine);
}

void CubemapRenderer::OnRender(Engine *engine, Frame *frame)
{
    return;
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

    engine->render_state.BindScene(m_scene.Get());
    m_renderer_instance->Render(engine, frame);
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

    CubemapUniforms cubemap_uniforms { };

    for (UInt i = 0; i < 6; i++) {
        cubemap_uniforms.projection_matrices[i] = Matrix4::Perspective(
            90.0f,
            m_cubemap_dimensions.width, m_cubemap_dimensions.height,
            0.015f, m_aabb.GetExtent().Max()
        );

        cubemap_uniforms.view_matrices[i] = Matrix4::LookAt(
            origin,
            origin + cubemap_directions[i].first,
            cubemap_directions[i].second
        );
    }

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_cubemap_render_uniform_buffers[i] = std::make_unique<UniformBuffer>();
        HYPERION_ASSERT_RESULT(m_cubemap_render_uniform_buffers[i]->Create(engine->GetDevice(), sizeof(CubemapUniforms)));
        m_cubemap_render_uniform_buffers[i]->Copy(engine->GetDevice(), sizeof(CubemapUniforms), &cubemap_uniforms);
    }

    // EnvProbeShaderData env_probe {
    //     .aabb_max       = m_aabb.max.ToVector4(),
    //     .aabb_min       = m_aabb.min.ToVector4(),
    //     .world_position = origin.ToVector4(),
    //     .flags          = static_cast<UInt32>(ENV_PROBE_PARALLAX_CORRECTED)
    // };

    // HYPERION_ASSERT_RESULT(m_env_probe_uniform_buffer.Create(engine->GetDevice(), sizeof(EnvProbeShaderData)));
    // m_env_probe_uniform_buffer.Copy(engine->GetDevice(), sizeof(EnvProbeShaderData), &env_probe);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_cubemaps[i] = engine->CreateHandle<Texture>(new TextureCube(
            m_cubemap_dimensions,
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB,
            m_filter_mode,
            Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        engine->InitObject(m_cubemaps[i]);
    }

    // create descriptors
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::CUBEMAP_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = GetComponentIndex(),
                .buffer        = m_cubemap_render_uniform_buffers[i].get()
            });

        // descriptor_set
        //     ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::ENV_PROBES)
        //     ->SetSubDescriptor({
        //        .element_index = GetComponentIndex(),
        //        .buffer        = &m_env_probe_uniform_buffer
        //    });
    }

    DebugLog(
        LogType::Debug,
        "Added cubemap uniform buffers\n"
    );
}

void CubemapRenderer::CreateRendererInstance(Engine *engine)
{
    auto renderer_instance = std::make_unique<RendererInstance>(
        Handle<Shader>(m_shader),
        Handle<RenderPass>(m_render_pass),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes,
                .cull_faces = FaceCullMode::BACK
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

    m_renderer_instance = engine->AddRendererInstance(std::move(renderer_instance));
    engine->InitObject(m_renderer_instance);
}

void CubemapRenderer::CreateShader(Engine *engine)
{
    std::vector<SubShader> sub_shaders = {
        {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/cubemap_renderer.vert.spv")).Read()}},
        {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/cubemap_renderer.frag.spv")).Read()}}
    };

    m_shader = engine->CreateHandle<Shader>(sub_shaders);
    engine->InitObject(m_shader);
}

void CubemapRenderer::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->CreateHandle<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

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

    engine->InitObject(m_render_pass);
}

void CubemapRenderer::CreateFramebuffers(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->CreateHandle<Framebuffer>(
            m_cubemap_dimensions,
            Handle<RenderPass>(m_render_pass)
        );

        /* Add all attachments from the renderpass */
        for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
            m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        engine->InitObject(m_framebuffers[i]);
    }
}

} // namespace hyperion::v2

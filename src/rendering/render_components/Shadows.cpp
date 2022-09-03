#include "Shadows.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <camera/OrthoCamera.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::DescriptorKey;
using renderer::ImageSamplerDescriptor;

ShadowPass::ShadowPass()
    : FullScreenPass(),
      m_max_distance(100.0f),
      m_shadow_map_index(~0u),
      m_dimensions{ 1024, 1024 }
{
}

ShadowPass::~ShadowPass() = default;

void ShadowPass::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader> {
            SubShader { ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vert.spv")).Read()} },
            SubShader { ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/shadow_frag.spv")).Read()} }
        }
    ));

    m_shader.Init();
}

void ShadowPass::SetParentScene(Scene::ID id)
{
    m_parent_scene_id = id;

    if (m_scene != nullptr) {
        m_scene->SetParentId(m_parent_scene_id);
    }
}

void ShadowPass::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    AttachmentRef *attachment_ref;

    m_attachments.PushBack(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_dimensions,
            engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass = engine->resources.render_passes.Add(std::move(render_pass));
    m_render_pass.Init();
}

void ShadowPass::CreateDescriptors(Engine *engine)
{
    AssertThrow(m_shadow_map_index != ~0u);


    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();
    
        if (!framebuffer.GetAttachmentRefs().empty()) {
            /* TODO: Removal of these descriptors */

            //for (DescriptorSet::Index descriptor_set_index : DescriptorSet::scene_buffer_mapping) {
                auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                    .GetDescriptorSet(DescriptorSet::scene_buffer_mapping[i]);

                auto *shadow_map_descriptor = descriptor_set
                    ->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);
                
                for (const auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                    const auto sub_descriptor_index = shadow_map_descriptor->SetSubDescriptor({
                        .element_index = m_shadow_map_index,
                        .image_view    = attachment_ref->GetImageView(),
                        .sampler       = attachment_ref->GetSampler()
                    });

                    AssertThrow(sub_descriptor_index == m_shadow_map_index);
                }
            //}
        }
    }
}

void ShadowPass::CreateRendererInstance(Engine *engine)
{
    auto renderer_instance = std::make_unique<RendererInstance>(
        std::move(m_shader),
        m_render_pass.IncRef(),
        RenderableAttributeSet{
            .bucket            = BUCKET_SHADOW,
            .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
        }
    );

    renderer_instance->SetFaceCullMode(FaceCullMode::FRONT);
    renderer_instance->AddFramebuffer(m_framebuffers[0].IncRef());
    renderer_instance->AddFramebuffer(m_framebuffers[1].IncRef());
    
    m_renderer_instance = engine->AddRendererInstance(std::move(renderer_instance));
    m_renderer_instance.Init();
}

void ShadowPass::Create(Engine *engine)
{
    CreateShader(engine);
    CreateRenderPass(engine);

    m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(
        std::make_unique<OrthoCamera>(
            m_dimensions.width, m_dimensions.height,
            -100.0f, 100.0f,
            -100.0f, 100.0f,
            -100.0f, 100.0f
        )
    ));

    m_scene->SetParentId(m_parent_scene_id);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
            m_dimensions,
            m_render_pass.IncRef()
        ));

        /* Add all attachments from the renderpass */
        for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
            m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        m_framebuffers[i].Init();
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }

    CreateRendererInstance(engine);
    CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine); // force init stuff
}

void ShadowPass::Destroy(Engine *engine)
{
    engine->GetWorld().RemoveScene(m_scene->GetId());
    m_scene.Reset();

    FullScreenPass::Destroy(engine); // flushes render queue
}

void ShadowPass::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    engine->render_state.BindScene(m_scene);

    m_framebuffers[frame->GetFrameIndex()]->BeginCapture(frame->GetCommandBuffer());
    m_renderer_instance->Render(engine, frame);
    m_framebuffers[frame->GetFrameIndex()]->EndCapture(frame->GetCommandBuffer());

    engine->render_state.UnbindScene();
}

ShadowRenderer::ShadowRenderer(Ref<Light> &&light)
    : ShadowRenderer(std::move(light), Vector3::Zero(), 25.0f)
{
}

ShadowRenderer::ShadowRenderer(Ref<Light> &&light, const Vector3 &origin, float max_distance)
    : EngineComponentBase(),
      RenderComponent(5)
{
    m_shadow_pass.SetLight(std::move(light));
    m_shadow_pass.SetOrigin(origin);
    m_shadow_pass.SetMaxDistance(max_distance);
}

ShadowRenderer::~ShadowRenderer()
{
    Teardown();
}

// called from render thread
void ShadowRenderer::Init(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    AssertThrow(IsValidComponent());
    m_shadow_pass.SetShadowMapIndex(GetComponentIndex());

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](...) {
        auto *engine = GetEngine();

        m_shadow_pass.Create(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](...) {
            m_shadow_pass.Destroy(GetEngine()); // flushes render queue

            SetReady(false);
        }));
    }));
}

// called from game thread
void ShadowRenderer::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    engine->GetWorld().AddScene(m_shadow_pass.GetScene().IncRef());
    // TODO: RemoveScene()

    for (auto &it : GetParent()->GetScene()->GetEntities()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        if (BucketRendersShadows(entity->GetBucket())
            && (entity->GetRenderableAttributes().vertex_attributes & m_shadow_pass.GetRendererInstance()->GetRenderableAttributes().vertex_attributes)) {

            m_shadow_pass.GetRendererInstance()->AddEntity(it.second.IncRef());
        }
    }
}

void ShadowRenderer::OnEntityAdded(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (BucketRendersShadows(entity->GetBucket())
        && (entity->GetRenderableAttributes().vertex_attributes & m_shadow_pass.GetRendererInstance()->GetRenderableAttributes().vertex_attributes)) {
        m_shadow_pass.GetRendererInstance()->AddEntity(entity.IncRef());
    }
}

void ShadowRenderer::OnEntityRemoved(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_shadow_pass.GetRendererInstance()->RemoveEntity(entity.IncRef());
}

void ShadowRenderer::OnEntityRenderableAttributesChanged(Ref<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();
    
    if (BucketRendersShadows(entity->GetBucket())
        && (entity->GetRenderableAttributes().vertex_attributes & m_shadow_pass.GetRendererInstance()->GetRenderableAttributes().vertex_attributes)) {
        m_shadow_pass.GetRendererInstance()->AddEntity(entity.IncRef());
    } else {
        m_shadow_pass.GetRendererInstance()->RemoveEntity(entity.IncRef());
    }
}

void ShadowRenderer::OnUpdate(Engine *engine, GameCounter::TickUnit delta)
{
    // Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    
    UpdateSceneCamera(engine);
}

void ShadowRenderer::OnRender(Engine *engine, Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    const auto *camera = m_shadow_pass.GetScene()->GetCamera();
    
    engine->shader_globals->shadow_maps.Set(
        m_shadow_pass.GetShadowMapIndex(),
        {
            .projection  = camera->GetProjectionMatrix(),
            .view        = camera->GetViewMatrix(),
            .scene_index = m_shadow_pass.GetScene()->GetId().value - 1
        }
    );

    m_shadow_pass.Render(engine, frame);
}

void ShadowRenderer::UpdateSceneCamera(Engine *engine)
{
    const auto aabb   = m_shadow_pass.GetAABB();
    const auto center = aabb.GetCenter();

    const auto light_direction = m_shadow_pass.GetLight() != nullptr
        ? m_shadow_pass.GetLight()->GetPosition()
        : Vector3::Zero();

    auto *camera = m_shadow_pass.GetScene()->GetCamera();

    camera->SetTranslation(center + light_direction);
    camera->SetTarget(center);

    switch (camera->GetCameraType()) {
    case CameraType::ORTHOGRAPHIC: {
        auto corners = aabb.GetCorners();

        auto maxes = MathUtil::MinSafeValue<Vector3>(),
             mins  = MathUtil::MaxSafeValue<Vector3>();

        for (auto &corner : corners) {
            corner = camera->GetViewMatrix() * corner;

            maxes = MathUtil::Max(maxes, corner);
            mins  = MathUtil::Min(mins, corner);
        }

        static_cast<OrthoCamera *>(camera)->Set(  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            mins.x, maxes.x,
            mins.y, maxes.y,
            -m_shadow_pass.GetMaxDistance(),
            m_shadow_pass.GetMaxDistance()
        );

        break;
    }
    default:
        AssertThrowMsg(false, "Unhandled camera type");
    }
}

void ShadowRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

} // namespace hyperion::v2

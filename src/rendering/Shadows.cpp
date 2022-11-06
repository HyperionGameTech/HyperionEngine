#include "Shadows.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::DescriptorKey;
using renderer::StorageImageDescriptor;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::SamplerDescriptor;
using renderer::StorageImage2D;

ShadowPass::ShadowPass()
    : FullScreenPass(),
      m_shadow_mode(ShadowMode::VSM),
      m_max_distance(100.0f),
      m_shadow_map_index(~0u),
      m_dimensions { 1024, 1024 }
{
}

ShadowPass::~ShadowPass() = default;

void ShadowPass::CreateShader(Engine *engine)
{
    m_shader = engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("Shadows"));
    engine->InitObject(m_shader);
}

void ShadowPass::SetParentScene(Scene::ID id)
{
    m_parent_scene_id = id;

    if (m_scene != nullptr) {
        m_scene->SetParentID(m_parent_scene_id);
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

    { // depth, depth^2 texture (for variance shadow map)
        m_attachments.PushBack(std::make_unique<Attachment>(
            std::make_unique<renderer::FramebufferImage2D>(
                m_dimensions,
                InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F,
                FilterMode::TEXTURE_FILTER_NEAREST
            ),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentRef(
            engine->GetInstance()->GetDevice(),
            renderer::LoadOperation::UNDEFINED,
            renderer::StoreOperation::STORE,
            &attachment_ref
        ));

        render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);
    }

    { // standard depth texture
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
    }

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass = engine->CreateHandle<RenderPass>(render_pass.release());
    engine->InitObject(m_render_pass);
}

void ShadowPass::CreateDescriptors(Engine *engine)
{
    AssertThrow(m_shadow_map_index != ~0u);

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            /* TODO: Removal of these descriptors */
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::scene_buffer_mapping[i]);

            auto *shadow_map_descriptor = descriptor_set
                ->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);
        
            AssertThrow(m_shadow_map_image != nullptr);

            const auto sub_descriptor_index = shadow_map_descriptor->SetSubDescriptor({
                .element_index = m_shadow_map_index,
                .image_view = m_shadow_map_image_view.get(),
                .sampler = &engine->GetPlaceholderData().GetSamplerLinear()
            });

            AssertThrow(sub_descriptor_index == m_shadow_map_index);
        }

        HYPERION_RETURN_OK;
    });
}

void ShadowPass::CreateRendererInstance(Engine *engine)
{
    auto renderer_instance = std::make_unique<RendererInstance>(
        std::move(m_shader),
        Handle<RenderPass>(m_render_pass),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
            },
            MaterialAttributes {
                .bucket = BUCKET_SHADOW,
                .cull_faces = FaceCullMode::FRONT
            }
        )
    );

    for (auto &framebuffer : m_framebuffers) {
        renderer_instance->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }
    
    m_renderer_instance = engine->AddRendererInstance(std::move(renderer_instance));
    engine->InitObject(m_renderer_instance);
}

void ShadowPass::CreateShadowMap(Engine *engine)
{
    m_shadow_map_image = std::make_unique<StorageImage2D>(
        m_dimensions,
        InternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F
    );

    m_shadow_map_image_view = std::make_unique<ImageView>();

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        HYPERION_BUBBLE_ERRORS(m_shadow_map_image->Create(engine->GetDevice()));
        HYPERION_BUBBLE_ERRORS(m_shadow_map_image_view->Create(engine->GetDevice(), m_shadow_map_image.get()));

        HYPERION_RETURN_OK;
    });
}

void ShadowPass::CreateComputePipelines(Engine *engine)
{
    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_blur_descriptor_sets[i].AddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_render_pass->GetRenderPass().GetAttachmentRefs().front()->GetImageView()
            });

        m_blur_descriptor_sets[i].AddDescriptor<SamplerDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &engine->GetPlaceholderData().GetSamplerNearest()
            });

        m_blur_descriptor_sets[i].AddDescriptor<StorageImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_shadow_map_image_view.get()
            });
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_BUBBLE_ERRORS(m_blur_descriptor_sets[i].Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    });

    m_blur_shadow_map = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("BlurShadowMap")),
        Array<const DescriptorSet *> { &m_blur_descriptor_sets[0] }
    );

    engine->InitObject(m_blur_shadow_map);
}

void ShadowPass::Create(Engine *engine)
{
    CreateShadowMap(engine);
    CreateShader(engine);
    CreateRenderPass(engine);
    CreateDescriptors(engine);
    CreateComputePipelines(engine);

    m_scene = engine->CreateHandle<Scene>(
        engine->CreateHandle<Camera>(new OrthoCamera(
            m_dimensions.width, m_dimensions.height,
            -100.0f, 100.0f,
            -100.0f, 100.0f,
            -100.0f, 100.0f
        ))
    );

    m_scene->SetParentID(m_parent_scene_id);
    engine->InitObject(m_scene);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        { // init framebuffers
            m_framebuffers[i] = engine->CreateHandle<Framebuffer>(
                m_dimensions,
                Handle<RenderPass>(m_render_pass)
            );

            /* Add all attachments from the renderpass */
            for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
                m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
            }

            engine->InitObject(m_framebuffers[i]);
        }

        m_command_buffers[i] = UniquePtr<CommandBuffer>::Construct(CommandBuffer::COMMAND_BUFFER_SECONDARY);
    }

    CreateRendererInstance(engine);

    // create command buffers in render thread
    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        for (auto &command_buffer : m_command_buffers) {
            AssertThrow(command_buffer != nullptr);

            HYPERION_ASSERT_RESULT(command_buffer->Create(
                engine->GetInstance()->GetDevice(),
                engine->GetInstance()->GetGraphicsCommandPool()
            ));
        }

        HYPERION_RETURN_OK;
    });

    HYP_FLUSH_RENDER_QUEUE(engine); // force init stuff
}

void ShadowPass::Destroy(Engine *engine)
{
    if (m_scene) {
        engine->GetWorld()->RemoveScene(m_scene->GetID());
        m_scene.Reset();
    }

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        auto result = Result::OK;

        HYPERION_PASS_ERRORS(m_shadow_map_image->Destroy(engine->GetDevice()), result);
        HYPERION_PASS_ERRORS(m_shadow_map_image_view->Destroy(engine->GetDevice()), result);

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_PASS_ERRORS(m_blur_descriptor_sets[i].Destroy(engine->GetDevice()), result);
        }

        return result;
    });

    FullScreenPass::Destroy(engine); // flushes render queue
}

void ShadowPass::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *framebuffer_image = m_attachments.Front()->GetImage();

    if (framebuffer_image == nullptr) {
        return;
    }

    auto *command_buffer = frame->GetCommandBuffer();

    m_framebuffers[frame->GetFrameIndex()]->BeginCapture(command_buffer);

    engine->render_state.BindScene(m_scene.Get());
    m_renderer_instance->Render(engine, frame);
    engine->render_state.UnbindScene();

    m_framebuffers[frame->GetFrameIndex()]->EndCapture(command_buffer);

    if (m_shadow_mode == ShadowMode::VSM) {
        // blur the image using compute shader
        m_blur_shadow_map->GetPipeline()->Bind(
            command_buffer,
            Pipeline::PushConstantData {
                .blur_shadow_map_data = {
                    .image_dimensions = ShaderVec2<UInt32>(m_dimensions)
                }
            }
        );

        // bind descriptor set containing info needed to blur
        command_buffer->BindDescriptorSet(
            engine->GetInstance()->GetDescriptorPool(),
            m_blur_shadow_map->GetPipeline(),
            &m_blur_descriptor_sets[frame->GetFrameIndex()],
            DescriptorSet::Index(0)
        );

        // put our shadow map in a state for writing
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

        m_blur_shadow_map->GetPipeline()->Dispatch(
            command_buffer,
            Extent3D {
                (m_dimensions.width + 7) / 8,
                (m_dimensions.height + 7) / 8,
                1
            }
        );

        // put shadow map back into readable state
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    } else {
        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        m_shadow_map_image->Blit(
            command_buffer,
            framebuffer_image
        );

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    }
}

ShadowRenderer::ShadowRenderer(Handle<Light> &&light)
    : ShadowRenderer(std::move(light), Vector3::Zero(), 25.0f)
{
}

ShadowRenderer::ShadowRenderer(Handle<Light> &&light, const BoundingBox &aabb)
    : ShadowRenderer(std::move(light), aabb.GetCenter(), aabb.GetExtent().Max() * 0.5f)
{
}

ShadowRenderer::ShadowRenderer(Handle<Light> &&light, const Vector3 &origin, float max_distance)
    : EngineComponentBase(),
      RenderComponent()
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
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    AssertThrow(IsValidComponent());
    m_shadow_pass.SetShadowMapIndex(GetComponentIndex());
    m_shadow_pass.Create(engine);

    SetReady(true);

    OnTeardown([this]() {
        m_shadow_pass.Destroy(GetEngine()); // flushes render queue

        SetReady(false);
    });
}

// called from game thread
void ShadowRenderer::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    engine->GetWorld()->AddScene(Handle<Scene>(m_shadow_pass.GetScene()));

    for (auto &it : GetParent()->GetScene()->GetEntities()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        const auto &renderable_attributes = entity->GetRenderableAttributes();

        if (BucketRendersShadows(renderable_attributes.material_attributes.bucket)
            && (renderable_attributes.mesh_attributes.vertex_attributes
                & m_shadow_pass.GetRendererInstance()->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {

            m_shadow_pass.GetRendererInstance()->AddEntity(Handle<Entity>(it.second));
        }
    }
}

void ShadowRenderer::OnEntityAdded(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    const auto &renderable_attributes = entity->GetRenderableAttributes();

    if (BucketRendersShadows(renderable_attributes.material_attributes.bucket)
        && (renderable_attributes.mesh_attributes.vertex_attributes
            & m_shadow_pass.GetRendererInstance()->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {
        m_shadow_pass.GetRendererInstance()->AddEntity(Handle<Entity>(entity));
    }
}

void ShadowRenderer::OnEntityRemoved(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    m_shadow_pass.GetRendererInstance()->RemoveEntity(Handle<Entity>(entity));
}

void ShadowRenderer::OnEntityRenderableAttributesChanged(Handle<Entity> &entity)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();
    
    const auto &renderable_attributes = entity->GetRenderableAttributes();

    if (BucketRendersShadows(renderable_attributes.material_attributes.bucket)
        && (renderable_attributes.mesh_attributes.vertex_attributes
            & m_shadow_pass.GetRendererInstance()->GetRenderableAttributes().mesh_attributes.vertex_attributes)) {
        m_shadow_pass.GetRendererInstance()->AddEntity(Handle<Entity>(entity));
    } else {
        m_shadow_pass.GetRendererInstance()->RemoveEntity(Handle<Entity>(entity));
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

    const auto scene_index = m_shadow_pass.GetScene()->GetID().value - 1;

    if (const auto &camera = m_shadow_pass.GetScene()->GetCamera()) {
        engine->GetRenderData()->shadow_maps.Set(
            m_shadow_pass.GetShadowMapIndex(),
            {
                .projection = camera->GetDrawProxy().projection,
                .view = camera->GetDrawProxy().view,
                .scene_index = scene_index
            }
        );
    } else {
        engine->GetRenderData()->shadow_maps.Set(
            m_shadow_pass.GetShadowMapIndex(),
            {
                .projection = Matrix4::Identity(),
                .view = Matrix4::Identity(),
                .scene_index = scene_index
            }
        );
    }

    m_shadow_pass.Render(engine, frame);
}

void ShadowRenderer::UpdateSceneCamera(Engine *engine)
{
    // runs in game thread

    const auto aabb = m_shadow_pass.GetAABB();
    const auto center = aabb.GetCenter();

    const auto light_direction = m_shadow_pass.GetLight()
        ? m_shadow_pass.GetLight()->GetPosition() * -1.0f
        : Vector3::Zero();

    auto &camera = m_shadow_pass.GetScene()->GetCamera();

    if (!camera) {
        return;
    }

    camera->SetTranslation(center + light_direction);
    camera->SetTarget(center);

    switch (camera->GetCameraType()) {
    case CameraType::ORTHOGRAPHIC: {
        auto corners = aabb.GetCorners();

        auto maxes = MathUtil::MinSafeValue<Vector3>(),
             mins = MathUtil::MaxSafeValue<Vector3>();

        for (auto &corner : corners) {
            corner = camera->GetViewMatrix() * corner;

            maxes = MathUtil::Max(maxes, corner);
            mins = MathUtil::Min(mins, corner);
        }

        static_cast<OrthoCamera *>(camera.Get())->Set(  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            mins.x, maxes.x,
            mins.y, maxes.y,
            mins.z, maxes.z
            // -m_shadow_pass.GetMaxDistance() * 0.5f,
            // m_shadow_pass.GetMaxDistance() * 0.5f
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

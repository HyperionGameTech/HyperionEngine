#include "shadows.h"
#include "../engine.h"

#include <camera/ortho_camera.h>

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>

namespace hyperion::v2 {

using renderer::DescriptorKey;
using renderer::ImageSamplerDescriptor;

ShadowEffect::ShadowEffect()
    : FullScreenPass(),
      m_shadow_map_index(0)
{
}

ShadowEffect::~ShadowEffect() = default;

void ShadowEffect::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/vert.spv")).Read()} },
            SubShader{ ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/shadow_frag.spv")).Read()} }
        }
    ));

    m_shader.Init();
}

void ShadowEffect::SetParentScene(Scene::ID id)
{
    m_parent_scene_id = id;

    if (m_scene != nullptr) {
        m_scene->SetParentId(m_parent_scene_id);
    }
}

void ShadowEffect::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
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

    render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass = engine->resources.render_passes.Add(std::move(render_pass));
    m_render_pass.Init();
}

void ShadowEffect::CreateDescriptors(Engine *engine)
{
    /* set descriptor */
    engine->render_scheduler.Enqueue([this, engine, &framebuffer = m_framebuffer->GetFramebuffer()](...) {
        if (!framebuffer.GetAttachmentRefs().empty()) {
            /* TODO: Removal of these descriptors */

            for (DescriptorSet::Index descriptor_set_index : DescriptorSet::scene_buffer_mapping) {
                auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                    .GetDescriptorSet(descriptor_set_index);

                auto *shadow_map_descriptor = descriptor_set
                    ->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::SHADOW_MAPS);
                
                for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                    m_shadow_map_index = shadow_map_descriptor->AddSubDescriptor({
                        .image_view = attachment_ref->GetImageView(),
                        .sampler    = attachment_ref->GetSampler()
                    });
                }
            }
        }

        HYPERION_RETURN_OK;
    });
}

void ShadowEffect::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        m_render_pass.IncRef(),
        RenderableAttributeSet{
            .bucket            = BUCKET_PREPASS,
            .vertex_attributes = VertexAttributeSet::static_mesh | VertexAttributeSet::skeleton
        }
    );

    pipeline->SetFaceCullMode(FaceCullMode::FRONT);
    pipeline->AddFramebuffer(m_framebuffer.IncRef());
    
    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    m_pipeline.Init();

    const std::array<Bucket, 2> buckets = {BUCKET_OPAQUE, BUCKET_TRANSLUCENT};

    for (const Bucket bucket : buckets) {
        m_pipeline_observers.push_back(engine->GetRenderListContainer().Get(bucket).GetGraphicsPipelineNotifier().Add(Observer<Ref<GraphicsPipeline>>(
            [this](Ref<GraphicsPipeline> *pipelines, size_t count) {
                for (size_t i = 0; i < count; i++) {
                    if (pipelines[i] == m_pipeline.ptr) {
                        continue;
                    }

                    m_spatial_observers.Insert(pipelines[i]->GetId(), pipelines[i]->GetSpatialNotifier().Add(Observer<Ref<Spatial>>(
                        [this](Ref<Spatial> *items, size_t count) {
                            for (size_t i = 0; i < count; i++) {
                                m_pipeline->AddSpatial(items[i].IncRef());
                            }
                        },
                        [this](Ref<Spatial> *items, size_t count) {
                            for (size_t i = 0; i < count; i++) {
                                m_pipeline->RemoveSpatial(items[i]->GetId());
                            }
                        }
                    )));
                }
            },
            [this](Ref<GraphicsPipeline> *pipelines, size_t count) {
                for (size_t i = 0; i < count; i++) {
                    if (pipelines[i] == m_pipeline.ptr) {
                        continue;
                    }

                    m_spatial_observers.Erase(pipelines[i]->GetId());
                }
            }
        )));
    }
}

void ShadowEffect::Create(Engine *engine)
{
    CreateShader(engine);
    CreateRenderPass(engine);

    m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(
        std::make_unique<OrthoCamera>(
            2048, 2048,
            -100.0f, 100.0f,
            -100.0f, 100.0f,
            -100.0f, 100.0f
        )
    ));

    m_scene->SetParentId(m_parent_scene_id);
    m_scene.Init();
    
    m_framebuffer = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
        engine->GetInstance()->swapchain->extent,
        m_render_pass.IncRef()
    ));

    /* Add all attachments from the renderpass */
    for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
     //   attachment_ref->SetBinding(12);
        m_framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
    }


    m_framebuffer.Init();

    CreatePerFrameData(engine);
    CreatePipeline(engine);
    CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void ShadowEffect::Destroy(Engine *engine)
{
    m_spatial_observers.Clear();
    m_pipeline_observers.clear();

    FullScreenPass::Destroy(engine); // flushes render queue
}

void ShadowEffect::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    engine->render_state.BindScene(m_scene);

    m_framebuffer->BeginCapture(frame->GetCommandBuffer());
    m_pipeline->Render(engine, frame);
    m_framebuffer->EndCapture(frame->GetCommandBuffer());

    engine->render_state.UnbindScene();
}

ShadowRenderer::ShadowRenderer(Ref<Light> &&light)
    : ShadowRenderer(std::move(light), Vector3::Zero(), 25.0f)
{
}

ShadowRenderer::ShadowRenderer(Ref<Light> &&light, const Vector3 &origin, float max_distance)
    : EngineComponentBase()
{
    m_effect.SetLight(std::move(light));
    m_effect.SetOrigin(origin);
    m_effect.SetMaxDistance(max_distance);
}

ShadowRenderer::~ShadowRenderer()
{
    Teardown();
}

void ShadowRenderer::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](Engine *engine) {
        m_effect.Create(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](Engine *engine) {
            m_effect.Destroy(engine); // flushes render queue

            SetReady(false);
        }), engine);
    }));
}

void ShadowRenderer::Update(Engine *engine, GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    
    UpdateSceneCamera(engine);

    m_effect.GetScene()->Update(engine, delta);
}

void ShadowRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    const auto *camera = m_effect.GetScene()->GetCamera();
    
    engine->shader_globals->shadow_maps.Set(
        m_effect.GetShadowMapIndex(),
        {
            .projection  = camera->GetProjectionMatrix(),
            .view        = camera->GetViewMatrix(),
            .scene_index = m_effect.GetScene()->GetId().value - 1
        }
    );

    m_effect.Render(engine, frame);
}

void ShadowRenderer::UpdateSceneCamera(Engine *engine)
{
    const auto aabb = m_effect.GetAabb();
    const auto center = aabb.GetCenter();

    const auto light_direction = m_effect.GetLight() != nullptr
        ? m_effect.GetLight()->GetPosition()
        : Vector3::Zero();

    auto *camera = m_effect.GetScene()->GetCamera();

    camera->SetTranslation(center + light_direction);
    camera->SetTarget(center);

    switch (camera->GetCameraType()) {
    case CameraType::ORTHOGRAPHIC: {
        auto corners = aabb.GetCorners();

        auto maxes = MathUtil::MinSafeValue<Vector3>(),
             mins  = MathUtil::MaxSafeValue<Vector3>();

        for (auto &corner : corners) {
            corner *= camera->GetViewMatrix();

            maxes = MathUtil::Max(maxes, corner);
            mins  = MathUtil::Min(mins, corner);
        }

        static_cast<OrthoCamera *>(camera)->Set(  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            mins.x, maxes.x,
            mins.y, maxes.y,
            -m_effect.GetMaxDistance(),
            m_effect.GetMaxDistance()
        );

        break;
    }
    default:
        AssertThrowMsg(false, "Unhandled camera type");
    }
}

} // namespace hyperion::v2

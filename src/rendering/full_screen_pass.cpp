#include "./full_screen_pass.h"
#include <engine.h>

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>
#include <builders/mesh_builder.h>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::ImageSamplerDescriptor;
using renderer::FillMode;

std::unique_ptr<Mesh> FullScreenPass::full_screen_quad = MeshBuilder::Quad();

FullScreenPass::FullScreenPass()
    : FullScreenPass(nullptr)
{
}

FullScreenPass::FullScreenPass(Ref<Shader> &&shader)
    : FullScreenPass(std::move(shader), DescriptorKey::POST_FX_PRE_STACK, ~0)
{
}

FullScreenPass::FullScreenPass(Ref<Shader> &&shader, DescriptorKey descriptor_key, uint sub_descriptor_index)
    : m_shader(std::move(shader)),
      m_descriptor_key(descriptor_key),
      m_sub_descriptor_index(sub_descriptor_index)
{
}

FullScreenPass::~FullScreenPass() = default;

void FullScreenPass::Create(Engine *engine)
{
    /* will only init once */
    full_screen_quad->Init(engine);

    CreateRenderPass(engine);

    m_framebuffer = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
        engine->GetInstance()->swapchain->extent,
        m_render_pass.IncRef()
    ));

    /* Add all attachments from the renderpass */
    for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
        m_framebuffer->GetFramebuffer().AddAttachmentRef(attachment_ref);
    }
    
    m_framebuffer.Init();

    CreatePerFrameData(engine);
    CreatePipeline(engine);
    CreateDescriptors(engine);
    
    HYP_FLUSH_RENDER_QUEUE(engine);
}

void FullScreenPass::SetShader(Ref<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = std::move(shader);

    m_shader.Init();
}

void FullScreenPass::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    renderer::AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
            engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_COLOR),
            nullptr
        ),
        renderer::RenderPassStage::SHADER
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

void FullScreenPass::CreatePerFrameData(Engine *engine)
{
    Engine::AssertOnThread(THREAD_RENDER);

    const uint32_t num_frames = engine->GetInstance()->GetFrameHandler()->NumFrames();

    m_frame_data = std::make_unique<PerFrameData<CommandBuffer>>(num_frames);

    //engine->render_scheduler.Enqueue([this, engine, num_frames](...) {
        for (uint32_t i = 0; i < num_frames; i++) {
            auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

            HYPERION_ASSERT_RESULT(command_buffer->Create(
                engine->GetInstance()->GetDevice(),
                engine->GetInstance()->GetGraphicsCommandPool()
            ));

            m_frame_data->At(i).Set<CommandBuffer>(std::move(command_buffer));
        }

    //    HYPERION_RETURN_OK;
    //});
}

void FullScreenPass::CreateDescriptors(Engine *engine)
{
    Engine::AssertOnThread(THREAD_RENDER);

    auto &framebuffer = m_framebuffer->GetFramebuffer();

    //engine->render_scheduler.Enqueue([this, engine, &framebuffer = m_framebuffer->GetFramebuffer()](...) {
        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(m_descriptor_key);

            AssertThrowMsg(framebuffer.GetAttachmentRefs().size() == 1, "> 1 attachments not supported currently for full screen passes");

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                m_sub_descriptor_index = descriptor->AddSubDescriptor({
                    .element_index = m_sub_descriptor_index,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler()
                });
            }
        }

    //    HYPERION_RETURN_OK;
    //});
}

void FullScreenPass::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        m_render_pass.IncRef(),
        RenderableAttributeSet{
            .bucket            = BUCKET_PREPASS,
            .vertex_attributes = VertexAttributeSet::static_mesh,
            .fill_mode         = FillMode::FILL
        }
    );

    pipeline->AddFramebuffer(m_framebuffer.IncRef());
    pipeline->SetDepthWrite(false);
    pipeline->SetDepthTest(false);

    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    m_pipeline.Init();
}

void FullScreenPass::Destroy(Engine *engine)
{
    AssertThrow(m_frame_data != nullptr);

    if (m_framebuffer != nullptr) {
        for (auto &attachment : m_attachments) {
            m_framebuffer->RemoveAttachmentRef(attachment.get());
        }

        if (m_pipeline != nullptr) {
            m_pipeline->RemoveFramebuffer(m_framebuffer->GetId());
        }
    }

    if (m_render_pass != nullptr) {
        for (auto &attachment : m_attachments) {
            m_render_pass->GetRenderPass().RemoveAttachmentRef(attachment.get());
        }
    }

    m_render_pass = nullptr;
    m_framebuffer = nullptr;
    m_pipeline    = nullptr;

    engine->render_scheduler.Enqueue([this, engine, &frame_data = *m_frame_data](...) {
        auto result = renderer::Result::OK;

        for (uint32_t i = 0; i < frame_data.NumFrames(); i++) {
            HYPERION_PASS_ERRORS(
                frame_data[i].Get<CommandBuffer>()->Destroy(
                    engine->GetInstance()->GetDevice(),
                    engine->GetInstance()->GetGraphicsCommandPool()
                ),
                result
            );
        }

        m_frame_data->Reset();

        for (auto &attachment : m_attachments) {
            HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
        }

        m_attachments.clear();

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void FullScreenPass::Record(Engine *engine, uint32_t frame_index)
{
    Engine::AssertOnThread(THREAD_RENDER);

    using renderer::Result;

    auto result = Result::OK;

    auto *command_buffer = m_frame_data->At(frame_index).Get<CommandBuffer>();

    auto record_result = command_buffer->Record(
            engine->GetInstance()->GetDevice(),
            m_pipeline->GetPipeline()->GetConstructionInfo().render_pass,
            [this, engine, frame_index](CommandBuffer *cmd) {
                m_pipeline->GetPipeline()->Bind(cmd);
                
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    m_pipeline->GetPipeline(),
                    {
                        {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
                    }
                ));
                
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    m_pipeline->GetPipeline(),
                    {
                        {.set = DescriptorSet::scene_buffer_mapping[frame_index], .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                        {.offsets = {uint32_t(0 * sizeof(SceneShaderData))}} /* TODO: scene index */
                    }
                ));
                
#if HYP_FEATURES_BINDLESS_TEXTURES
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    m_pipeline->GetPipeline(),
                    {
                        {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                    }
                ));
#else
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    m_pipeline->GetPipeline(),
                    {
                        {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES}
                    }
                ));
#endif

                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    m_pipeline->GetPipeline(),
                    {
                        {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1}
                    }
                ));

                full_screen_quad->Render(engine, cmd);

                HYPERION_RETURN_OK;
            });

    HYPERION_ASSERT_RESULT(record_result);
}

void FullScreenPass::Render(Engine * engine, Frame *frame)
{
    Engine::AssertOnThread(THREAD_RENDER);

    m_framebuffer->BeginCapture(frame->GetCommandBuffer());

    auto *secondary_command_buffer = m_frame_data->At(frame->GetFrameIndex()).Get<CommandBuffer>();

    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffer->EndCapture(frame->GetCommandBuffer());
}

} // namespace hyperion::v2

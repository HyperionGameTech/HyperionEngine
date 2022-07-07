#include "./FullScreenPass.hpp"
#include <Engine.hpp>
#include <Types.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>
#include <builders/MeshBuilder.hpp>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::ImageSamplerDescriptor;
using renderer::FillMode;

std::unique_ptr<Mesh> FullScreenPass::full_screen_quad = MeshBuilder::Quad();

FullScreenPass::FullScreenPass(Image::InternalFormat image_format)
    : FullScreenPass(nullptr, image_format)
{
}

FullScreenPass::FullScreenPass(Ref<Shader> &&shader, Image::InternalFormat image_format)
    : FullScreenPass(std::move(shader), DescriptorKey::POST_FX_PRE_STACK, ~0u, image_format)
{
}

FullScreenPass::FullScreenPass(
    Ref<Shader> &&shader,
    DescriptorKey descriptor_key,
    UInt sub_descriptor_index,
    Image::InternalFormat image_format
) : m_shader(std::move(shader)),
    m_descriptor_key(descriptor_key),
    m_sub_descriptor_index(sub_descriptor_index),
    m_image_format(image_format)
{
}

FullScreenPass::~FullScreenPass() = default;

void FullScreenPass::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    /* will only init once */
    full_screen_quad->Init(engine);

    CreateRenderPass(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
            engine->GetInstance()->swapchain->extent,
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

    auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
        engine->GetInstance()->swapchain->extent,
        m_image_format,
        nullptr
    );

    m_attachments.push_back(std::make_unique<Attachment>(
        std::move(framebuffer_image),
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

void FullScreenPass::CreateDescriptors(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();
    
        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(m_descriptor_key);

            AssertThrowMsg(framebuffer.GetAttachmentRefs().size() == 1, "> 1 attachments not supported currently for full screen passes");

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                m_sub_descriptor_index = descriptor->SetSubDescriptor({
                    .element_index = m_sub_descriptor_index,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler()
                });
            }
        }
    }
}

void FullScreenPass::CreatePipeline(Engine *engine)
{
    CreatePipeline(engine, RenderableAttributeSet {
        .bucket            = BUCKET_INTERNAL,
        .vertex_attributes = renderer::static_mesh_vertex_attributes,
        .fill_mode         = FillMode::FILL,
        .depth_write       = false,
        .depth_test        = false
    });
}

void FullScreenPass::CreatePipeline(Engine *engine, const RenderableAttributeSet &renderable_attributes)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        m_render_pass.IncRef(),
        renderable_attributes
    );

    for (auto &framebuffer : m_framebuffers) {
        pipeline->AddFramebuffer(framebuffer.IncRef());
    }

    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    m_pipeline.Init();
}

void FullScreenPass::Destroy(Engine *engine)
{
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

    m_framebuffers = {};
    m_render_pass.Reset();
    m_pipeline.Reset();

    engine->render_scheduler.Enqueue([this, engine](...) {
        auto result = renderer::Result::OK;

        for (UInt32 i = 0; i < max_frames_in_flight; i++) {
            HYPERION_PASS_ERRORS(
                m_command_buffers[i]->Destroy(
                    engine->GetInstance()->GetDevice(),
                    engine->GetInstance()->GetGraphicsCommandPool()
                ),
                result
            );
        }

        m_command_buffers = {};

        for (auto &attachment : m_attachments) {
            HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
        }

        m_attachments.clear();

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void FullScreenPass::Record(Engine *engine, UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    using renderer::Result;

    auto *command_buffer = m_command_buffers[frame_index].get();

    auto record_result = command_buffer->Record(
        engine->GetInstance()->GetDevice(),
        m_pipeline->GetPipeline()->GetConstructionInfo().render_pass,
        [this, engine, frame_index](CommandBuffer *cmd) {
            m_pipeline->GetPipeline()->push_constants = m_push_constant_data;
            m_pipeline->GetPipeline()->Bind(cmd);

            const auto scene_binding = engine->render_state.GetScene();
            const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );

            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::scene_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                FixedArray {
                    UInt32(sizeof(SceneShaderData) * scene_index),
                    UInt32(sizeof(LightShaderData) * 0)
                }
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_pipeline->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER
            );

            full_screen_quad->Render(engine, cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void FullScreenPass::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const auto frame_index = frame->GetFrameIndex();

    m_framebuffers[frame_index]->BeginCapture(frame->GetCommandBuffer());

    auto *secondary_command_buffer = m_command_buffers[frame_index].get();
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffers[frame_index]->EndCapture(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
